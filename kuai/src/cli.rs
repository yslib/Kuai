use clap::Parser;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(version, name = "kuai", about = "Kuai Programming Language CLI")]
pub struct Args {
    /// 执行指定的代码字符串
    #[arg(short = 'e', long)]
    execute: Option<String>,
    //
    /// 要执行的 .ku 文件
    file: Option<PathBuf>,
    //
    /// 进入 REPL 模式 (默认行为)
    #[arg(short, long)]
    interactive: bool,
}
