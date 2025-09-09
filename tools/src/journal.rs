use clap::{Parser, Subcommand, Args as ClapArgs, ValueEnum};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};
use chrono::{SecondsFormat, Utc};

#[derive(Debug, Serialize, Deserialize)]
struct JournalFile {
    author: String,
    details: Details,
    progress: Progress,
    #[serde(rename = "journal")]
    journal: Vec<JournalEntry>,
}

#[derive(Debug, Serialize, Deserialize)]
struct Details {
    language: String,
    model: String,
    harness: String,
    requirements: String,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
struct Progress {
    #[serde(rename = "stage-1")] stage_1: String,
    #[serde(rename = "stage-2")] stage_2: String,
    #[serde(rename = "stage-3")] stage_3: String,
    #[serde(rename = "stage-4")] stage_4: String,
    #[serde(rename = "stage-5")] stage_5: String,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
struct JournalEntry {
    written_at: String,
    entry: String,
}

#[derive(Parser)]
#[command(name = "santa-journal", about = "Interact with an implementation's JOURNAL file")]
#[command(version = "0.1.0")]
#[command(long_about = r#"
santa-journal inspects and updates the JSON JOURNAL file in an implementation directory.

By default, it targets the most recently modified implementation directory under impl/ that contains a JOURNAL file.
You can override this with --dir.

Subcommands:
  - author           Show the current author, or set it once
  - progress         Show all stage statuses, show one stage, or set a stage status
  - entry            Append a free-form journal entry with a timestamp
  - entries          List all entries in reverse chronological order

Examples:
  santa-journal author
  santa-journal author set "Hermey the Elf"
  santa-journal progress
  santa-journal progress stage-2
  santa-journal progress stage-3 set in-progress
  santa-journal entry "Finished stage-1 lexer"
  santa-journal entries
"#)]
struct Cli {
    /// Path to the implementation directory (containing JOURNAL). Defaults to latest under impl/.
    #[arg(short, long, help = "Path to implementation dir (with JOURNAL). Defaults to newest under impl/.")]
    dir: Option<PathBuf>,

    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Show or set the author
    Author(AuthorCmd),
    /// Show or set progress for a stage, or show all
    Progress(ProgressCmd),
    /// Append a journal entry
    Entry(EntryCmd),
    /// List entries in reverse chronological order
    Entries,
}

#[derive(ClapArgs)]
struct AuthorCmd {
    #[command(subcommand)]
    sub: Option<AuthorSub>,
}

#[derive(Subcommand)]
enum AuthorSub {
    /// Set the author (fails if already set)
    Set { name: String },
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum, Debug)]
enum Stage {
    #[value(name = "stage-1")] Stage1,
    #[value(name = "stage-2")] Stage2,
    #[value(name = "stage-3")] Stage3,
    #[value(name = "stage-4")] Stage4,
    #[value(name = "stage-5")] Stage5,
}

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum, Debug)]
enum StatusVal {
    #[value(name = "not-started")] NotStarted,
    #[value(name = "in-progress")] InProgress,
    #[value(name = "complete")] Complete,
}

#[derive(ClapArgs)]
struct ProgressCmd {
    /// Optional stage identifier (e.g. stage-1). If omitted, shows the table.
    #[arg(value_enum, help = "Optional stage identifier (e.g. stage-1) to show/set.")]
    stage: Option<Stage>,

    /// Optional action literal. Only 'set' is accepted when provided.
    #[arg(help = "Action to perform when changing status. Only 'set' is supported.")]
    action: Option<String>,

    /// New status to set when action is 'set'. One of: not-started, in-progress, complete
    #[arg(value_enum, help = "New status when action is 'set': not-started | in-progress | complete.")]
    status: Option<StatusVal>,
}

#[derive(ClapArgs)]
struct EntryCmd {
    #[arg(help = "Free-form text to append to JOURNAL with a timestamp.")]
    text: String,
}

fn resolve_impl_dir(explicit: &Option<PathBuf>) -> Result<PathBuf, String> {
    if let Some(dir) = explicit {
        return Ok(dir.clone());
    }
    let cwd = std::env::current_dir().map_err(|e| e.to_string())?;
    let repo_root = if cwd.ends_with("tools") { cwd.parent().unwrap().to_path_buf() } else { cwd.clone() };
    let impl_dir = repo_root.join("impl");
    let mut newest: Option<(PathBuf, std::time::SystemTime)> = None;
    for entry in fs::read_dir(&impl_dir).map_err(|e| format!("Failed to read {}: {}", impl_dir.display(), e))? {
        let entry = entry.map_err(|e| e.to_string())?;
        let meta = entry.metadata().map_err(|e| e.to_string())?;
        if meta.is_dir() {
            let modified = meta.modified().unwrap_or(std::time::SystemTime::UNIX_EPOCH);
            let path = entry.path();
            if path.join("JOURNAL").exists() {
                if let Some((_, m)) = &newest {
                    if modified > *m { newest = Some((path, modified)); }
                } else {
                    newest = Some((path, modified));
                }
            }
        }
    }
    newest.map(|(p, _)| p).ok_or_else(|| "Could not locate an implementation directory with a JOURNAL".to_string())
}

fn read_journal(dir: &Path) -> Result<JournalFile, String> {
    let path = dir.join("JOURNAL");
    let data = fs::read_to_string(&path).map_err(|e| format!("Failed to read {}: {}", path.display(), e))?;
    serde_json::from_str(&data).map_err(|e| format!("Failed to parse {}: {}", path.display(), e))
}

fn write_journal(dir: &Path, jf: &JournalFile) -> Result<(), String> {
    let path = dir.join("JOURNAL");
    let s = serde_json::to_string_pretty(jf).map_err(|e| e.to_string())?;
    fs::write(&path, s).map_err(|e| format!("Failed to write {}: {}", path.display(), e))
}

fn stage_get<'a>(p: &'a Progress, s: Stage) -> &'a str {
    match s {
        Stage::Stage1 => &p.stage_1,
        Stage::Stage2 => &p.stage_2,
        Stage::Stage3 => &p.stage_3,
        Stage::Stage4 => &p.stage_4,
        Stage::Stage5 => &p.stage_5,
    }
}

fn stage_set(p: &mut Progress, s: Stage, v: &str) {
    match s {
        Stage::Stage1 => p.stage_1 = v.to_string(),
        Stage::Stage2 => p.stage_2 = v.to_string(),
        Stage::Stage3 => p.stage_3 = v.to_string(),
        Stage::Stage4 => p.stage_4 = v.to_string(),
        Stage::Stage5 => p.stage_5 = v.to_string(),
    }
}

fn status_to_str(s: StatusVal) -> &'static str {
    match s { StatusVal::NotStarted => "not-started", StatusVal::InProgress => "in-progress", StatusVal::Complete => "complete" }
}

fn print_progress_table(p: &Progress) {
    println!("Stage     Status");
    println!("--------  -----------");
    println!("stage-1  {}", p.stage_1);
    println!("stage-2  {}", p.stage_2);
    println!("stage-3  {}", p.stage_3);
    println!("stage-4  {}", p.stage_4);
    println!("stage-5  {}", p.stage_5);
}

fn main() -> Result<(), String> {
    let cli = Cli::parse();
    let dir = resolve_impl_dir(&cli.dir)?;

    match cli.command {
        Commands::Author(cmd) => {
            let mut jf = read_journal(&dir)?;
            match cmd.sub {
                Some(AuthorSub::Set { name }) => {
                    if !jf.author.trim().is_empty() {
                        return Err("Author is already set. Use the existing value or edit JOURNAL manually if needed.".to_string());
                    }
                    jf.author = name;
                    write_journal(&dir, &jf)?;
                    println!("Author set.");
                }
                None => {
                    if jf.author.trim().is_empty() {
                        println!("Author is not set yet. Use: santa-journal author set \"<elf-name>\"");
                    } else {
                        println!("Author: {}", jf.author);
                    }
                }
            }
        }
        Commands::Progress(cmd) => {
            let mut jf = read_journal(&dir)?;
            match (&cmd.stage, &cmd.action, &cmd.status) {
                // no args => table
                (None, _, _) => {
                    print_progress_table(&jf.progress);
                }
                // one arg (stage) => show that stage
                (Some(stage), None, _) => {
                    let val = stage_get(&jf.progress, *stage);
                    let key = match stage { Stage::Stage1 => "stage-1", Stage::Stage2 => "stage-2", Stage::Stage3 => "stage-3", Stage::Stage4 => "stage-4", Stage::Stage5 => "stage-5" };
                    println!("{}: {}", key, val);
                }
                // three args: <stage> set <status>
                (Some(stage), Some(action), Some(status)) => {
                    if action != "set" {
                        return Err(format!("Unknown action '{}'. Did you mean 'set'?", action));
                    }
                    stage_set(&mut jf.progress, *stage, status_to_str(*status));
                    write_journal(&dir, &jf)?;
                    println!("Updated.");
                }
                // invalid combinations
                (Some(_), Some(action), None) => {
                    return Err(format!("Action '{}' requires a status: not-started | in-progress | complete", action));
                }
            }
        }
        Commands::Entry(cmd) => {
            let mut jf = read_journal(&dir)?;
            let now = Utc::now().to_rfc3339_opts(SecondsFormat::Secs, true);
            jf.journal.push(JournalEntry { written_at: now, entry: cmd.text });
            write_journal(&dir, &jf)?;
            println!("Entry added.");
        }
        Commands::Entries => {
            let mut jf = read_journal(&dir)?;
            jf.journal.sort_by(|a, b| b.written_at.cmp(&a.written_at));
            if jf.journal.is_empty() {
                println!("No entries yet.");
            } else {
                for e in &jf.journal {
                    println!("- {}\n  {}\n", e.written_at, e.entry);
                }
            }
        }
    }

    Ok(())
}
