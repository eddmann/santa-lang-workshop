use clap::Parser;
use std::fs;
use std::io::{self, Write};
use std::time::{SystemTime, UNIX_EPOCH};
use serde_json::json;

#[derive(Parser)]
#[command(name = "santa-bootstrap")]
#[command(about = "Bootstrap a new Santa language implementation")]
#[command(version = "0.1.0")]
#[command(long_about = r#"
santa-bootstrap creates a new implementation directory for the Santa language.

It prompts for:
  - Language to implement (e.g., Python, Rust, Go)
  - Harness/agent (e.g., Claude Code, Codex, Cursor)
  - LLM model being used (e.g., GPT-4o, GPT-5, Sonnet 4)
  - Additional requirements (optional)

Creates impl/<lang>-<harness>-<model>-<timestamp>/ directory (segments normalized to lowercase alphanumerics only) and generates TASKS.md
from the template in specs/TASKS.md, replacing placeholders:
  - <lang> with the language name
  - <harness> with the harness/agent name
  - <model> with the model name
  - <requirements> with additional requirements
  - <directory> with the generated directory name

Additionally, the tool:
  - Creates a JOURNAL file pre-populated with metadata and empty progress and entries
  - Copies Makefile.template to the new directory as Makefile if present

Examples:
  santa-bootstrap                                                    # Interactive mode
  santa-bootstrap --lang Rust --harness Cursor --model GPT-4o        # Non-interactive
  santa-bootstrap --lang Python --harness Claude Code --model Sonnet 4 --requirements "Using custom parser" --force
"#)]
struct Args {
    #[arg(short, long, help = "Language to implement (e.g., Python, Rust, Go)")]
    lang: Option<String>,

    #[arg(long, help = "Harness/agent (e.g., Claude Code, Codex, Cursor)")]
    harness: Option<String>,

    #[arg(short, long, help = "LLM model being used (e.g., GPT-4o, GPT-5, Sonnet 4)")]
    model: Option<String>,

    #[arg(short, long, help = "Additional requirements to include (optional)")]
    requirements: Option<String>,

    #[arg(short, long, help = "Force overwrite existing files")]
    force: bool,
}

fn print_usage_and_exit() -> ! {
    eprintln!(
        "Usage: santa-bootstrap [--lang <lang>] [--harness <harness>] [--model <model>] [--requirements <text>] [--force]\n\
         Interactive by default. When flags are provided, runs non-interactively.\n\
         Creates impl/<lang>-<harness>-<model>-<unixtimestamp>/ and generates TASKS.md from specs/TASKS.md\n\
         replacing <lang>, <harness>, <model>, <requirements>, and <directory> tokens."
    );
    std::process::exit(2);
}

fn ask_question(prompt: &str) -> Result<String, Box<dyn std::error::Error>> {
    print!("{}", prompt);
    io::stdout().flush()?;
    
    let mut input = String::new();
    io::stdin().read_line(&mut input)?;
    Ok(input.trim().to_string())
}

fn sanitize_segment(value: &str) -> String {
    if value.is_empty() {
        return String::new();
    }
    let mut out = String::new();
    for c in value.trim().chars() {
        match c {
            '#' => out.push_str("sharp"),
            _ if c.is_alphanumeric() => out.push(c.to_ascii_lowercase()),
            _ => { /* drop whitespace and other punctuation */ }
        }
    }
    out
}

fn unix_timestamp_seconds() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("Time went backwards")
        .as_secs()
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = Args::parse();
    
    // Interactive prompts if values not provided
    if args.lang.is_none() {
        let lang = ask_question("Language to implement (e.g., Python, Ruby, Rust, Go, F#, C++, C#): ")?;
        if lang.is_empty() {
            print_usage_and_exit();
        }
        args.lang = Some(lang);
    }
    
    if args.harness.is_none() {
        let harness = ask_question("Harness/agent (e.g., Claude Code, Codex, Cursor, Amp): ")?;
        if harness.is_empty() {
            print_usage_and_exit();
        }
        args.harness = Some(harness);
    }

    if args.model.is_none() {
        let model = ask_question("LLM model you will use (e.g., Sonnet 4, Opus 4.1, GPT-5, GPT-4o): ")?;
        if model.is_empty() {
            print_usage_and_exit();
        }
        args.model = Some(model);
    }
    
    if args.requirements.is_none() {
        let req = ask_question("Additional requirements (optional; extra notes about the desired implementation): ")?;
        args.requirements = Some(req);
    }
    
    let lang = args.lang.as_ref().unwrap();
    let harness = args.harness.as_ref().unwrap();
    let model = args.model.as_ref().unwrap();
    let requirements = args.requirements.as_deref().unwrap_or("");
    
    if lang.is_empty() || harness.is_empty() || model.is_empty() {
        print_usage_and_exit();
    }
    
    let sanitized_lang = sanitize_segment(lang);
    let sanitized_harness = sanitize_segment(harness);
    let sanitized_model = sanitize_segment(model);
    let ts = unix_timestamp_seconds();
    
    // Find repository root (current directory should be tools)
    let current_dir = std::env::current_dir()?;
    let repo_root = if current_dir.ends_with("tools") {
        current_dir.parent().unwrap()
    } else {
        current_dir.as_path()
    };
    
    let template_path = repo_root.join("specs").join("TASKS.md");
    let makefile_template_path = repo_root.join("Makefile.template");
    let impl_folder_name = format!("{}-{}-{}-{}", sanitized_lang, sanitized_harness, sanitized_model, ts);
    let target_dir = repo_root.join("impl").join(&impl_folder_name);
    let tasks_path = target_dir.join("TASKS.md");
    let makefile_target_path = target_dir.join("Makefile");
    let journal_path = target_dir.join("JOURNAL");
    
    if !template_path.exists() {
        eprintln!("Error: Missing template at {}", template_path.display());
        std::process::exit(1);
    }
    
    fs::create_dir_all(&target_dir)?;
    
    if tasks_path.exists() && !args.force {
        eprintln!(
            "Error: {} already exists. Re-run with --force to overwrite.",
            tasks_path.display()
        );
        std::process::exit(1);
    }
    
    let template = fs::read_to_string(&template_path)?;
    // Keep pretty values in the TASKS.md Details section as Key: Value verbatim
    let replaced = template
        .replace("<lang>", lang)
        .replace("<harness>", harness)
        .replace("<model>", model)
        .replace("<requirements>", requirements)
        .replace("<directory>", &impl_folder_name);

    // If requirements not provided, remove the entire Requirements line
    let processed = if requirements.trim().is_empty() {
        let filtered: String = replaced
            .lines()
            .filter(|line| {
                let trimmed = line.trim();
                // Drop lines that are exactly "Requirements:" (optionally with trailing spaces)
                !(trimmed == "Requirements:" || trimmed == "Requirements: <requirements>")
            })
            .collect::<Vec<_>>()
            .join("\n");
        filtered
    } else {
        replaced
    };
    
    fs::write(&tasks_path, processed)?;
    
    // Create JOURNAL file with requested JSON structure
    if !journal_path.exists() || args.force {
        let journal_json = json!({
            "author": "",
            "details": {
                "language": lang,
                "model": model,
                "harness": harness,
                "requirements": requirements
            },
            "progress": {
                "stage-1": "not-started",
                "stage-2": "not-started",
                "stage-3": "not-started",
                "stage-4": "not-started",
                "stage-5": "not-started"
            },
            "journal": []
        });
        let journal_contents = serde_json::to_string_pretty(&journal_json)?;
        fs::write(&journal_path, journal_contents)?;
        println!("Created: {}", journal_path.display());
    }
    
    // Best-effort: copy Makefile.template into the new implementation directory as Makefile
    if makefile_template_path.exists() {
        match std::fs::copy(&makefile_template_path, &makefile_target_path) {
            Ok(_) => {
                println!("Created: {}", makefile_target_path.display());
            }
            Err(e) => {
                eprintln!(
                    "Warning: failed to copy {} to {}: {}",
                    makefile_template_path.display(),
                    makefile_target_path.display(),
                    e
                );
            }
        }
    } else {
        eprintln!("Warning: missing Makefile.template at {}", makefile_template_path.display());
    }

    println!("Created: {}", tasks_path.display());
    
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sanitize_segment() {
        assert_eq!(sanitize_segment(""), "");
        assert_eq!(sanitize_segment("Python"), "python");
        assert_eq!(sanitize_segment("GPT-4o"), "gpt4o");
        assert_eq!(sanitize_segment("Sonnet 4"), "sonnet4");
        assert_eq!(sanitize_segment("C++"), "c");
        assert_eq!(sanitize_segment("node.js"), "nodejs");
        assert_eq!(sanitize_segment("  Rust  "), "rust");
        assert_eq!(sanitize_segment("test_123"), "test123");
        assert_eq!(sanitize_segment("Claude Code"), "claudecode");
        assert_eq!(sanitize_segment("C#"), "csharp");
        assert_eq!(sanitize_segment("F#"), "fsharp");
    }

    #[test]
    fn test_unix_timestamp_seconds() {
        let ts = unix_timestamp_seconds();
        // Should be a reasonable timestamp (after 2020)
        assert!(ts > 1577836800); // Jan 1, 2020
    }
}
