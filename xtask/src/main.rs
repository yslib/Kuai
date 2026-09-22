use std::path::Path;
use std::process::ExitCode;

use clap::{Parser, Subcommand};

mod cpp;
mod python;

#[derive(Parser)]
#[command(name = "cargo xtask", about = "Kuai repository development tasks")]
pub(crate) struct Cli {
    #[command(subcommand)]
    command: Task,
}

#[derive(Subcommand)]
enum Task {
    /// C++ runtime development tools.
    Cpp {
        #[command(subcommand)]
        command: CppTask,
    },
    /// Build or install the KuPy Python package.
    Python {
        #[command(subcommand)]
        command: python::PythonTask,
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
        Task::Python { command } => match python::run(root, &command) {
            Ok(result) => {
                println!("python {}: {}", command.action(), result.wheel.display());
                if let Some(destination) = result.destination {
                    println!("python install: {}", destination.display());
                }
                ExitCode::SUCCESS
            }
            Err(error) => {
                eprintln!("python {}: {error}", command.action());
                ExitCode::FAILURE
            }
        },
    }
}
