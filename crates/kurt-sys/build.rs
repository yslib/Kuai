use std::env;
use std::path::{self, Path, PathBuf};
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-env-changed=CMAKE_INSTALL_PREFIX");
    let prefix = env::var_os("CMAKE_INSTALL_PREFIX")
        .map(|prefix| {
            let prefix = PathBuf::from(prefix);
            println!("cargo:rerun-if-changed={}", prefix.join("lib").display());
            prefix
        })
        .unwrap_or_else(build_runtime);
    // Preserve Cargo's path form: Windows canonicalization adds a verbatim prefix,
    // which Cargo would filter out of its runtime DLL search paths.
    let prefix = path::absolute(&prefix).unwrap_or_else(|error| {
        panic!(
            "cannot resolve host installation at {}: {error}",
            prefix.display()
        )
    });
    let library_dir = prefix.join("lib");
    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=static=kurt");
    match env::var("CARGO_CFG_TARGET_OS").unwrap().as_str() {
        "windows" => {
            // CMake installs vendor DLLs in bin; Cargo adds OUT_DIR search paths
            // to PATH when it runs binaries and tests.
            println!(
                "cargo:rustc-link-search=native={}",
                prefix.join("bin").display()
            );
        }
        "macos" => println!("cargo:rustc-link-lib=c++"),
        "linux" => {
            println!("cargo:rustc-link-lib=stdc++");
            println!("cargo:rustc-link-lib=dl");
            println!("cargo:rustc-link-lib=pthread");
        }
        _ => {}
    }
}

fn build_runtime() -> PathBuf {
    let source = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../../kurt-cpp");
    let prefix = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let profile = if env::var("OPT_LEVEL").unwrap() == "0" {
        "Debug"
    } else {
        "Release"
    };

    for path in ["CMakeLists.txt", "cmake", "src", "vendor"] {
        println!("cargo:rerun-if-changed={}", source.join(path).display());
    }
    for name in [
        "CMAKE_TOOLCHAIN_FILE",
        "CMAKE_GENERATOR",
        "CXX",
        "CXXFLAGS",
        "CUDACXX",
        "CUDAARCHS",
    ] {
        println!("cargo:rerun-if-env-changed={name}");
    }

    let vendors: Vec<_> = [("cpu", "CARGO_FEATURE_CPU"), ("cuda", "CARGO_FEATURE_CUDA")]
        .into_iter()
        .filter_map(|(vendor, feature)| env::var_os(feature).map(|_| vendor))
        .collect();
    build_cmake(&source, &prefix.join("build"), &prefix, profile, &[]);
    if !vendors.is_empty() {
        build_cmake(
            &source.join("vendor"),
            &prefix.join("vendor-build"),
            &prefix,
            profile,
            &vendors,
        );
    }
    prefix
}

fn build_cmake(source: &Path, build: &Path, prefix: &Path, profile: &str, vendors: &[&str]) {
    let mut configure = Command::new("cmake");
    configure
        .arg("--fresh")
        .arg("-S")
        .arg(source)
        .arg("-B")
        .arg(build)
        .arg(format!("-DCMAKE_BUILD_TYPE={profile}"))
        .arg(format!("-DCMAKE_PREFIX_PATH={}", prefix.display()))
        .arg(format!("-DCMAKE_INSTALL_PREFIX={}", prefix.display()))
        .arg("-DCMAKE_INSTALL_LIBDIR=lib")
        .arg("-DBUILD_SHARED_LIBS=OFF");
    if env::var("CARGO_CFG_TARGET_ENV").unwrap() == "msvc" {
        // Rust uses the release CRT even in debug builds; host and plugins must match.
        configure.arg("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL");
    }
    if !vendors.is_empty() {
        configure.arg(format!("-DKURT_ENABLED_VENDORS={}", vendors.join(";")));
    }
    run(&mut configure);
    run(Command::new("cmake")
        .arg("--build")
        .arg(build)
        .arg("--config")
        .arg(profile)
        .arg("--parallel")
        .arg(env::var("NUM_JOBS").unwrap()));
    run(Command::new("cmake")
        .arg("--install")
        .arg(build)
        .arg("--config")
        .arg(profile));
}

fn run(command: &mut Command) {
    let status = command
        .status()
        .unwrap_or_else(|error| panic!("{command:?}: {error}"));
    assert!(status.success(), "{command:?} failed with {status}");
}
