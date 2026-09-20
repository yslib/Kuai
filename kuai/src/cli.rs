use clap::Parser;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(version, name = "kuai", about = "Kuai Programming Language CLI")]
pub struct Args {
    /// Execute the specified code string
    #[arg(short = 'e', long)]
    execute: Option<String>,
    //
    /// The .ku file to execute
    file: Option<PathBuf>,
    //
    /// Enter REPL mode (the default behavior)
    #[arg(short, long)]
    interactive: bool,
}
