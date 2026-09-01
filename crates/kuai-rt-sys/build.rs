fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    let dst = cmake::Config::new("../../kuai-runtime")
        .define("CMAKE_BUILD_TYPE", "Release")
        .build();
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo::rustc-link-lib=static=krt");
    println!("cargo::rustc-link-lib=stdc++");
}
