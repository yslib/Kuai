fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rustc-check-cfg=cfg(kuai_runtime_cpu)");
    if std::env::var("DEP_KURT_CPU_ENABLED").as_deref() == Ok("true") {
        println!("cargo:rustc-cfg=kuai_runtime_cpu");
    }
}
