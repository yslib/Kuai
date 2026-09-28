use std::path::Path;
use std::process::ExitCode;

use clap::{Parser, Subcommand};

mod cpp;
mod install;

#[derive(Parser)]
#[command(name = "cargo xtask", about = "Kuai repository development tasks")]
pub(crate) struct Cli {
    #[command(subcommand)]
    command: Task,
}

#[derive(Subcommand)]
enum Task {
    /// Build and install an executable with its selected Kurt plugins.
    Install(install::InstallArgs),
    /// C++ runtime development tools.
    Cpp {
        #[command(subcommand)]
        command: CppTask,
    },
}

#[derive(Subcommand)]
enum CppTask {
    /// Format C/C++/CUDA sources with the pinned clang-format release.
    Fmt(cpp::FormatArgs),
}

fn main() -> ExitCode {
    let Cli { command } = Cli::parse();
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("xtask is a root workspace member");
    match command {
        Task::Install(args) => match install::install(root, &args) {
            Ok(()) => ExitCode::SUCCESS,
            Err(error) => {
                eprintln!("install: {error}");
                ExitCode::FAILURE
            }
        },
        Task::Cpp {
            command: CppTask::Fmt(args),
        } => match cpp::format(root, &args) {
            Ok(count) => {
                let action = if args.check { "checked" } else { "formatted" };
                println!("cpp fmt: {action} {count} file(s).");
                ExitCode::SUCCESS
            }
            Err(error) => {
                eprintln!("cpp fmt: {error}");
                ExitCode::FAILURE
            }
        },
    }
}
