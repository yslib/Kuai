use super::*;
use crate::Cli;
use clap::Parser;
use std::fs;

struct RecordingRunner {
    commands: Vec<Vec<std::ffi::OsString>>,
    failing_at: usize,
}

impl RecordingRunner {
    fn failing_at(failing_at: usize) -> Self {
        Self {
            commands: vec![],
            failing_at,
        }
    }
}

impl Runner for RecordingRunner {
    fn run(
        &mut self,
        program: &std::path::Path,
        args: &[std::ffi::OsString],
        _: &ChildEnvironment,
    ) -> Result<(), String> {
        self.commands.push(
            std::iter::once(program.as_os_str().to_owned())
                .chain(args.iter().cloned())
                .collect(),
        );
        if self.commands.len() == self.failing_at {
            Err("failed as requested".into())
        } else {
            Ok(())
        }
    }
}

fn supported_probe(prefix: &std::path::Path, base_prefix: &std::path::Path) -> PythonProbe {
    PythonProbe {
        prefix: prefix.into(),
        base_prefix: base_prefix.into(),
        implementation: "cpython".into(),
        major: 3,
        minor: 14,
        gil_disabled: false,
        machine: if std::env::consts::ARCH == "aarch64" {
            "arm64"
        } else {
            "x86_64"
        }
        .into(),
        platform: "darwin".into(),
    }
}

#[test]
fn probe_validation_rejects_unsupported_python_variants() {
    let temp = tempfile::tempdir().unwrap();
    let base = temp.path().join("base");
    for mutate in [
        |probe: &mut PythonProbe| probe.implementation = "pypy".into(),
        |probe: &mut PythonProbe| probe.minor = 9,
        |probe: &mut PythonProbe| probe.gil_disabled = true,
        |probe: &mut PythonProbe| probe.platform = "linux".into(),
        |probe: &mut PythonProbe| probe.machine = "not-the-host".into(),
    ] {
        let mut probe = supported_probe(temp.path(), &base);
        mutate(&mut probe);
        assert!(validate_probe(&probe).is_err());
    }
}

#[test]
fn validated_probe_selects_the_native_apple_rust_target() {
    let temp = tempfile::tempdir().unwrap();
    let probe = supported_probe(temp.path(), &temp.path().join("base"));
    assert_eq!(
        validate_probe(&probe).unwrap(),
        native_rust_target().unwrap()
    );
}

#[cfg(unix)]
#[test]
fn venv_validation_keeps_the_requested_python_symlink_and_checks_prefix() {
    use std::os::unix::fs::symlink;

    let temp = tempfile::tempdir().unwrap();
    let venv = temp.path().join("venv");
    let base = temp.path().join("base");
    fs::create_dir_all(venv.join("bin")).unwrap();
    fs::create_dir(&base).unwrap();
    let python = venv.join("bin/python");
    symlink("../../base/python", &python).unwrap();
    fs::write(base.join("python"), []).unwrap();

    let (result, _) =
        validate_venv_with_probe(&venv, |_| Ok(supported_probe(&venv, &base))).unwrap();
    assert_eq!(result, fs::canonicalize(&venv).unwrap().join("bin/python"));
    assert_ne!(fs::canonicalize(&result).unwrap(), result);
    assert!(validate_venv_with_probe(&venv, |_| Ok(supported_probe(&venv, &venv))).is_err());
    assert!(
        validate_venv_with_probe(&venv, |_| Ok(supported_probe(
            &base,
            &temp.path().join("other")
        )))
        .is_err()
    );
}

#[test]
fn child_environment_overrides_pyo3_and_runtime_only_on_the_child_command() {
    let temp = tempfile::tempdir().unwrap();
    let python = temp.path().join("python with spaces");
    let environment = ChildEnvironment {
        root: temp.path().into(),
        python: python.clone(),
    };
    let before = std::env::var_os("PYO3_CONFIG_FILE");
    let mut command = std::process::Command::new("ignored");
    environment.apply(&mut command);
    let envs = command
        .get_envs()
        .collect::<std::collections::BTreeMap<_, _>>();
    assert_eq!(
        envs.get(std::ffi::OsStr::new("PYO3_PYTHON")),
        Some(&Some(python.as_os_str()))
    );
    assert_eq!(
        envs.get(std::ffi::OsStr::new("PYO3_CONFIG_FILE")),
        Some(&None)
    );
    assert_eq!(
        envs.get(std::ffi::OsStr::new("KUAI_RUNTIME_BUILD_MODE")),
        Some(&Some(std::ffi::OsStr::new("local")))
    );
    assert_eq!(
        envs.get(std::ffi::OsStr::new("KUAI_RUNTIME_PRESET")),
        Some(&Some(std::ffi::OsStr::new("release-cpu")))
    );
    assert_eq!(std::env::var_os("PYO3_CONFIG_FILE"), before);
}

#[test]
fn cli_requires_python_build_flag_and_venv_install_flag() {
    assert!(Cli::try_parse_from(["cargo-xtask", "python", "build"]).is_err());
    assert!(Cli::try_parse_from(["cargo-xtask", "python", "install"]).is_err());
    assert!(
        Cli::try_parse_from([
            "cargo-xtask",
            "python",
            "build",
            "--python",
            "python with spaces/bin/python",
        ])
        .is_ok()
    );
    assert!(
        Cli::try_parse_from([
            "cargo-xtask",
            "python",
            "install",
            "--venv",
            "venv with spaces",
        ])
        .is_ok()
    );
}

#[test]
fn pins_come_from_the_pyproject_configuration() {
    let pins = parse_pins(
        r#"
[build-system]
requires = ["setuptools", "maturin==1.15.0"]

[tool.kurt-wheel]
delocate = "0.13.0"
"#,
    )
    .unwrap();
    assert_eq!(pins.maturin, "1.15.0");
    assert_eq!(pins.delocate, "0.13.0");
    for broken in [
        "[build-system]\nrequires = [\"maturin>=1\"]\n[tool.kurt-wheel]\ndelocate = \"0.13.0\"",
        "[build-system]\nrequires = [\"maturin==1.15.0\"]",
    ] {
        assert!(parse_pins(broken).is_err());
    }
}

#[test]
fn paths_are_made_absolute_from_the_caller_before_subprocesses() {
    let caller = std::env::temp_dir().join("kuai caller path");
    assert_eq!(
        caller_relative(&caller, std::path::Path::new("venv with spaces/bin/python")),
        caller.join("venv with spaces/bin/python")
    );
    assert_eq!(
        caller_relative(&caller, std::path::Path::new("/already/absolute")),
        std::path::PathBuf::from("/already/absolute")
    );
}

#[test]
fn wheel_selection_requires_exactly_one_fresh_wheel() {
    let temp = tempfile::tempdir().unwrap();
    assert!(only_wheel(temp.path()).unwrap_err().contains("found 0"));
    fs::write(temp.path().join("first.whl"), []).unwrap();
    assert_eq!(
        only_wheel(temp.path()).unwrap(),
        temp.path().join("first.whl")
    );
    fs::write(temp.path().join("second.whl"), []).unwrap();
    assert!(only_wheel(temp.path()).unwrap_err().contains("found 2"));
}

#[test]
fn each_stage_is_empty_and_unique_even_when_wheels_already_exist() {
    let temp = tempfile::tempdir().unwrap();
    fs::write(temp.path().join("stale.whl"), []).unwrap();
    let (first, raw, repaired) = create_stage(temp.path()).unwrap();
    let (second, _, _) = create_stage(temp.path()).unwrap();
    assert_ne!(first.path(), second.path());
    assert!(raw.is_dir() && repaired.is_dir());
    assert!(only_wheel(&raw).is_err());
}

#[test]
fn missing_interpreter_and_venv_are_rejected_before_any_build() {
    let temp = tempfile::tempdir().unwrap();
    assert!(validate_python(&temp.path().join("missing-python")).is_err());
    assert!(validate_venv(&temp.path().join("missing-venv")).is_err());
}

#[test]
fn runtime_environment_accepts_only_local_release_cpu_or_unset() {
    let good = [
        RuntimeEnvironment::default(),
        RuntimeEnvironment {
            build_mode: Some("local".into()),
            preset: Some("release-cpu".into()),
        },
        RuntimeEnvironment {
            build_mode: Some("local".into()),
            preset: None,
        },
        RuntimeEnvironment {
            build_mode: None,
            preset: Some("release-cpu".into()),
        },
    ];
    for environment in good {
        environment.validate().unwrap();
    }
    for environment in [
        RuntimeEnvironment {
            build_mode: Some("release".into()),
            preset: None,
        },
        RuntimeEnvironment {
            build_mode: Some("local".into()),
            preset: Some("debug-cpu".into()),
        },
    ] {
        assert!(environment.validate().is_err());
    }
}

#[test]
fn build_or_repair_failure_stops_before_a_wheel_can_be_installed() {
    let temp = tempfile::tempdir().unwrap();
    let raw = temp.path().join("raw");
    let repaired = temp.path().join("repaired");
    fs::create_dir_all(&raw).unwrap();
    fs::create_dir_all(&repaired).unwrap();
    fs::write(raw.join("kupy.whl"), []).unwrap();
    let mut runner = RecordingRunner::failing_at(1);
    assert!(
        run_build_and_repair(
            &mut runner,
            &Pins {
                maturin: "1.15.0".into(),
                delocate: "0.13.0".into()
            },
            temp.path(),
            std::path::Path::new("/python"),
            native_rust_target().unwrap(),
            &raw,
            &repaired,
        )
        .is_err()
    );
    assert_eq!(runner.commands.len(), 1);
    assert!(
        runner.commands[0]
            .windows(2)
            .any(|args| args == ["--target", native_rust_target().unwrap()])
    );

    let mut runner = RecordingRunner::failing_at(2);
    assert!(
        run_build_and_repair(
            &mut runner,
            &Pins {
                maturin: "1.15.0".into(),
                delocate: "0.13.0".into()
            },
            temp.path(),
            std::path::Path::new("/python with spaces"),
            native_rust_target().unwrap(),
            &raw,
            &repaired,
        )
        .is_err()
    );
    assert_eq!(runner.commands.len(), 2);
    assert_eq!(runner.commands[1][0], "uvx");
    assert!(
        runner.commands[1]
            .iter()
            .any(|arg| arg == "delocate==0.13.0")
    );
}

#[test]
fn pipeline_failure_never_invokes_the_install_command() {
    let temp = tempfile::tempdir().unwrap();
    let raw = temp.path().join("raw");
    let repaired = temp.path().join("repaired");
    fs::create_dir_all(&raw).unwrap();
    fs::create_dir_all(&repaired).unwrap();
    fs::write(raw.join("kupy.whl"), []).unwrap();
    for failing_at in [1, 2] {
        let mut runner = RecordingRunner::failing_at(failing_at);
        let pins = Pins {
            maturin: "1.15.0".into(),
            delocate: "0.13.0".into(),
        };
        let pipeline = Pipeline {
            pins: &pins,
            root: temp.path(),
            python: std::path::Path::new("/python"),
            target: native_rust_target().unwrap(),
            raw: &raw,
            repaired: &repaired,
        };
        assert!(run_pipeline(&mut runner, &pipeline, true).is_err());
        assert_eq!(runner.commands.len(), failing_at);
        assert!(runner.commands.iter().all(|command| command[0] != "uv"));
    }
}
