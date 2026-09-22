use std::collections::BTreeSet;
use std::ffi::{OsStr, OsString};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

use walkdir::WalkDir;

const DEFAULT_ROOTS: [&str; 3] = [
    "kurt-cpp/src",
    "kurt-cpp/vendor",
    "kurt-cpp/python/bindings",
];
// Leave headroom for the process environment and platform command-line limits.
const ARGUMENT_BUDGET: usize = 16 * 1024;

#[derive(clap::Args)]
pub(crate) struct FormatArgs {
    /// Check without modifying files.
    #[arg(long)]
    pub check: bool,
    /// Exclude a file or directory (repeatable, relative to the caller).
    #[arg(long, value_name = "PATH")]
    pub exclude: Vec<PathBuf>,
    /// Files/directories relative to the caller; defaults to runtime source roots.
    #[arg(value_name = "PATH")]
    pub paths: Vec<PathBuf>,
}

pub(crate) fn format(root: &Path, args: &FormatArgs) -> Result<usize, String> {
    let files = collect_files(root, args)?;
    if files.is_empty() {
        return Ok(0);
    }
    let formatter = Formatter::load(root)?;
    let arguments = formatter.arguments(args.check);
    let fixed_cost = arguments
        .iter()
        .fold(argument_cost(OsStr::new("uvx")), |cost, arg| {
            cost.saturating_add(argument_cost(arg))
        });
    let chunks = batches(&files, fixed_cost, ARGUMENT_BUDGET)?;
    // Resolve and check the pinned binary before any in-place changes.
    let version = Command::new("uvx")
        .args(&arguments[..3])
        .arg("--version")
        .stdin(Stdio::null())
        .stderr(Stdio::inherit())
        .output()
        .map_err(launch_error)?;
    if !version.status.success() {
        return Err(format!(
            "clang-format version check failed: {}",
            version.status
        ));
    }
    verify_version(&formatter.version, &version.stdout)?;

    for (index, chunk) in chunks.iter().enumerate() {
        let status = Command::new("uvx")
            .args(&arguments)
            .args(*chunk)
            .stdin(Stdio::null())
            .status();
        let error = match status {
            Ok(status) if status.success() => continue,
            Ok(status) => format!("formatter exited with {status}"),
            Err(error) => launch_error(error),
        };
        let warning = if args.check {
            ""
        } else {
            "; files may have been modified in earlier batches or part of this batch"
        };
        return Err(format!(
            "batch {}/{} failed: {error}{warning}",
            index + 1,
            chunks.len()
        ));
    }
    Ok(files.len())
}

fn canonical(path: &Path) -> Result<PathBuf, String> {
    fs::canonicalize(path).map_err(|error| format!("cannot resolve {}: {error}", path.display()))
}

fn collect_files(root: &Path, args: &FormatArgs) -> Result<Vec<PathBuf>, String> {
    let targets = if args.paths.is_empty() {
        DEFAULT_ROOTS.iter().map(|path| root.join(path)).collect()
    } else {
        args.paths.clone()
    };
    // Resolve every explicit path before collecting or modifying any files.
    let targets = targets
        .iter()
        .map(|p| canonical(p))
        .collect::<Result<Vec<_>, _>>()?;
    let excludes = args
        .exclude
        .iter()
        .map(|p| canonical(p))
        .collect::<Result<Vec<_>, _>>()?;
    let excluded = |path: &Path| excludes.iter().any(|exclude| path.starts_with(exclude));
    let mut files = BTreeSet::new();
    for target in targets {
        let metadata = fs::metadata(&target)
            .map_err(|error| format!("cannot inspect {}: {error}", target.display()))?;
        if metadata.is_file() {
            if is_source(&target) && !excluded(&target) {
                files.insert(target);
            }
        } else if metadata.is_dir() {
            let entries = WalkDir::new(&target)
                .follow_links(false)
                .into_iter()
                .filter_entry(|entry| {
                    !excluded(entry.path())
                        && !(entry.file_type().is_dir() && pruned_directory(entry.file_name()))
                });
            for entry in entries {
                let entry = entry
                    .map_err(|error| format!("cannot traverse {}: {error}", target.display()))?;
                if entry.file_type().is_file() && is_source(entry.path()) {
                    files.insert(canonical(entry.path())?);
                }
            }
        } else {
            return Err(format!(
                "target is not a regular file or directory: {}",
                target.display()
            ));
        }
    }
    Ok(files.into_iter().collect())
}

fn is_source(path: &Path) -> bool {
    matches!(
        path.extension().and_then(OsStr::to_str),
        Some("c" | "cc" | "cpp" | "cxx" | "h" | "hh" | "hpp" | "hxx" | "cu" | "cuh")
    )
}

fn pruned_directory(name: &OsStr) -> bool {
    matches!(name.to_str(), Some(".git" | "build" | "__pycache__"))
        || name.as_encoded_bytes().starts_with(b"cmake-build-")
}

fn parse_version(input: &str) -> Result<String, String> {
    let version = input.trim();
    let parts: Vec<_> = version.split('.').collect();
    if parts.len() != 3
        || parts
            .iter()
            .any(|part| part.is_empty() || !part.bytes().all(|c| c.is_ascii_digit()))
    {
        return Err("expected three numeric components in .clang-format-version".into());
    }
    Ok(version.into())
}

fn verify_version(expected: &str, output: &[u8]) -> Result<(), String> {
    let actual = String::from_utf8_lossy(output);
    let actual = actual.trim_end_matches(['\r', '\n']);
    if actual != format!("clang-format version {expected}") {
        return Err(format!(
            "expected clang-format version {expected}, got {actual:?}"
        ));
    }
    Ok(())
}

struct Formatter {
    version: String,
    style: PathBuf,
}

impl Formatter {
    fn load(root: &Path) -> Result<Self, String> {
        let version_path = root.join("kurt-cpp/.clang-format-version");
        let contents = fs::read_to_string(&version_path)
            .map_err(|error| format!("cannot read {}: {error}", version_path.display()))?;
        let version = parse_version(&contents)?;
        let style = canonical(&root.join("kurt-cpp/.clang-format"))?;
        if !style.is_file() {
            return Err(format!("style is not a regular file: {}", style.display()));
        }
        Ok(Self { version, style })
    }

    fn arguments(&self, check: bool) -> Vec<OsString> {
        let mut style = OsString::from("--style=file:");
        style.push(&self.style);
        let mut args = vec![
            "--from".into(),
            format!("clang-format=={}", self.version).into(),
            "clang-format".into(),
            style,
        ];
        if check {
            args.extend(["--dry-run".into(), "--Werror".into()]);
        } else {
            args.push("-i".into());
        }
        args
    }
}

fn batches(files: &[PathBuf], fixed_cost: usize, budget: usize) -> Result<Vec<&[PathBuf]>, String> {
    if fixed_cost >= budget {
        return Err("formatter options exceed the command-line argument budget".into());
    }
    let mut chunks = Vec::new();
    let mut start = 0;
    let mut cost = fixed_cost;
    for (index, path) in files.iter().enumerate() {
        let next = argument_cost(path.as_os_str());
        if next > budget - fixed_cost {
            return Err(format!(
                "path exceeds the command-line argument budget: {}",
                path.display()
            ));
        }
        if next > budget - cost {
            chunks.push(&files[start..index]);
            start = index;
            cost = fixed_cost;
        }
        cost += next;
    }
    if start < files.len() {
        chunks.push(&files[start..]);
    }
    Ok(chunks)
}

fn argument_cost(argument: &OsStr) -> usize {
    // Conservatively allow for Windows quoting/UTF-16 and Unix argv pointers.
    argument
        .as_encoded_bytes()
        .len()
        .saturating_mul(2)
        .saturating_add(3 + size_of::<usize>())
}

fn launch_error(error: std::io::Error) -> String {
    if error.kind() == std::io::ErrorKind::NotFound {
        format!(
            "uvx is required; install uv: https://docs.astral.sh/uv/getting-started/installation/ ({error})"
        )
    } else {
        format!("cannot launch uvx: {error}")
    }
}

#[cfg(test)]
mod tests;
