mod highligher;

use miette::{NamedSource, Report};
use parser::parser::Parser;
use rustyline::error::ReadlineError;
use rustyline::{Config, DefaultEditor, EditMode};

use crate::highligher::TofuHelper;

fn main() -> miette::Result<()> {
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

    println!("🚀 Tofu Playground");
    println!("Commands: .ast (toggle AST), .exit (quit), .help");

    let mut ctx = parser::context::Context::new();

    let show_ast = true;
    loop {
        let mut terminate = false;
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
                    let (is_balanced, depth) = parser::lexer::check_balanced(&buffer);
                    // println!(
                    //     "Debug: is_balanced = {}\n-------\n{:?}\n-------",
                    //     is_balanced, buffer
                    // );
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
                    buffer.clear();
                    terminate = buffer.is_empty();
                    break;
                }
                Err(ReadlineError::Eof) => {
                    buffer.clear();
                    println!("CTRL-D");
                    terminate = buffer.is_empty();
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
        parse_and_report(final_input, &mut ctx, show_ast);
    }

    rl.save_history("history.txt").ok();
    Ok(())
}

fn parse_and_report(input: &str, ctx: &mut parser::context::Context, show_ast: bool) {
    let mut parser = Parser::new(input, ctx);

    match parser.parse() {
        Ok(module) => {
            if show_ast {
                println!("{}", parser::pretty::print_ast(&module));
            }
            println!("✨ Parse successful.");
        }
        Err(diagnostics) => {
            for diag in diagnostics {
                let adapter = diag.render("stdin".to_string(), input.to_string());
                println!("{:?}", Report::new(adapter));
            }
        }
    }
}
