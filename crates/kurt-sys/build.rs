use std::env;
use std::path::PathBuf;

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

fn main() {
    println!("cargo:rerun-if-env-changed=CMAKE_INSTALL_PREFIX");
    println!("cargo:rerun-if-env-changed={PRESET_ENV}");

    let preset = env::var(PRESET_ENV).unwrap_or_else(|_| "release-cpu".to_owned());
    if !SUPPORTED_PRESETS.contains(&preset.as_str()) {
        panic!(
            "unsupported {PRESET_ENV}={preset:?}; expected one of {}",
            SUPPORTED_PRESETS.join(", ")
        );
    }

    // This legacy selector controls which tests compile; it does not build a vendor.
    println!("cargo:rustc-check-cfg=cfg(kuai_runtime_cpu)");
    let cpu_enabled = matches!(preset.as_str(), "debug-cpu" | "release-cpu" | "release-all");
    println!("cargo:cpu_enabled={cpu_enabled}");
    if cpu_enabled {
        println!("cargo:rustc-cfg=kuai_runtime_cpu");
    }

    let prefix = PathBuf::from(
        env::var_os("CMAKE_INSTALL_PREFIX")
            .expect("CMAKE_INSTALL_PREFIX must point to an installed Kurt host"),
    );
    let prefix = prefix.canonicalize().unwrap_or_else(|error| {
        panic!(
            "cannot resolve CMAKE_INSTALL_PREFIX at {}: {error}",
            prefix.display()
        )
    });
    let library_dir = prefix.join("lib");
    println!("cargo:rerun-if-changed={}", library_dir.display());
    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=dylib=kurt");
    println!(
        "cargo:rustc-env=KUAI_RUNTIME_INSTALL_DIR={}",
        prefix.display()
    );
}
