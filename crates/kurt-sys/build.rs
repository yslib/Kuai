use std::env;
use std::fs;
use std::path::{Path, PathBuf};
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
    let prefix = prefix.canonicalize().unwrap_or_else(|error| {
        panic!(
            "cannot resolve host installation at {}: {error}",
            prefix.display()
        )
    });
    let library_dir = prefix.join("lib");
    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=static=kurt");
    match env::var("CARGO_CFG_TARGET_OS").unwrap().as_str() {
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
    for name in ["CMAKE_TOOLCHAIN_FILE", "CMAKE_GENERATOR", "CXX", "CXXFLAGS"] {
        println!("cargo:rerun-if-env-changed={name}");
    }

    build_cmake(&source, &prefix.join("build"), &prefix, profile);
    build_cmake(
        &source.join("vendor"),
        &prefix.join("vendor-build"),
        &prefix,
        profile,
    );

    let (directory, library) = match env::var("CARGO_CFG_TARGET_OS").unwrap().as_str() {
        "windows" => ("bin", "kurt_cpu.dll"),
        "macos" => ("lib", "libkurt_cpu.dylib"),
        "linux" => ("lib", "libkurt_cpu.so"),
        target => panic!("unsupported Kurt target: {target}"),
    };
    let plugin = prefix.join(directory).join(library);
    // OUT_DIR is <target>/<profile>/build/<package>/out, including --target builds.
    let output = prefix.ancestors().nth(3).unwrap();
    for directory in [
        output.to_path_buf(),
        output.join("deps"),
        output.join("examples"),
    ] {
        fs::create_dir_all(&directory).unwrap();
        let destination = directory.join(library);
        fs::copy(&plugin, &destination).unwrap_or_else(|error| {
            panic!(
                "cannot copy {} to {}: {error}",
                plugin.display(),
                destination.display()
            )
        });
    }
    prefix
}

fn build_cmake(source: &Path, build: &Path, prefix: &Path, profile: &str) {
    run(Command::new("cmake")
        .arg("--fresh")
        .arg("-S")
        .arg(source)
        .arg("-B")
        .arg(build)
        .arg(format!("-DCMAKE_BUILD_TYPE={profile}"))
        .arg(format!("-DCMAKE_PREFIX_PATH={}", prefix.display()))
        .arg(format!("-DCMAKE_INSTALL_PREFIX={}", prefix.display()))
        .arg("-DCMAKE_INSTALL_LIBDIR=lib")
        .arg("-DBUILD_SHARED_LIBS=OFF"));
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
