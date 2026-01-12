mod highligher;
use driver::context::*;
use miette::Report;
use rustyline::error::ReadlineError;
use rustyline::{Config, DefaultEditor, EditMode};
use sema::resolver::*;
use std::io::Read;
use std::net::{SocketAddr, TcpListener};
use std::sync::{Arc, Mutex};
use std::thread;
use syntax::parser::*;

use clap::Parser as ClapParser;
use std::path::PathBuf;

/// Simple program to greet a person
#[derive(ClapParser, Debug)]
#[command(
    name = "Tofu",
    version = "0.1.0",
    about = "Tofu Programming Language CLI"
)]
struct Args {
    #[arg(short = 'e', long)]
    execute: Option<String>,

    #[arg(short, long)]
    file: Option<PathBuf>,

    #[arg(short = 'l', long = "listen",
        num_args=0..=1,
        default_missing_value = "127.0.0.1:9999",
        require_equals = true)]
    listen: Option<SocketAddr>,
}

fn compile_and_run(input: &str, ctx: &mut Context) {
    let mut parser = Parser::new(input, &mut ctx.interner);

    match parser.parse() {
        Ok(module) => {
            let mut resolver = Resolver::new(&ctx.interner, Arc::clone(&ctx.global_scope));
            resolver.resolve(&module);
            if resolver.diagnostics().is_empty() {
                println!("✨ Parse successful.");
            } else {
                for diag in resolver.diagnostics() {
                    let adapter = diag.render("stdin".to_string(), input.to_string());
                    println!("{:?}", Report::new(adapter));
                }
            }
        }
        Err(diagnostics) => {
            for diag in diagnostics {
                let adapter = diag.render("stdin".to_string(), input.to_string());
                println!("{:?}", Report::new(adapter));
            }
        }
    }
}

fn start_tcp_server(shared_ctx: Arc<Mutex<Context>>, addr: SocketAddr) -> miette::Result<()> {
    let listener =
        TcpListener::bind(addr).map_err(|e| miette::miette!("Failed to bind TCP port {}", e))?;

    println!("Tofu REPL Listening on {}", addr);

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                let mut buffer = String::new();
                let res = stream.read_to_string(&mut buffer);
                match res {
                    Ok(_) => {
                        let mut ctx = shared_ctx.lock().unwrap();
                        println!(">> {}", buffer);
                        compile_and_run(&buffer, &mut ctx);
                    }
                    Err(e) => {
                        println!("Failed to read from connection: {}", e);
                    }
                }
            }
            Err(e) => {
                println!("Connection failed: {}", e);
            }
        }
    }

    Ok(())
}

fn print_banner() {
    let version = env!("CARGO_PKG_VERSION");
    println!("╔══════════════════════════════════════════════════════════════════════════╗");
    println!("║                                                                          ║");
    println!(
        "║                         🚀 Tofu REPL v{:<7}                            ║",
        version
    );
    println!("║                                                                          ║");
    println!("║   • Type expressions to evaluate                                         ║");
    println!("║   • Ctrl-D / Ctrl-C to exit                                              ║");
    println!("║                                                                          ║");
    println!("╚══════════════════════════════════════════════════════════════════════════╝");
}

fn run_repl(shared_ctx: Arc<Mutex<Context>>) -> miette::Result<()> {
    let config = Config::builder()
        .history_ignore_space(true)
        .edit_mode(EditMode::Vi)
        .build();

    let mut rl = DefaultEditor::with_config(config)
        .map_err(|e| miette::miette!("Failed to initialize Tofu REPL: {}", e))?;

    // rl.set_helper(Some(TofuHelper::default()));
    if rl.load_history("history.txt").is_err() {
        println!("No previous history.");
    }

    print_banner();

    let mut terminate = false;
    loop {
        let mut is_first_line = true;
        let mut buffer = String::new();
        loop {
            let prompt = if buffer.is_empty() { ">> " } else { ".. " };
            match rl.readline(prompt) {
                Ok(line) => {
                    let trimmed = line.trim();
                    if is_first_line && trimmed.is_empty() {
                        break;
                    }
                    buffer.push_str(&line);
                    buffer.push('\n');
                    let (is_balanced, depth) = syntax::lexer::check_balanced(&buffer);
                    if is_balanced {
                        if is_first_line
                            || trimmed.is_empty()
                                && !(trimmed.ends_with(',')
                                    || trimmed.ends_with('.')
                                    || trimmed.ends_with('='))
                        {
                            break;
                        }
                    } else if depth < 0 {
                        // Unmatched closing bracket, parsing error will handle it
                        break;
                    }
                    is_first_line = false;
                }
                Err(ReadlineError::Interrupted) => {
                    println!("CTRL-C");
                    terminate = buffer.is_empty();
                    buffer.clear();
                    break;
                }
                Err(ReadlineError::Eof) => {
                    println!("CTRL-D");
                    terminate = buffer.is_empty();
                    buffer.clear();
                    break;
                }
                Err(err) => {
                    println!("Error: {:?}", err);
                    break;
                }
            }
        }
        if terminate {
            break;
        }
        let final_input = buffer.trim();
        if final_input.is_empty() {
            continue;
        }
        rl.add_history_entry(final_input).ok();
        let mut ctx = shared_ctx.lock().unwrap();
        compile_and_run(final_input, &mut ctx);
    }

    rl.save_history("history.txt").ok();
    Ok(())
}

fn main() -> miette::Result<()> {
    let args = Args::parse();
    let mut c = Context::new();
    c.inject_builtins();
    let ctx = Arc::new(Mutex::new(c));
    if let Some(code) = args.execute {
        let shared_ctx = Arc::clone(&ctx);
        let mut a = shared_ctx.lock().unwrap();
        compile_and_run(&code, &mut a);
        return Ok(());
    } else if let Some(file_path) = args.file {
        let code = std::fs::read_to_string(&file_path)
            .map_err(|e| miette::miette!("Failed to read file {}: {}", file_path.display(), e))?;
        let shared_ctx = Arc::clone(&ctx);
        let mut ctx = shared_ctx.lock().unwrap();
        compile_and_run(&code, &mut ctx);
        return Ok(());
    }
    if let Some(addr) = args.listen {
        let shared_ctx = Arc::clone(&ctx);
        thread::spawn(move || {
            if let Err(e) = start_tcp_server(shared_ctx, addr) {
                eprintln!("TCP server error: {:?}", e);
            }
        });
    }

    run_repl(Arc::clone(&ctx))?;
    Ok(())
}
