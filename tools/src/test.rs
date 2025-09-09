use clap::Parser;
use rayon::prelude::*;
use similar::{ChangeTag, TextDiff};
use std::collections::HashMap;
use std::fs;
use std::env;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::Duration;
use tempfile::TempDir;
use wait_timeout::ChildExt;

#[derive(Parser)]
#[command(name = "santa-test")]
#[command(about = "A test runner for Santa language .santat test files")]
#[command(version = "0.1.0")]
#[command(long_about = r#"
santa-test is a comprehensive test runner for Santa language .santat test files.

It parses .santat files containing test definitions with sections like:
  --FILE--        The Santa source code to test
  --EXPECT--      Expected standard output
  --EXPECT_AST--  Expected AST representation
  --EXPECT_TOKENS-- Expected token stream
  --TEST--        Test description (optional)

The tool runs the Santa compiler/interpreter with different modes and compares
the actual output against expected results, showing detailed diffs for failures.

Examples:
  santa-test --bin ./cli tests/
  santa-test --bin docker://edd/santa-go:cli tests/
  santa-test --bin ./cli --update test.santat
  santa-test --bin ./cli --timeout 10 tests/basic/
  santa-test --bin ./cli --jobs 4 tests/    # Use 4 parallel jobs

Docker notes:
  When using a docker:// image via --bin, the current working directory is mounted
  read-write into the container at the same path, and the working directory is set
  accordingly. The container is started with -i (and -t if stdout is a TTY). You can
  pass additional docker run flags via the SANTA_DOCKER_FLAGS environment variable,
  e.g. SANTA_DOCKER_FLAGS="--network host --cpus 2" santa-test --bin docker://image:tag tests/
"#)]
struct Args {
    #[arg(short, long, help = "Path to the Santa CLI executable or docker image URI (docker://image:tag)")]
    bin: String,

    #[arg(long, help = "Disable colored output")]
    no_color: bool,

    #[arg(long, help = "Update test files with actual output on failure")]
    update: bool,

    #[arg(short, long, default_value = "5", help = "Timeout in seconds for each test execution")]
    timeout: u64,

    #[arg(short, long, help = "Number of parallel jobs (0 = auto-detect CPU count)")]
    jobs: Option<usize>,

    #[arg(help = "Test files or directories to run", required = true)]
    targets: Vec<PathBuf>,
}

/// How to run the target CLI (either a host path or a docker image)
#[derive(Debug, Clone)]
enum Runner {
    Local(PathBuf),
    Docker {
        image: String,
        /// Extra flags to pass to `docker run` (space separated), taken from env SANTA_DOCKER_FLAGS
        extra: Vec<String>,
    },
}

impl Runner {
    fn parse(bin: &str) -> Self {
        const PREFIX: &str = "docker://";
        if let Some(rest) = bin.strip_prefix(PREFIX) {
            let image = rest.to_string();
            let extra = env::var("SANTA_DOCKER_FLAGS")
                .unwrap_or_default()
                .split_whitespace()
                .map(|s| s.to_string())
                .collect();
            Runner::Docker { image, extra }
        } else {
            Runner::Local(PathBuf::from(bin))
        }
    }

    /// Prepare a `Command` that runs the implementation with the provided argv.
    /// - mounts CWD into the container at the same path
    /// - sets workdir to CWD
    /// - keeps stdin attached (-i) and allocates TTY if stdout is a tty
    fn command(&self, argv: &[String]) -> Command {
        match self {
            Runner::Local(bin) => {
                let mut cmd = Command::new(bin);
                cmd.args(argv);
                cmd
            }
            Runner::Docker { image, extra } => {
                let cwd = env::current_dir().expect("cwd");
                let cwd_str = cwd.to_string_lossy().to_string();

                let mut cmd = Command::new("docker");
                cmd.arg("run")
                    .arg("--rm")
                    .arg("-i");

                if atty::is(atty::Stream::Stdout) {
                    cmd.arg("-t");
                }

                let uid = users::get_current_uid();
                let gid = users::get_current_gid();
                cmd.args(["-e", &format!("HOST_UID={}", uid)])
                    .args(["-e", &format!("HOST_GID={}", gid)]);

                for f in extra {
                    cmd.arg(f);
                }

                cmd.args(["-v", &format!("{0}:{0}", cwd_str)])
                    .args(["-w", &cwd_str])
                    .arg(image)
                    .args(argv);
                cmd
            }
        }
    }
}

#[derive(Debug)]
struct TestBlock {
    name: String,
    content: String,
}

#[derive(Debug)]
struct SantatFile {
    blocks: Vec<TestBlock>,
    map: HashMap<String, String>,
}

#[derive(Debug)]
struct TestResult {
    passed: bool,
    output: String,
}

struct Colors {
    red: fn(&str) -> String,
    green: fn(&str) -> String,
    cyan: fn(&str) -> String,
    bold: fn(&str) -> String,
}

impl Colors {
    fn new(use_color: bool) -> Self {
        if use_color {
            Colors {
                red: |s| format!("\x1b[31m{}\x1b[0m", s),
                green: |s| format!("\x1b[32m{}\x1b[0m", s),
                cyan: |s| format!("\x1b[36m{}\x1b[0m", s),
                bold: |s| format!("\x1b[1m{}\x1b[0m", s),
            }
        } else {
            Colors {
                red: |s| s.to_string(),
                green: |s| s.to_string(),
                cyan: |s| s.to_string(),
                bold: |s| s.to_string(),
            }
        }
    }
}

fn normalize_newlines(s: &str) -> String {
    s.replace("\r\n", "\n")
}

fn parse_santat_file(content: &str) -> Result<SantatFile, Box<dyn std::error::Error>> {
    let content = normalize_newlines(content);
    let lines: Vec<&str> = content.lines().collect();
    
    let section_re = regex::Regex::new(r"^--([A-Z_]+)--\s*$")?;
    
    let mut blocks = Vec::new();
    let mut current_name: Option<String> = None;
    let mut buffer = Vec::new();

    for line in lines {
        if let Some(captures) = section_re.captures(line) {
            if let Some(name) = current_name {
                blocks.push(TestBlock {
                    name,
                    content: buffer.join("\n"),
                });
            }
            current_name = Some(captures[1].to_string());
            buffer.clear();
        } else {
            buffer.push(line);
        }
    }

    if let Some(name) = current_name {
        blocks.push(TestBlock {
            name,
            content: buffer.join("\n"),
        });
    }

    let map: HashMap<String, String> = blocks
        .iter()
        .map(|b| (b.name.clone(), b.content.clone()))
        .collect();

    if !map.contains_key("FILE") {
        return Err("Missing required --FILE-- section".into());
    }

    Ok(SantatFile { blocks, map })
}

fn stringify_santat_blocks(blocks: &[TestBlock]) -> String {
    blocks
        .iter()
        .map(|b| format!("--{}--\n{}", b.name, b.content))
        .collect::<Vec<_>>()
        .join("\n")
}

fn create_unified_diff(expected: &str, actual: &str, expected_label: &str, actual_label: &str, colors: &Colors) -> String {
    let diff = TextDiff::from_lines(expected, actual);
    let mut result = Vec::new();
    
    result.push(format!("--- {}", expected_label));
    result.push(format!("+++ {}", actual_label));
    
    for change in diff.iter_all_changes() {
        let sign = match change.tag() {
            ChangeTag::Delete => (colors.red)("-"),
            ChangeTag::Insert => (colors.green)("+"),
            ChangeTag::Equal => " ".to_string(),
        };
        result.push(format!("{}{}", sign, change.value().trim_end()));
    }
    
    result.join("\n")
}

fn run_command(runner: &Runner, args: &[String], timeout_secs: u64) -> Result<(i32, String, String, bool), Box<dyn std::error::Error>> {
    let mut cmd = runner.command(args);
    cmd.stdout(Stdio::piped())
        .stderr(Stdio::piped());

    let mut child = cmd.spawn()?;
    
    match child.wait_timeout(Duration::from_secs(timeout_secs))? {
        Some(status) => {
            let output = child.wait_with_output()?;
            let stdout = String::from_utf8_lossy(&output.stdout).trim_end().to_string();
            let stderr = String::from_utf8_lossy(&output.stderr).trim_end().to_string();
            Ok((status.code().unwrap_or(-1), stdout, stderr, false))
        }
        None => {
            let _ = child.kill();
            let _ = child.wait();
            Ok((-1, String::new(), String::new(), true))
        }
    }
}

fn collect_santat_files(target: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    
    if target.is_file() && target.extension().map_or(false, |ext| ext == "santat") {
        files.push(target.to_path_buf());
    } else if target.is_dir() {
        collect_santat_files_recursive(target, &mut files);
    }
    
    files.sort();
    files
}

fn collect_santat_files_recursive(dir: &Path, files: &mut Vec<PathBuf>) {
    if let Ok(entries) = fs::read_dir(dir) {
        let mut entries: Vec<_> = entries.filter_map(|e| e.ok()).collect();
        entries.sort_by_key(|e| e.file_name());
        
        for entry in entries {
            let path = entry.path();
            if path.is_dir() {
                collect_santat_files_recursive(&path, files);
            } else if path.extension().map_or(false, |ext| ext == "santat") {
                files.push(path);
            }
        }
    }
}

fn update_santat_file_on_failures(
    path: &Path, 
    failures: &HashMap<&str, &str>
) -> Result<bool, Box<dyn std::error::Error>> {
    let original = fs::read_to_string(path)?;
    let mut santat = parse_santat_file(&original)?;
    
    let section_map = [
        ("expect", "EXPECT"),
        ("expect_ast", "EXPECT_AST"), 
        ("expect_tokens", "EXPECT_TOKENS"),
    ];
    
    let mut changed = false;
    
    for (key, actual) in failures {
        if let Some((_, section)) = section_map.iter().find(|(k, _)| k == key) {
            for block in &mut santat.blocks {
                if block.name == *section {
                    let normalized_actual = normalize_newlines(actual);
                    if normalize_newlines(&block.content) != normalized_actual {
                        block.content = normalized_actual;
                        changed = true;
                    }
                }
            }
        }
    }
    
    if changed {
        fs::write(path, stringify_santat_blocks(&santat.blocks))?;
    }
    
    Ok(changed)
}

fn run_one_test_parallel(runner: &Runner, test_path: &Path, timeout_secs: u64, do_update: bool) -> TestResult {
    let mut output_lines = Vec::new();
    
    let raw_content = match fs::read_to_string(test_path) {
        Ok(content) => content,
        Err(e) => {
            output_lines.push(format!("Error reading {}: {}", test_path.display(), e));
            return TestResult {
                passed: false,
                output: output_lines.join("\n"),
            };
        }
    };
    
    let santat = match parse_santat_file(&raw_content) {
        Ok(s) => s,
        Err(e) => {
            output_lines.push(format!("Error parsing {}: {}", test_path.display(), e));
            return TestResult {
                passed: false,
                output: output_lines.join("\n"),
            };
        }
    };
    
    let title = santat.map.get("TEST")
        .map(|s| s.trim())
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| test_path.file_name().unwrap().to_str().unwrap());
    
    output_lines.push(format!("• {} ({})", title, test_path.display()));
    
    let temp_dir = match (|| -> std::io::Result<TempDir> {
        let cwd = env::current_dir()?;
        TempDir::new_in(cwd)
    })() {
        Ok(dir) => dir,
        Err(e) => {
            output_lines.push(format!("Error creating temp dir: {}", e));
            return TestResult {
                passed: false,
                output: output_lines.join("\n"),
            };
        }
    };
    
    let temp_file = temp_dir.path().join("test.santa");
    if let Err(e) = fs::write(&temp_file, santat.map.get("FILE").unwrap_or(&String::new())) {
        output_lines.push(format!("Error writing temp file: {}", e));
        return TestResult {
            passed: false,
            output: output_lines.join("\n"),
        };
    }
    
    let mut any_fail = false;
    let mut actuals = HashMap::new();
    let mut failed = HashMap::new();
    
    let checks = [
        ("output", "EXPECT", vec![temp_file.to_str().unwrap()]),
        ("ast", "EXPECT_AST", vec!["ast", temp_file.to_str().unwrap()]),
        ("tokens", "EXPECT_TOKENS", vec!["tokens", temp_file.to_str().unwrap()]),
    ];
    
    for (kind, expect_key, args) in checks {
        if !santat.map.contains_key(expect_key) {
            continue;
        }
        
        let expected = santat.map.get(expect_key).unwrap();
        let args_vec: Vec<String> = args.iter().map(|s| s.to_string()).collect();
        
        match run_command(runner, &args_vec, timeout_secs) {
            Ok((_, stdout, _, timed_out)) => {
                if timed_out {
                    output_lines.push(format!("    ✗ {} timed out after {}s", kind, timeout_secs));
                    any_fail = true;
                    continue;
                }
                
                actuals.insert(expect_key, stdout.clone());
                
                if normalize_newlines(expected) == normalize_newlines(&stdout) {
                    output_lines.push(format!("    ✓ {} matches", kind));
                } else {
                    output_lines.push(format!("    ✗ {} differs", kind));
                    let diff = create_unified_diff(
                        expected, 
                        &stdout, 
                        &format!("{} expected", kind),
                        &format!("{} actual", kind),
                        &Colors::new(false) // Use no-color for stored output
                    );
                    output_lines.push(diff);
                    any_fail = true;
                    failed.insert(expect_key, true);
                }
            }
            Err(e) => {
                output_lines.push(format!("    ✗ {} failed: {}", kind, e));
                any_fail = true;
            }
        }
    }
    
    if do_update && any_fail {
        let mut changes = HashMap::new();
        if failed.contains_key("EXPECT") {
            changes.insert("expect", actuals.get("EXPECT").map(|s| s.as_str()).unwrap_or(""));
        }
        if failed.contains_key("EXPECT_AST") {
            changes.insert("expect_ast", actuals.get("EXPECT_AST").map(|s| s.as_str()).unwrap_or(""));
        }
        if failed.contains_key("EXPECT_TOKENS") {
            changes.insert("expect_tokens", actuals.get("EXPECT_TOKENS").map(|s| s.as_str()).unwrap_or(""));
        }
        
        match update_santat_file_on_failures(test_path, &changes) {
            Ok(true) => {
                output_lines.push(format!("  UPDATED {}", test_path.file_name().unwrap().to_str().unwrap()));
            }
            Ok(false) => {}
            Err(e) => {
                output_lines.push(format!("Error updating file: {}", e));
            }
        }
    }
    
    if !any_fail {
        output_lines.push("  PASS".to_string());
    } else {
        output_lines.push("  FAIL".to_string());
    }
    
    TestResult { passed: !any_fail, output: output_lines.join("\n") }
}

// note: a non-parallel version existed earlier but was unused; the parallel path prints results coherently

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();
    let runner = Runner::parse(&args.bin);
    
    let use_color = !args.no_color && atty::is(atty::Stream::Stdout);
    let colors = Colors::new(use_color);
    
    // Set up parallelism
    if let Some(jobs) = args.jobs {
        if jobs > 0 {
            rayon::ThreadPoolBuilder::new()
                .num_threads(jobs)
                .build_global()?;
        }
        // If jobs == 0, let rayon auto-detect CPU count
    }
    
    let mut all_files = Vec::new();
    for target in &args.targets {
        all_files.extend(collect_santat_files(target));
    }
    
    if all_files.is_empty() {
        eprintln!("No .santat files found in the provided paths.");
        std::process::exit(2);
    }
    
    // Run tests in parallel
    let results: Vec<TestResult> = all_files
        .par_iter()
        .map(|file| run_one_test_parallel(&runner, file, args.timeout, args.update))
        .collect();
    
    // Output results in original order with colors
    let mut pass_count = 0;
    for result in &results {
        // Apply colors to the output for display
        let colored_output = if use_color {
            result.output
                .replace("✓", &(colors.green)("✓"))
                .replace("✗", &(colors.red)("✗"))
                .replace("PASS", &(colors.green)("PASS"))
                .replace("FAIL", &(colors.red)("FAIL"))
                .replace("UPDATED", &(colors.cyan)("UPDATED"))
        } else {
            result.output.clone()
        };
        
        println!("{}", colored_output);
        println!(); // Add spacing between tests
        
        if result.passed {
            pass_count += 1;
        }
    }
    
    let fail_count = all_files.len() - pass_count;
    println!("{}: {}/{} passing, {} failing",
        (colors.bold)("Summary"),
        pass_count,
        all_files.len(),
        fail_count
    );
    
    std::process::exit(if fail_count == 0 { 0 } else { 1 });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_santat_file() {
        let content = r#"--FILE--
print("hello world")

--EXPECT--
hello world

--TEST--
Simple hello world test"#;

        let result = parse_santat_file(content).unwrap();
        assert_eq!(result.blocks.len(), 3);
        assert!(result.map.contains_key("FILE"));
        assert!(result.map.contains_key("EXPECT"));
        assert!(result.map.contains_key("TEST"));
        assert_eq!(result.map["FILE"], "print(\"hello world\")\n");
        assert_eq!(result.map["EXPECT"], "hello world\n");
        assert_eq!(result.map["TEST"], "Simple hello world test");
    }

    #[test]
    fn test_parse_santat_file_missing_file_section() {
        let content = r#"--EXPECT--
hello world"#;

        let result = parse_santat_file(content);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Missing required --FILE-- section"));
    }

    #[test]
    fn test_normalize_newlines() {
        assert_eq!(normalize_newlines("hello\r\nworld"), "hello\nworld");
        assert_eq!(normalize_newlines("hello\nworld"), "hello\nworld");
        assert_eq!(normalize_newlines("hello world"), "hello world");
    }

    #[test]
    fn test_stringify_santat_blocks() {
        let blocks = vec![
            TestBlock {
                name: "FILE".to_string(),
                content: "print(\"test\")".to_string(),
            },
            TestBlock {
                name: "EXPECT".to_string(),
                content: "test".to_string(),
            },
        ];

        let result = stringify_santat_blocks(&blocks);
        assert_eq!(result, "--FILE--\nprint(\"test\")\n--EXPECT--\ntest");
    }

    #[test]
    fn test_colors_with_color() {
        let colors = Colors::new(true);
        let red_text = (colors.red)("error");
        assert!(red_text.contains("\x1b[31m"));
        assert!(red_text.contains("\x1b[0m"));
    }

    #[test]
    fn test_colors_without_color() {
        let colors = Colors::new(false);
        let red_text = (colors.red)("error");
        assert_eq!(red_text, "error");
    }

    #[test]
    fn test_collect_santat_files_empty() {
        use tempfile::TempDir;
        let temp_dir = TempDir::new().unwrap();
        let files = collect_santat_files(temp_dir.path());
        assert!(files.is_empty());
    }

    #[test]
    fn test_collect_santat_files_single_file() {
        use tempfile::TempDir;
        let temp_dir = TempDir::new().unwrap();
        let test_file = temp_dir.path().join("test.santat");
        std::fs::write(&test_file, "test content").unwrap();
        
        let files = collect_santat_files(&test_file);
        assert_eq!(files.len(), 1);
        assert_eq!(files[0], test_file);
    }
}
