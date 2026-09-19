use std::env;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::{Command, ExitStatus};

const BUILD_MODE_ENV: &str = "KUAI_RUNTIME_BUILD_MODE";
const DOCKER_IMAGE_ENV: &str = "KUAI_RUNTIME_DOCKER_IMAGE";
const PRESET_ENV: &str = "KUAI_RUNTIME_PRESET";

const SUPPORTED_PRESETS: &[&str] = &[
    "debug",
    "debug-cpu",
    "release",
    "release-all",
    "release-cpu",
    "release-cuda",
    "release-cuda-nvcc",
];

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum BuildMode {
    Local,
    Docker,
}

impl BuildMode {
    fn as_str(self) -> &'static str {
        match self {
            Self::Local => "local",
            Self::Docker => "docker",
        }
    }
}

struct BuildConfig {
    build_dir: PathBuf,
    install_dir: PathBuf,
    jobs: String,
    mode: BuildMode,
    out_dir: PathBuf,
    preset: String,
    repository_root: PathBuf,
    runtime_root: PathBuf,
}

impl BuildConfig {
    fn from_env() -> Self {
        let manifest_dir = required_path("CARGO_MANIFEST_DIR");
        let runtime_root = manifest_dir.join("../../kuai-runtime");
        let runtime_root = canonicalize(&runtime_root, "kuai-runtime source directory");
        let repository_root = runtime_root
            .parent()
            .expect("kuai-runtime must have a repository parent")
            .to_path_buf();

        let out_dir = canonicalize(&required_path("OUT_DIR"), "Cargo OUT_DIR");
        let preset = env::var(PRESET_ENV).unwrap_or_else(|_| "release-cpu".to_owned());
        if !SUPPORTED_PRESETS.contains(&preset.as_str()) {
            panic!(
                "unsupported {PRESET_ENV}={preset:?}; expected one of {}",
                SUPPORTED_PRESETS.join(", ")
            );
        }

        let mode = match env::var(BUILD_MODE_ENV).as_deref() {
            Ok("docker") => BuildMode::Docker,
            Ok("local") | Err(_) => BuildMode::Local,
            Ok(value) => {
                panic!("unsupported {BUILD_MODE_ENV}={value:?}; expected \"local\" or \"docker\"")
            }
        };

        let configuration = format!("{}-{preset}", mode.as_str());
        Self {
            build_dir: out_dir.join(format!("cmake-build-{configuration}")),
            install_dir: out_dir.join(format!("install-{configuration}")),
            jobs: env::var("NUM_JOBS").unwrap_or_else(|_| "1".to_owned()),
            mode,
            out_dir,
            preset,
            repository_root,
            runtime_root,
        }
    }

    fn run(&self) {
        std::fs::create_dir_all(&self.build_dir)
            .expect("failed to create the kuai-runtime CMake build directory");
        std::fs::create_dir_all(&self.install_dir)
            .expect("failed to create the kuai-runtime install directory");

        self.run_cmake([
            OsString::from("--preset"),
            OsString::from(&self.preset),
            OsString::from("-B"),
            self.build_dir.as_os_str().to_owned(),
            OsString::from(format!(
                "-DCMAKE_INSTALL_PREFIX={}",
                self.install_dir.display()
            )),
        ]);
        self.run_cmake([
            OsString::from("--build"),
            self.build_dir.as_os_str().to_owned(),
            OsString::from("--parallel"),
            OsString::from(&self.jobs),
        ]);
        self.run_cmake([
            OsString::from("--install"),
            self.build_dir.as_os_str().to_owned(),
        ]);
    }

    fn run_cmake<const N: usize>(&self, args: [OsString; N]) {
        let status = match self.mode {
            BuildMode::Local => Command::new("cmake")
                .args(&args)
                .current_dir(&self.runtime_root)
                .status(),
            BuildMode::Docker => self.docker_command(&args).status(),
        }
        .unwrap_or_else(|error| panic!("failed to start CMake for kuai-runtime: {error}"));

        require_success(status, "kuai-runtime CMake command");
    }

    fn docker_command(&self, cmake_args: &[OsString]) -> Command {
        let image = env::var(DOCKER_IMAGE_ENV).unwrap_or_else(|_| {
            panic!("{DOCKER_IMAGE_ENV} must be set when {BUILD_MODE_ENV}=docker")
        });
        if image.is_empty() {
            panic!("{DOCKER_IMAGE_ENV} must not be empty when {BUILD_MODE_ENV}=docker");
        }
        let mut command = Command::new("docker");
        command.args(["run", "--rm"]);

        #[cfg(unix)]
        {
            let uid = command_output("id", ["-u"]);
            let gid = command_output("id", ["-g"]);
            command.args(["-u", &format!("{uid}:{gid}")]);
        }

        command.arg("-v").arg(bind_mount(&self.repository_root));
        if !self.out_dir.starts_with(&self.repository_root) {
            command.arg("-v").arg(bind_mount(&self.out_dir));
        }
        command
            .arg("-w")
            .arg(&self.runtime_root)
            .arg(image)
            .arg("cmake")
            .args(cmake_args);
        command
    }
}

fn bind_mount(path: &Path) -> OsString {
    let mut mount = path.as_os_str().to_owned();
    mount.push(":");
    mount.push(path);
    mount
}

fn canonicalize(path: &Path, description: &str) -> PathBuf {
    path.canonicalize().unwrap_or_else(|error| {
        panic!(
            "failed to resolve {description} at {}: {error}",
            path.display()
        )
    })
}

fn command_output<const N: usize>(program: &str, args: [&str; N]) -> String {
    let output = Command::new(program)
        .args(args)
        .output()
        .unwrap_or_else(|error| panic!("failed to start {program}: {error}"));
    require_success(output.status, program);
    String::from_utf8(output.stdout)
        .unwrap_or_else(|error| panic!("{program} returned non-UTF-8 output: {error}"))
        .trim()
        .to_owned()
}

fn required_path(name: &str) -> PathBuf {
    PathBuf::from(env::var_os(name).unwrap_or_else(|| panic!("Cargo did not set {name}")))
}

fn require_success(status: ExitStatus, description: &str) {
    if !status.success() {
        panic!("{description} failed with {status}");
    }
}

fn main() {
    for path in [
        "../../kuai-runtime/CMakeLists.txt",
        "../../kuai-runtime/CMakePresets.json",
        "../../kuai-runtime/cmake",
        "../../kuai-runtime/src",
        "../../kuai-runtime/vendor",
    ] {
        println!("cargo:rerun-if-changed={path}");
    }
    for variable in [BUILD_MODE_ENV, DOCKER_IMAGE_ENV, PRESET_ENV, "NUM_JOBS"] {
        println!("cargo:rerun-if-env-changed={variable}");
    }

    let config = BuildConfig::from_env();
    // CPU integration tests require an installed CPU vendor module.
    println!("cargo:rustc-check-cfg=cfg(kuai_runtime_cpu)");
    let cpu_enabled = matches!(
        config.preset.as_str(),
        "debug-cpu" | "release-cpu" | "release-all"
    );
    println!("cargo:cpu_enabled={cpu_enabled}");
    if cpu_enabled {
        println!("cargo:rustc-cfg=kuai_runtime_cpu");
    }
    println!(
        "cargo:warning=building kuai-runtime with preset {} in {:?} mode",
        config.preset, config.mode
    );
    config.run();

    let library_dir = config.install_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=dylib=kurt");
    println!(
        "cargo:rustc-env=KUAI_RUNTIME_INSTALL_DIR={}",
        config.install_dir.display()
    );
}
