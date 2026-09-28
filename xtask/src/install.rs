use std::env;
use std::error::Error;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};

use serde_json::Value;

#[derive(clap::Args)]
pub(crate) struct InstallArgs {
    /// Install executables and plugins under <prefix>/bin.
    #[arg(long)]
    prefix: PathBuf,
    /// Workspace package to install.
    #[arg(short, long, default_value = "kuai")]
    package: String,
    /// Install one executable example instead of the package's binaries.
    #[arg(long)]
    example: Option<String>,
    #[arg(long, default_value = "release")]
    profile: String,
    #[arg(long, value_delimiter = ',')]
    features: Vec<String>,
    #[arg(long)]
    no_default_features: bool,
}

pub(crate) fn install(root: &Path, args: &InstallArgs) -> Result<(), Box<dyn Error>> {
    let metadata: Value = serde_json::from_slice(
        &run(cargo(root).args(["metadata", "--no-deps", "--format-version", "1", "--locked"]))?
            .stdout,
    )?;
    let packages = metadata["packages"]
        .as_array()
        .ok_or("missing Cargo packages")?;
    let package = packages
        .iter()
        .find(|p| p["name"] == args.package)
        .ok_or_else(|| format!("unknown workspace package: {}", args.package))?;
    let runtime = packages
        .iter()
        .find(|p| p["name"] == "kurt-sys")
        .ok_or("missing kurt-sys package")?;

    let mut build = cargo(root);
    build.args([
        "build",
        "--locked",
        "--message-format=json-render-diagnostics",
        "--package",
        &args.package,
        "--profile",
        &args.profile,
    ]);
    if let Some(example) = &args.example {
        build.args(["--example", example]);
    } else {
        build.arg("--bins");
    }
    if !args.features.is_empty() {
        build.arg("--features").arg(args.features.join(","));
    }
    if args.no_default_features {
        build.arg("--no-default-features");
    }
    let output = run(&mut build)?;
    let messages: Vec<Value> = String::from_utf8(output.stdout)?
        .lines()
        .filter(|line| line.starts_with('{'))
        .map(serde_json::from_str)
        .collect::<Result<_, _>>()?;
    let kind = if args.example.is_some() {
        "example"
    } else {
        "bin"
    };
    let mut artifacts: Vec<PathBuf> = messages
        .iter()
        .filter(|m| {
            m["reason"] == "compiler-artifact"
                && m["package_id"] == package["id"]
                && m["target"]["kind"] == serde_json::json!([kind])
                && args
                    .example
                    .as_ref()
                    .is_none_or(|name| m["target"]["name"] == *name)
        })
        .filter_map(|m| m["executable"].as_str().map(PathBuf::from))
        .collect();
    if artifacts.is_empty() {
        return Err("Cargo produced no executable to install".into());
    }

    if let Some(native) = messages.iter().find(|m| {
        m["reason"] == "compiler-artifact"
            && m["package_id"] == runtime["id"]
            && m["target"]["name"] == "kurt_sys"
    }) {
        let script = messages
            .iter()
            .find(|m| m["reason"] == "build-script-executed" && m["package_id"] == runtime["id"])
            .ok_or("missing Kurt build output")?;
        let paths: Vec<_> = script["linked_paths"]
            .as_array()
            .ok_or("missing native search paths")?
            .iter()
            .filter_map(Value::as_str)
            .map(|path| PathBuf::from(path.strip_prefix("native=").unwrap_or(path)))
            .collect();
        let features = native["features"]
            .as_array()
            .ok_or("missing runtime features")?;
        for vendor in ["cpu", "cuda"] {
            if !features.iter().any(|feature| feature == vendor) {
                continue;
            }
            let filename = format!(
                "{}kurt_{vendor}{}",
                env::consts::DLL_PREFIX,
                env::consts::DLL_SUFFIX
            );
            let plugin = paths
                .iter()
                .map(|path| path.join(&filename))
                .find(|path| path.is_file())
                .ok_or_else(|| {
                    format!("selected plugin {filename} was not found in the native installation")
                })?;
            artifacts.push(plugin);
        }
    }

    let bin = args.prefix.join("bin");
    install_artifacts(&artifacts, &bin)
}

fn install_artifacts(artifacts: &[PathBuf], bin: &Path) -> Result<(), Box<dyn Error>> {
    fs::create_dir_all(bin)?;
    for artifact in artifacts {
        let destination = bin.join(artifact.file_name().ok_or("artifact has no filename")?);
        if !destination.exists() || artifact.canonicalize()? != destination.canonicalize()? {
            fs::copy(artifact, &destination)?;
        }
        println!("Installed {}", destination.display());
    }
    Ok(())
}

fn cargo(root: &Path) -> Command {
    let mut command = Command::new(env::var_os("CARGO").unwrap_or_else(|| "cargo".into()));
    command.current_dir(root);
    command
}

fn run(command: &mut Command) -> Result<Output, Box<dyn Error>> {
    let output = command.stderr(Stdio::inherit()).output()?;
    if !output.status.success() {
        return Err(format!("{command:?} failed with {}", output.status).into());
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn installing_an_existing_artifact_in_place_preserves_it() {
        let directory = tempfile::tempdir().unwrap();
        let plugin = directory.path().join("plugin");
        fs::write(&plugin, b"native plugin contents").unwrap();
        install_artifacts(std::slice::from_ref(&plugin), directory.path()).unwrap();
        assert_eq!(fs::read(&plugin).unwrap(), b"native plugin contents");
    }
}
