use std::env;
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use clap::{Args, Subcommand};
use serde_json::Value;

#[derive(Subcommand)]
pub(crate) enum PythonTask {
    /// Build a repaired KuPy wheel for an explicit CPython interpreter.
    Build(BuildArgs),
    /// Build, repair, and install KuPy into an existing virtual environment.
    Install(InstallArgs),
}

#[derive(Args)]
pub(crate) struct BuildArgs {
    /// CPython interpreter to use (relative to the caller if not absolute).
    #[arg(long, value_name = "PATH")]
    python: PathBuf,
}

#[derive(Args)]
pub(crate) struct InstallArgs {
    /// Existing virtual environment to install into (relative to the caller if not absolute).
    #[arg(long, value_name = "PATH")]
    venv: PathBuf,
}

impl PythonTask {
    pub(crate) fn action(&self) -> &'static str {
        match self {
            Self::Build(_) => "build",
            Self::Install(_) => "install",
        }
    }
}

pub(crate) struct ResultPath {
    pub(crate) wheel: PathBuf,
    pub(crate) destination: Option<PathBuf>,
}

#[derive(Clone, Debug, Default)]
struct RuntimeEnvironment {
    build_mode: Option<String>,
    preset: Option<String>,
}

impl RuntimeEnvironment {
    fn current() -> Self {
        Self {
            build_mode: env::var("KUAI_RUNTIME_BUILD_MODE").ok(),
            preset: env::var("KUAI_RUNTIME_PRESET").ok(),
        }
    }

    fn validate(&self) -> Result<(), String> {
        if self
            .build_mode
            .as_deref()
            .is_some_and(|mode| mode != "local")
        {
            return Err("KUAI_RUNTIME_BUILD_MODE must be unset or local".into());
        }
        if self
            .preset
            .as_deref()
            .is_some_and(|preset| preset != "release-cpu")
        {
            return Err("KUAI_RUNTIME_PRESET must be unset or release-cpu".into());
        }
        for key in [
            "CARGO_BUILD_TARGET",
            "PYO3_CROSS",
            "PYO3_CROSS_LIB_DIR",
            "PYO3_CROSS_PYTHON_VERSION",
        ] {
            if env::var_os(key).is_some() {
                return Err(format!(
                    "cross-target setting {key} is unsupported for Python wheels"
                ));
            }
        }
        Ok(())
    }
}

#[derive(Clone)]
struct ChildEnvironment {
    root: PathBuf,
    python: PathBuf,
}

impl ChildEnvironment {
    fn apply(&self, command: &mut Command) {
        for key in [
            "PYO3_PYTHON",
            "PYO3_CONFIG_FILE",
            "PYO3_CROSS",
            "PYO3_CROSS_LIB_DIR",
            "PYO3_CROSS_PYTHON_VERSION",
        ] {
            command.env_remove(key);
        }
        command
            .current_dir(&self.root)
            .env("PYO3_PYTHON", &self.python)
            .env("KUAI_RUNTIME_BUILD_MODE", "local")
            .env("KUAI_RUNTIME_PRESET", "release-cpu");
    }
}

struct Pins {
    maturin: String,
    delocate: String,
}

fn parse_pins(contents: &str) -> Result<Pins, String> {
    let document: toml::Value = contents
        .parse()
        .map_err(|error| format!("invalid pyproject.toml: {error}"))?;
    let requires = document
        .get("build-system")
        .and_then(|value| value.get("requires"))
        .and_then(toml::Value::as_array)
        .ok_or("pyproject.toml build-system.requires must be an array")?;
    let maturin = requires
        .iter()
        .filter_map(toml::Value::as_str)
        .find_map(|requirement| requirement.strip_prefix("maturin=="))
        .filter(|version| !version.is_empty())
        .ok_or("pyproject.toml build-system.requires must pin maturin with maturin==VERSION")?;
    let delocate = document
        .get("tool")
        .and_then(|value| value.get("kurt-wheel"))
        .and_then(|value| value.get("delocate"))
        .and_then(toml::Value::as_str)
        .filter(|version| !version.is_empty())
        .ok_or("pyproject.toml tool.kurt-wheel.delocate must be a non-empty string")?;
    Ok(Pins {
        maturin: maturin.into(),
        delocate: delocate.into(),
    })
}

fn caller_relative(caller: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.into()
    } else {
        caller.join(path)
    }
}

fn only_wheel(directory: &Path) -> Result<PathBuf, String> {
    let mut wheels = fs::read_dir(directory)
        .map_err(|error| {
            format!(
                "cannot read wheel directory {}: {error}",
                directory.display()
            )
        })?
        .map(|entry| entry.map(|entry| entry.path()))
        .collect::<Result<Vec<_>, _>>()
        .map_err(|error| {
            format!(
                "cannot inspect wheel directory {}: {error}",
                directory.display()
            )
        })?
        .into_iter()
        .filter(|path| path.extension().is_some_and(|extension| extension == "whl"))
        .collect::<Vec<_>>();
    wheels.sort();
    match wheels.as_slice() {
        [wheel] => Ok(wheel.clone()),
        _ => Err(format!(
            "expected exactly one wheel in {}, found {}",
            directory.display(),
            wheels.len()
        )),
    }
}

trait Runner {
    fn run(
        &mut self,
        program: &Path,
        args: &[OsString],
        environment: &ChildEnvironment,
    ) -> Result<(), String>;
}

struct ProcessRunner;

impl Runner for ProcessRunner {
    fn run(
        &mut self,
        program: &Path,
        args: &[OsString],
        environment: &ChildEnvironment,
    ) -> Result<(), String> {
        let mut command = Command::new(program);
        command
            .args(args)
            .stdin(Stdio::null())
            .stderr(Stdio::inherit());
        environment.apply(&mut command);
        match command.status() {
            Ok(status) if status.success() => Ok(()),
            Ok(status) => Err(format!("{} exited with {status}", program.display())),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Err(format!(
                "{} is required; install uv: https://docs.astral.sh/uv/getting-started/installation/ ({error})",
                program.display()
            )),
            Err(error) => Err(format!("cannot launch {}: {error}", program.display())),
        }
    }
}

fn run_build_and_repair<R: Runner>(
    runner: &mut R,
    pins: &Pins,
    root: &Path,
    python: &Path,
    target: &str,
    raw: &Path,
    repaired: &Path,
) -> Result<PathBuf, String> {
    let environment = ChildEnvironment {
        root: root.into(),
        python: python.into(),
    };
    let manifest = root.join("kurt-python/Cargo.toml");
    let build = vec![
        "--from".into(),
        format!("maturin=={}", pins.maturin).into(),
        "maturin".into(),
        "build".into(),
        "--manifest-path".into(),
        manifest.into_os_string(),
        "--release".into(),
        "--locked".into(),
        "--interpreter".into(),
        python.as_os_str().to_owned(),
        // This keeps Cargo configuration from redirecting a native wheel build.
        "--target".into(),
        target.into(),
        "--out".into(),
        raw.as_os_str().to_owned(),
    ];
    runner.run(Path::new("uvx"), &build, &environment)?;
    let raw_wheel = only_wheel(raw)?;
    let repair = vec![
        "--from".into(),
        format!("delocate=={}", pins.delocate).into(),
        "python".into(),
        root.join("kurt-python/scripts/repair_wheel.py")
            .into_os_string(),
        raw_wheel.into_os_string(),
        repaired.as_os_str().to_owned(),
    ];
    runner.run(Path::new("uvx"), &repair, &environment)?;
    only_wheel(repaired)
}

#[derive(Debug)]
struct PythonProbe {
    prefix: PathBuf,
    base_prefix: PathBuf,
    implementation: String,
    major: u64,
    minor: u64,
    gil_disabled: bool,
    machine: String,
    platform: String,
}

fn probe_python(python: &Path) -> Result<PythonProbe, String> {
    if !python.is_file() {
        return Err(format!(
            "Python interpreter is not a regular file: {}",
            python.display()
        ));
    }
    const SCRIPT: &str = "import json, platform, sys, sysconfig; print(json.dumps({'prefix':sys.prefix,'base_prefix':sys.base_prefix,'implementation':sys.implementation.name,'version':list(sys.version_info[:2]),'gil_disabled':bool(sysconfig.get_config_var('Py_GIL_DISABLED')),'machine':platform.machine(),'platform':sys.platform}))";
    let output = Command::new(python)
        .args(["-I", "-c", SCRIPT])
        .stdin(Stdio::null())
        .stderr(Stdio::inherit())
        .output()
        .map_err(|error| {
            format!(
                "cannot launch Python interpreter {}: {error}",
                python.display()
            )
        })?;
    if !output.status.success() {
        return Err(format!(
            "Python interpreter probe failed for {}: {}",
            python.display(),
            output.status
        ));
    }
    let value: Value = serde_json::from_slice(&output.stdout)
        .map_err(|error| format!("Python interpreter probe returned invalid JSON: {error}"))?;
    let string = |key: &str| {
        value
            .get(key)
            .and_then(Value::as_str)
            .map(str::to_owned)
            .ok_or_else(|| format!("Python interpreter probe omitted {key}"))
    };
    let version = value
        .get("version")
        .and_then(Value::as_array)
        .ok_or("Python interpreter probe omitted version")?;
    let number = |index: usize| {
        version
            .get(index)
            .and_then(Value::as_u64)
            .ok_or("Python interpreter probe has invalid version")
    };
    Ok(PythonProbe {
        prefix: PathBuf::from(string("prefix")?),
        base_prefix: PathBuf::from(string("base_prefix")?),
        implementation: string("implementation")?,
        major: number(0)?,
        minor: number(1)?,
        gil_disabled: value
            .get("gil_disabled")
            .and_then(Value::as_bool)
            .ok_or("Python interpreter probe omitted gil_disabled")?,
        machine: string("machine")?,
        platform: string("platform")?,
    })
}

fn native_rust_target() -> Result<&'static str, String> {
    match env::consts::ARCH {
        "aarch64" => Ok("aarch64-apple-darwin"),
        "x86_64" => Ok("x86_64-apple-darwin"),
        arch => Err(format!(
            "initial Python wheels do not support host architecture {arch}"
        )),
    }
}

fn host_machine_matches(machine: &str) -> bool {
    matches!(
        (env::consts::ARCH, machine),
        ("aarch64", "arm64" | "aarch64") | ("x86_64", "x86_64" | "amd64")
    )
}

fn validate_probe(probe: &PythonProbe) -> Result<&'static str, String> {
    if probe.implementation != "cpython" || probe.major != 3 || probe.minor < 10 {
        return Err("initial Python wheel support requires CPython 3.10 or newer".into());
    }
    if probe.gil_disabled {
        return Err("free-threaded Python is not supported for initial wheels; use conventional GIL CPython".into());
    }
    if probe.platform != "darwin" || !host_machine_matches(&probe.machine) {
        return Err(format!(
            "initial Python wheels require native macOS host architecture (got {} on {})",
            probe.machine, probe.platform
        ));
    }
    native_rust_target()
}

fn validate_python(python: &Path) -> Result<(PythonProbe, &'static str), String> {
    let probe = probe_python(python)?;
    let target = validate_probe(&probe)?;
    Ok((probe, target))
}

fn validate_venv_with_probe(
    venv: &Path,
    get_probe: impl FnOnce(&Path) -> Result<PythonProbe, String>,
) -> Result<(PathBuf, &'static str), String> {
    let requested = fs::canonicalize(venv).map_err(|error| {
        format!(
            "cannot resolve virtual environment {}: {error}",
            venv.display()
        )
    })?;
    if !requested.is_dir() {
        return Err(format!(
            "virtual environment is not a directory: {}",
            requested.display()
        ));
    }
    let python = requested.join("bin/python");
    let probe = get_probe(&python)?;
    let target = validate_probe(&probe)?;
    if probe.prefix == probe.base_prefix {
        return Err(format!(
            "{} is not a virtual environment (sys.prefix equals sys.base_prefix)",
            requested.display()
        ));
    }
    let prefix = fs::canonicalize(&probe.prefix).map_err(|error| {
        format!(
            "cannot resolve Python sys.prefix {}: {error}",
            probe.prefix.display()
        )
    })?;
    if prefix != requested {
        return Err(format!(
            "virtual environment Python belongs to {}, not {}",
            prefix.display(),
            requested.display()
        ));
    }
    Ok((python, target))
}

fn validate_venv(venv: &Path) -> Result<(PathBuf, &'static str), String> {
    validate_venv_with_probe(venv, probe_python)
}

fn target_directory(root: &Path) -> Result<PathBuf, String> {
    let output = Command::new("cargo")
        .args(["metadata", "--no-deps", "--format-version", "1", "--locked"])
        .current_dir(root)
        .stderr(Stdio::inherit())
        .output()
        .map_err(|error| format!("cannot launch cargo metadata: {error}"))?;
    if !output.status.success() {
        return Err(format!("cargo metadata failed: {}", output.status));
    }
    let metadata: Value = serde_json::from_slice(&output.stdout)
        .map_err(|error| format!("cargo metadata returned invalid JSON: {error}"))?;
    metadata
        .get("target_directory")
        .and_then(Value::as_str)
        .map(PathBuf::from)
        .ok_or("cargo metadata omitted target_directory".into())
}

fn create_stage(wheels: &Path) -> Result<(tempfile::TempDir, PathBuf, PathBuf), String> {
    fs::create_dir_all(wheels).map_err(|error| {
        format!(
            "cannot create wheel directory {}: {error}",
            wheels.display()
        )
    })?;
    let stage = tempfile::Builder::new()
        .prefix("python-stage-")
        .tempdir_in(wheels)
        .map_err(|error| {
            format!(
                "cannot create wheel staging directory in {}: {error}",
                wheels.display()
            )
        })?;
    let raw = stage.path().join("raw");
    let repaired = stage.path().join("repaired");
    fs::create_dir(&raw).map_err(|error| format!("cannot create raw wheel staging: {error}"))?;
    fs::create_dir(&repaired)
        .map_err(|error| format!("cannot create repaired wheel staging: {error}"))?;
    Ok((stage, raw, repaired))
}

fn install(
    runner: &mut impl Runner,
    root: &Path,
    python: &Path,
    wheel: &Path,
) -> Result<(), String> {
    let args = vec![
        "pip".into(),
        "install".into(),
        "--python".into(),
        python.as_os_str().to_owned(),
        "--reinstall-package".into(),
        "kupy".into(),
        wheel.as_os_str().to_owned(),
    ];
    runner.run(
        Path::new("uv"),
        &args,
        &ChildEnvironment {
            root: root.into(),
            python: python.into(),
        },
    )
}

struct Pipeline<'a> {
    pins: &'a Pins,
    root: &'a Path,
    python: &'a Path,
    target: &'a str,
    raw: &'a Path,
    repaired: &'a Path,
}

fn run_pipeline<R: Runner>(
    runner: &mut R,
    pipeline: &Pipeline<'_>,
    should_install: bool,
) -> Result<PathBuf, String> {
    let wheel = run_build_and_repair(
        runner,
        pipeline.pins,
        pipeline.root,
        pipeline.python,
        pipeline.target,
        pipeline.raw,
        pipeline.repaired,
    )?;
    if should_install {
        install(runner, pipeline.root, pipeline.python, &wheel)?;
    }
    Ok(wheel)
}

pub(crate) fn run(root: &Path, command: &PythonTask) -> Result<ResultPath, String> {
    RuntimeEnvironment::current().validate()?;
    let caller = env::current_dir()
        .map_err(|error| format!("cannot determine caller directory: {error}"))?;
    let pins = parse_pins(
        &fs::read_to_string(root.join("kurt-python/pyproject.toml"))
            .map_err(|error| format!("cannot read kurt-python/pyproject.toml: {error}"))?,
    )?;
    let (python, target, destination) = match command {
        PythonTask::Build(args) => {
            let python = caller_relative(&caller, &args.python);
            let (_, target) = validate_python(&python)?;
            (python, target, None)
        }
        PythonTask::Install(args) => {
            let venv = caller_relative(&caller, &args.venv);
            let (python, target) = validate_venv(&venv)?;
            (python, target, Some(venv))
        }
    };
    let wheels = target_directory(root)?.join("wheels");
    let (_stage, raw, repaired) = create_stage(&wheels)?;
    let mut runner = ProcessRunner;
    let pipeline = Pipeline {
        pins: &pins,
        root,
        python: &python,
        target,
        raw: &raw,
        repaired: &repaired,
    };
    let repaired_wheel = run_pipeline(&mut runner, &pipeline, destination.is_some())?;
    let stable = wheels.join(
        repaired_wheel
            .file_name()
            .ok_or("repaired wheel has no file name")?,
    );
    fs::copy(&repaired_wheel, &stable)
        .map_err(|error| format!("cannot retain repaired wheel {}: {error}", stable.display()))?;
    Ok(ResultPath {
        wheel: stable,
        destination,
    })
}

#[cfg(test)]
mod tests;
