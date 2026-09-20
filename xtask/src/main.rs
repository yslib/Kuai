use std::path::Path;
use std::process::ExitCode;

use clap::{Parser, Subcommand};

mod cpp;

#[derive(Parser)]
#[command(name = "cargo xtask", about = "Kuai repository development tasks")]
struct Cli {
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
}

#[derive(Subcommand)]
enum CppTask {
    /// Format C/C++/CUDA sources with the pinned clang-format release.
    Fmt(cpp::FormatArgs),
}

fn main() -> ExitCode {
    let Cli {
        command: Task::Cpp {
            command: CppTask::Fmt(args),
        },
    } = Cli::parse();
    let root = Path::new(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("xtask is a root workspace member");
    match cpp::format(root, &args) {
        Ok(count) => {
            let action = if args.check { "checked" } else { "formatted" };
            println!("cpp fmt: {action} {count} file(s).");
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("cpp fmt: {error}");
            ExitCode::FAILURE
        }
    }
}
