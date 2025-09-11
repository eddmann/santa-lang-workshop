use clap::Parser;
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use pulldown_cmark::{Parser as MdParser, Options as MdOptions, html as md_html};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

#[derive(Parser)]
#[command(name = "santa-site", about = "Generate a static website for elf-lang implementations")]
#[command(version = "0.1.0")]
#[command(long_about = "Generates a static website showcasing elf-lang implementations. 
The tool reads implementation directories, processes journal entries, and creates 
a beautiful website with filtering, code browsing, and responsive design.

The generated site includes:
- Homepage with implementation showcase and filtering
- Language reference page from specs/LANG.md
- Tasks page from specs/TASKS.md  
- Individual implementation pages with journal entries and code browser
- Responsive design with dark theme and festive styling

Perfect for GitHub Pages deployment with configurable base paths.")]
struct Args {
    /// Output directory for the generated static site
    /// 
    /// Defaults to 'docs/' in the repository root. All HTML files, assets, and 
    /// subdirectories will be created here. This directory can be deployed 
    /// directly to any static hosting service.
    #[arg(long)]
    out_dir: Option<PathBuf>,

    /// Directory containing elf-lang implementations
    /// 
    /// Each subdirectory should contain a JOURNAL file with implementation 
    /// metadata and progress. Defaults to 'impl/' in the repository root.
    #[arg(long)]
    impl_dir: Option<PathBuf>,

    /// Base URL path for hosting under a subdirectory
    /// 
    /// Use this when deploying to GitHub Pages under a project repository 
    /// (e.g., '/santa-lang-workshop'). Leave empty for user/organization 
    /// sites (*.github.io). All internal links and assets will be prefixed 
    /// with this path.
    #[arg(long)]
    base_path: Option<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
struct JournalFile {
    author: String,
    details: Details,
    progress: Progress,
    #[serde(rename = "journal")]
    journal: Vec<JournalEntry>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
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

#[derive(Debug, Clone)]
struct ImplInfo {
    dir_name: String,
    abs_path: PathBuf,
    elf_png_path: Option<PathBuf>,
    journal: JournalFile,
}

fn repo_root() -> PathBuf {
    let cwd = std::env::current_dir().expect("cwd");
    if cwd.ends_with("tools") { cwd.parent().unwrap().to_path_buf() } else { cwd }
}

fn read_impls(impl_dir: &Path) -> Result<Vec<ImplInfo>, String> {
    let mut list = Vec::new();
    let entries = fs::read_dir(impl_dir).map_err(|e| format!("Failed to read {}: {}", impl_dir.display(), e))?;
    for ent in entries.flatten() {
        let path = ent.path();
        if !path.is_dir() { continue; }
        let journal_path = path.join("JOURNAL");
        if !journal_path.exists() { continue; }
        let data = fs::read_to_string(&journal_path)
            .map_err(|e| format!("Failed to read {}: {}", journal_path.display(), e))?;
        let jf: JournalFile = serde_json::from_str(&data)
            .map_err(|e| format!("Failed to parse {}: {}", journal_path.display(), e))?;

        let elf_candidate_paths = [
            path.join("elf.png"),
            path.join("ELF.png"),
            path.join("assets").join("elf.png"),
        ];
        let elf_png_path = elf_candidate_paths.into_iter().find(|p| p.exists());

        list.push(ImplInfo {
            dir_name: path.file_name().unwrap().to_string_lossy().to_string(),
            abs_path: path.clone(),
            elf_png_path,
            journal: jf,
        });
    }
    // Newer first by modified time
    list.sort_by_key(|ii| std::fs::metadata(&ii.abs_path).and_then(|m| m.modified()).ok());
    list.reverse();
    Ok(list)
}

fn ensure_dir(p: &Path) -> Result<(), String> {
    fs::create_dir_all(p).map_err(|e| format!("Failed to create {}: {}", p.display(), e))
}

fn write_file(path: &Path, content: &str) -> Result<(), String> {
    if let Some(parent) = path.parent() { ensure_dir(parent)?; }
    let mut f = fs::File::create(path).map_err(|e| format!("Failed to create {}: {}", path.display(), e))?;
    f.write_all(content.as_bytes()).map_err(|e| format!("Failed to write {}: {}", path.display(), e))
}

fn tailwind_head() -> String {
    // CDN Tailwind for simplicity; static hosting friendly
    // Includes a festive Google Font and basic base styles
    r#"<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<script>
  window.tailwind = window.tailwind || {};
  tailwind.config = {
    theme: {
      extend: {
        colors: {
          holly: '#064e3b',
          pine: '#065f46',
          candy: '#f43f5e',
          gold: '#f59e0b',
          snow: '#f0f9ff',
        },
        boxShadow: {
          glow: '0 10px 30px rgba(244,63,94,0.25)',
        }
      }
    }
  }
</script>
<script src="https://cdn.tailwindcss.com?plugins=typography"></script>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github-dark.min.css">
<script src="https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js"></script>
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap" rel="stylesheet">
<style>
  :root { --snow:#f0f9ff; --holly:#064e3b; --pine:#065f46; --candy:#f43f5e; --gold:#f59e0b; --header-h: 88px; }
  html { scroll-behavior: smooth; scroll-padding-top: var(--header-h); }
  .xmas-title { font-weight: 800; }
  .body-font { font-family: 'Inter', system-ui, -apple-system, Segoe UI, Roboto, Ubuntu, Cantarell, 'Helvetica Neue', Arial, 'Apple Color Emoji', 'Segoe UI Emoji'; }
  .paper { background: linear-gradient(180deg, rgba(255,255,255,0.9), rgba(255,255,255,0.95)); box-shadow: 0 10px 30px rgba(0,0,0,0.08); }
  .paper-dark { background: linear-gradient(180deg, rgba(20,24,35,0.9), rgba(15,20,32,0.95)); box-shadow: 0 10px 30px rgba(0,0,0,0.25); }
  .journal-line { border-left: 3px solid var(--gold); }
  .code { font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace; }
  .tab { border: 0; padding: 10px 16px; border-radius: 9999px; transition: background .15s ease, color .15s ease; color: rgba(255,255,255,0.85); cursor: pointer; font-weight: 600; }
  .tab:hover { background: rgba(255,255,255,0.08); color: #fff; }
  .tab-active { background: #ffffff; color: #0f172a; box-shadow: 0 1px 2px rgba(0,0,0,0.12); }
  .theme-green .tab-active { background:#10b981; color:#0b1220; }
  .journal-body { color: rgba(255,255,255,0.75); }
  h1, h2, h3, h4, h5, h6 { scroll-margin-top: var(--header-h); }
  .snow, .snow2 {
    pointer-events: none; position: fixed; inset: 0; top: var(--header-h); z-index: 0; opacity: .22;
    background-image: radial-gradient(2px 2px at 20px 30px, rgba(255,255,255,0.9) 2px, transparent 3px),
                      radial-gradient(3px 3px at 40px 70px, rgba(255,255,255,0.6) 2px, transparent 3px),
                      radial-gradient(2px 2px at 50px 160px, rgba(255,255,255,0.7) 2px, transparent 3px),
                      radial-gradient(3px 3px at 90px 40px, rgba(255,255,255,0.6) 2px, transparent 3px),
                      radial-gradient(2px 2px at 130px 80px, rgba(255,255,255,0.75) 2px, transparent 3px);
    background-size: 200px 200px;
    animation: snow 25s linear infinite;
  }
  @media (min-width: 1024px) { .sticky-toc { position: sticky; top: calc(var(--header-h) + 1rem); } }
  .snow2 { opacity: .18; animation-duration: 45s; background-size: 300px 300px; }
  @keyframes snow { from { background-position: 0 0, 0 0, 0 0, 0 0, 0 0; } to { background-position: 0 1000px, 0 800px, 0 600px, 0 400px, 0 200px; } }
</style>"#
        .to_string()
}

fn normalize_base_path(input: &str) -> String {
    let trimmed = input.trim();
    if trimmed.is_empty() || trimmed == "/" { return String::new(); }
    let mut s = trimmed.to_string();
    if !s.starts_with('/') { s.insert(0, '/'); }
    while s.ends_with('/') { s.pop(); }
    s
}

fn base_url(base: &str, path: &str) -> String {
    let base_norm = normalize_base_path(base);
    let p = if path.starts_with('/') { &path[1..] } else { path };
    if base_norm.is_empty() { format!("/{}", p) } else { format!("{}/{}", base_norm, p) }
}

fn layout_with_base(title: &str, body: &str, base_path: &str) -> String {
    let logo_src = base_url(base_path, "logo-light.png");
    let home_href = if normalize_base_path(base_path).is_empty() { "/".to_string() } else { format!("{}/", normalize_base_path(base_path)) };
    let lang_href = format!("{}/", base_url(base_path, "language"));
    let tasks_href = format!("{}/", base_url(base_path, "tasks"));
    format!(
        r#"<!doctype html>
<html lang="en" class="h-full">
<head>
  <title>{}</title>
  {}
</head>
<body class="min-h-screen body-font bg-gradient-to-b from-[#0B1220] via-[#0f1a2d] to-[#0B1220] text-white relative theme-green">
  <div class="snow"></div>
  <div class="snow2"></div>
  <header class="px-6 md:px-10 py-2 md:py-3 border-b border-white/10 backdrop-blur sticky top-0 bg-black/20 relative z-50">
    <div class="max-w-7xl mx-auto flex items-center gap-3">
      <a href="{home}" class="inline-flex items-center">
        <img src="{logo}" alt="elf-lang" class="h-12 md:h-14 w-auto"/>
      </a>
      <div class="ml-auto flex items-center gap-3">
        <nav class="hidden md:flex items-center gap-4">
          <a class="text-white/80 hover:text-white text-sm leading-none" href="{home}">Home</a>
          <a class="text-white/80 hover:text-white text-sm leading-none" href="{lang}">Language</a>
          <a class="text-white/80 hover:text-white text-sm leading-none" href="{tasks}">Tasks</a>
          <a class="text-white/80 hover:text-white text-sm leading-none" href="https://github.com/eddmann/santa-lang-workshop" target="_blank" rel="noopener noreferrer">GitHub</a>
        </nav>
        <button id="nav-toggle" class="md:hidden p-2 rounded bg-white/10 hover:bg-white/20 text-white/80" aria-expanded="false" aria-controls="mobile-nav" aria-label="Toggle menu">
          <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 6h16M4 12h16M4 18h16"/></svg>
        </button>
      </div>
    </div>
  </header>
  <div id="mobile-nav" class="md:hidden px-6 md:px-10 py-4 bg-gradient-to-b from-black/95 to-black/90 backdrop-blur-xl fixed left-0 right-0 z-40 shadow-2xl border-t border-white/10" style="display:none">
    <nav class="space-y-1">
      <a class="flex items-center py-3 px-4 rounded-lg text-white/80 hover:text-white hover:bg-white/10 text-sm font-medium transition-all duration-200 hover:translate-x-1 group" href="{home}">
        <div class="w-2 h-2 rounded-full bg-emerald-400 mr-3 group-hover:bg-emerald-300 transition-colors"></div>
        Home
      </a>
      <a class="flex items-center py-3 px-4 rounded-lg text-white/80 hover:text-white hover:bg-white/10 text-sm font-medium transition-all duration-200 hover:translate-x-1 group" href="{lang}">
        <div class="w-2 h-2 rounded-full bg-blue-400 mr-3 group-hover:bg-blue-300 transition-colors"></div>
        Language
      </a>
      <a class="flex items-center py-3 px-4 rounded-lg text-white/80 hover:text-white hover:bg-white/10 text-sm font-medium transition-all duration-200 hover:translate-x-1 group" href="{tasks}">
        <div class="w-2 h-2 rounded-full bg-amber-400 mr-3 group-hover:bg-amber-300 transition-colors"></div>
        Tasks
      </a>
      <a class="flex items-center py-3 px-4 rounded-lg text-white/80 hover:text-white hover:bg-white/10 text-sm font-medium transition-all duration-200 hover:translate-x-1 group" href="https://github.com/eddmann/santa-lang-workshop" target="_blank" rel="noopener noreferrer">
        <div class="w-2 h-2 rounded-full bg-purple-400 mr-3 group-hover:bg-purple-300 transition-colors"></div>
        GitHub
        <svg class="w-3 h-3 ml-auto opacity-60 group-hover:opacity-100 transition-opacity" fill="currentColor" viewBox="0 0 20 20">
          <path fill-rule="evenodd" d="M10.293 3.293a1 1 0 011.414 0l6 6a1 1 0 010 1.414l-6 6a1 1 0 01-1.414-1.414L14.586 11H3a1 1 0 110-2h11.586l-4.293-4.293a1 1 0 010-1.414z" clip-rule="evenodd"></path>
        </svg>
      </a>
    </nav>
  </div>
  <main class="max-w-7xl mx-auto px-6 md:px-10 py-10 relative z-0">
    {}
  </main>
  <footer class="px-6 md:px-10 py-10 text-white/50 text-sm relative z-0 text-center">© 2025, <a class="text-white/70 hover:text-white no-underline hover:underline" href="https://eddmann.com/" target="_blank" rel="noopener noreferrer">Edd Mann</a></footer>
</body>
</html>"#,
        html_escape::encode_text(title),
        tailwind_head(),
        format!(r#"{}
<script>
  (function() {{
    const btn = document.getElementById('nav-toggle');
    const menu = document.getElementById('mobile-nav');
    if (btn && menu) {{
      menu.style.maxHeight = '0';
      menu.style.overflow = 'hidden';
      menu.style.display = 'block';
      menu.style.transition = 'max-height 0.3s ease-out, opacity 0.2s ease-out';
      menu.style.opacity = '0';
      
      btn.addEventListener('click', function() {{
        const isOpen = menu.style.maxHeight !== '0px' && menu.style.maxHeight !== '';
        if (isOpen) {{
          menu.style.maxHeight = '0';
          menu.style.opacity = '0';
          btn.setAttribute('aria-expanded', 'false');
        }} else {{
          menu.style.maxHeight = '400px';
          menu.style.opacity = '1';
          btn.setAttribute('aria-expanded', 'true');
        }}
      }});
    }}
  }})();
  if (window.hljs && window.hljs.highlightAll) {{ window.hljs.highlightAll(); }}
</script>"#, body),
        logo = logo_src,
        home = home_href,
        lang = lang_href,
        tasks = tasks_href
    )
}

fn layout(title: &str, body: &str, base_path: &str) -> String {
    layout_with_base(title, body, base_path)
}

fn layout_impl(title: &str, body: &str, base_path: &str) -> String {
    layout_with_base(title, body, base_path)
}

fn render_language_page(root: &Path, base_path: &str) -> String {
    let lang_path = root.join("specs").join("LANG.md");
    let (html, toc) = if let Ok(md) = fs::read_to_string(&lang_path) {
        let mut opts = MdOptions::empty();
        opts.insert(MdOptions::ENABLE_TABLES);
        opts.insert(MdOptions::ENABLE_FOOTNOTES);
        let parser = MdParser::new_ext(&md, opts);
        let mut out = String::new();
        md_html::push_html(&mut out, parser);
        // Helper to slugify titles
        fn slugify(s: &str) -> String {
            s.to_lowercase()
                .chars()
                .map(|c| if c.is_alphanumeric() { c } else if c.is_whitespace() || c == '-' { '-' } else { ' ' })
                .collect::<String>()
                .split_whitespace()
                .collect::<Vec<_>>()
                .join("-")
        }
        // Build TOC (exclude H1)
        let mut toc_items = Vec::new();
        for line in md.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with('#') {
                let level = trimmed.chars().take_while(|c| *c == '#').count();
                if level >= 2 {
                    let title = trimmed[level..].trim();
                    if !title.is_empty() {
                        let id = slugify(title);
                        toc_items.push((level, title.to_string(), id));
                    }
                }
            }
        }
        // Add ids to headings in HTML (manual scan; backrefs not supported)
        let heading_re = regex::Regex::new(r"(?s)<h([1-6])>(.*?)</h([1-6])>").unwrap();
        let mut html_with_ids = String::new();
        let mut last_end = 0usize;
        for caps in heading_re.captures_iter(&out) {
            let m = caps.get(0).unwrap();
            html_with_ids.push_str(&out[last_end..m.start()]);
            let open_level = &caps[1];
            let inner = &caps[2];
            let close_level = &caps[3];
            if open_level == close_level {
                let text_only = regex::Regex::new(r"<[^>]+>").unwrap().replace_all(inner, "");
                let id = slugify(&text_only);
                html_with_ids.push_str(&format!("<h{lvl} id=\"{id}\">{inner}</h{lvl}>", lvl=open_level, id=id, inner=inner));
            } else {
                html_with_ids.push_str(m.as_str());
            }
            last_end = m.end();
        }
        html_with_ids.push_str(&out[last_end..]);
        // Normalize unknown language fences for highlight.js
        html_with_ids = html_with_ids.replace("class=\"language-santa\"", "class=\"language-plaintext\"");

        let mut toc_html = String::new();
        for (level, title, id) in toc_items {
            let indent = match level { 2 => "pl-0", 3 => "pl-3", 4 => "pl-6", _ => "pl-9" };
            toc_html.push_str(&format!(
                "<a class=\"block text-sm text-white/70 hover:text-white {} py-1\" href=\"#{}\">{}</a>",
                indent,
                html_escape::encode_text(&id),
                html_escape::encode_text(&title)
            ));
        }
        (html_with_ids, toc_html)
    } else {
        ("<p>LANG.md not found.</p>".to_string(), String::new())
    };

    let body = format!(
        r#"<div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
  <aside class="paper-dark rounded-xl p-4 border border-white/10 lg:col-span-1 h-max sticky sticky-toc z-20">
    <div class="text-white/80 font-semibold mb-2">Table of contents</div>
    {}
  </aside>
  <section class="paper-dark rounded-xl p-6 border border-white/10 lg:col-span-3">
    <div class="prose prose-invert max-w-none">{}</div>
  </section>
</div>"#,
        toc,
        html
    );
    layout("elf-lang Language Reference", &body, base_path)
}

fn render_tasks_page(root: &Path, base_path: &str) -> String {
    let tasks_path = root.join("specs").join("TASKS.md");
    fn escape_placeholders_outside_code(src: &str) -> String {
        let placeholder_re = regex::Regex::new(r"(?i)<([a-z0-9_-]+)>").unwrap();
        let mut out = String::new();
        let mut in_fence = false;
        for line in src.lines() {
            let trimmed = line.trim_start();
            if trimmed.starts_with("```") {
                in_fence = !in_fence;
                out.push_str(line);
                out.push('\n');
                continue;
            }
            if in_fence {
                out.push_str(line);
                out.push('\n');
            } else {
                // Process inline code: split by backticks and escape only even segments
                let mut result_segment = String::new();
                let mut parts = line.split('`').peekable();
                let mut idx = 0usize;
                while let Some(seg) = parts.next() {
                    if idx % 2 == 0 {
                        // outside inline code
                        let replaced = placeholder_re.replace_all(seg, "&lt;$1&gt;");
                        result_segment.push_str(&replaced);
                    } else {
                        // inside inline code, put backticks around untouched segment
                        result_segment.push('`');
                        result_segment.push_str(seg);
                        if parts.peek().is_some() { result_segment.push('`'); }
                    }
                    idx += 1;
                }
                out.push_str(&result_segment);
                out.push('\n');
            }
        }
        out
    }
    let (html, toc) = if let Ok(md) = fs::read_to_string(&tasks_path) {
        let processed_md = escape_placeholders_outside_code(&md);
        let mut opts = MdOptions::empty();
        opts.insert(MdOptions::ENABLE_TABLES);
        opts.insert(MdOptions::ENABLE_FOOTNOTES);
        let parser = MdParser::new_ext(&processed_md, opts);
        let mut out = String::new();
        md_html::push_html(&mut out, parser);
        fn slugify(s: &str) -> String {
            s.to_lowercase()
                .chars()
                .map(|c| if c.is_alphanumeric() { c } else if c.is_whitespace() || c == '-' { '-' } else { ' ' })
                .collect::<String>()
                .split_whitespace()
                .collect::<Vec<_>>()
                .join("-")
        }
        let mut toc_items = Vec::new();
        for line in processed_md.lines() {
            let trimmed = line.trim();
            if trimmed.starts_with('#') {
                let level = trimmed.chars().take_while(|c| *c == '#').count();
                if level >= 2 {
                    let title = trimmed[level..].trim();
                    if !title.is_empty() {
                        let id = slugify(title);
                        toc_items.push((level, title.to_string(), id));
                    }
                }
            }
        }
        let heading_re = regex::Regex::new(r"(?s)<h([1-6])>(.*?)</h([1-6])>").unwrap();
        let mut html_with_ids = String::new();
        let mut last_end = 0usize;
        for caps in heading_re.captures_iter(&out) {
            let m = caps.get(0).unwrap();
            html_with_ids.push_str(&out[last_end..m.start()]);
            let open_level = &caps[1];
            let inner = &caps[2];
            let close_level = &caps[3];
            if open_level == close_level {
                let text_only = regex::Regex::new(r"<[^>]+>").unwrap().replace_all(inner, "");
                let id = slugify(&text_only);
                html_with_ids.push_str(&format!("<h{lvl} id=\"{id}\">{inner}</h{lvl}>", lvl=open_level, id=id, inner=inner));
            } else {
                html_with_ids.push_str(m.as_str());
            }
            last_end = m.end();
        }
        html_with_ids.push_str(&out[last_end..]);
        // Normalize unknown language fences for highlight.js
        html_with_ids = html_with_ids.replace("class=\"language-santa\"", "class=\"language-plaintext\"");

        let mut toc_html = String::new();
        for (level, title, id) in toc_items {
            let indent = match level { 2 => "pl-0", 3 => "pl-3", 4 => "pl-6", _ => "pl-9" };
            toc_html.push_str(&format!(
                "<a class=\"block text-sm text-white/70 hover:text-white {} py-1\" href=\"#{}\">{}</a>",
                indent,
                html_escape::encode_text(&id),
                html_escape::encode_text(&title)
            ));
        }
        (html_with_ids, toc_html)
    } else {
        ("<p>TASKS.md not found.</p>".to_string(), String::new())
    };

    let body = format!(
        r#"<div class="grid grid-cols-1 lg:grid-cols-4 gap-6">
  <aside class="paper-dark rounded-xl p-4 border border-white/10 lg:col-span-1 h-max sticky sticky-toc">
    <div class="text-white/80 font-semibold mb-2">Table of contents</div>
    {}
  </aside>
  <section class="paper-dark rounded-xl p-6 border border-white/10 lg:col-span-3">
    <div class="prose prose-invert max-w-none">{}</div>
  </section>
</div>"#,
        toc,
        html
    );
    layout("elf-lang Tasks", &body, base_path)
}

fn read_readme_intro(_root: &Path) -> String {
    let p1 = r#"👋🏻 Welcome to the santa-lang Workshop, a cozy corner of the North Pole where elf-lang (a subset of santa-lang) comes to life. Here, magical elves (LLM agents) sit at their workbenches, carefully crafting elf-lang implementations in different languages and making sure each one behaves exactly as expected.

Below, you’ll find their handiwork on display, each accompanied by a journal where the elves explain how and why they built it the way they did."#;
    let parts: Vec<String> = p1
        .split("\n\n")
        .map(|s| format!("<p>{}</p>", html_escape::encode_text(s.trim())))
        .collect();
    parts.join("\n")
}

fn render_index(impls: &[ImplInfo], intro_html: &str, base_path: &str) -> String {
    let mut cards = String::new();
    let mut langs: BTreeSet<String> = BTreeSet::new();
    let mut harnesses: BTreeSet<String> = BTreeSet::new();
    let mut models: BTreeSet<String> = BTreeSet::new();
    for ii in impls {
        let img_src = match &ii.elf_png_path {
            Some(_) => format!("impl/{}/elf.png", ii.dir_name),
            None => base_url(base_path, "unknown-elf.png"),
        };
        let author = if ii.journal.author.trim().is_empty() { "Unknown Elf".to_string() } else { ii.journal.author.clone() };
        let _progress = &ii.journal.progress;
        let lang = ii.journal.details.language.clone();
        let harness = ii.journal.details.harness.clone();
        let model = ii.journal.details.model.clone();
        if !lang.trim().is_empty() { langs.insert(lang.clone()); }
        if !harness.trim().is_empty() { harnesses.insert(harness.clone()); }
        if !model.trim().is_empty() { models.insert(model.clone()); }
        cards.push_str(&format!(
            r#"<a class="impl-card group overflow-hidden rounded-xl paper-dark bg-white/5 border border-white/10 hover:shadow-glow transition" href="impl/{dir}/index.html" data-lang="{data_lang}" data-harness="{data_harness}" data-model="{data_model}">
  <div class="aspect-square overflow-hidden bg-black/30">
    <img class="w-full h-full object-cover object-center group-hover:scale-105 transition" src="{img}" alt="Elf {author}">
  </div>
  <div class="p-5">
    <h3 class="text-lg font-semibold">{author}</h3>
    <div class="mt-2 flex items-center gap-2 text-xs text-white/60">
      <span class="px-2 py-0.5 rounded-full bg-emerald-500/10 text-emerald-300 border border-emerald-300/20">{lang}</span>
      <span class="px-2 py-0.5 rounded-full bg-rose-500/10 text-rose-300 border border-rose-300/20">{harness}</span>
      <span class="px-2 py-0.5 rounded-full bg-amber-500/10 text-amber-300 border border-amber-300/20">{model}</span>
    </div>

  </div>
</a>"#,
            dir = ii.dir_name,
            img = img_src,
            author = html_escape::encode_text(&author),
            lang = html_escape::encode_text(&lang),
            harness = html_escape::encode_text(&harness),
            model = html_escape::encode_text(&model),
            data_lang = html_escape::encode_double_quoted_attribute(&lang),
            data_harness = html_escape::encode_double_quoted_attribute(&harness),
            data_model = html_escape::encode_double_quoted_attribute(&model),
        ));
    }

    // Build filter controls
    let mut lang_opts = String::new();
    lang_opts.push_str("<option value=\"\">All</option>");
    for v in &langs { lang_opts.push_str(&format!("<option value=\"{}\">{}</option>", html_escape::encode_double_quoted_attribute(v), html_escape::encode_text(v))); }
    let mut harness_opts = String::new();
    harness_opts.push_str("<option value=\"\">All</option>");
    for v in &harnesses { harness_opts.push_str(&format!("<option value=\"{}\">{}</option>", html_escape::encode_double_quoted_attribute(v), html_escape::encode_text(v))); }
    let mut model_opts = String::new();
    model_opts.push_str("<option value=\"\">All</option>");
    for v in &models { model_opts.push_str(&format!("<option value=\"{}\">{}</option>", html_escape::encode_double_quoted_attribute(v), html_escape::encode_text(v))); }

    let filters = format!(
        r#"<div class="mt-4 w-full rounded-xl bg-white/5 backdrop-blur border border-white/15 p-3">
  <div class="flex items-center justify-between gap-2">
    <div class="text-white/80 font-semibold">Filters</div>
    <div class="flex items-center gap-2">
      <button id="filters-clear" class="px-3 py-1 rounded bg-white/10 hover:bg-white/20 text-white/80 border border-white/10 text-sm">Clear</button>
      <button id="filters-toggle" class="md:hidden px-3 py-1 rounded bg-white/10 hover:bg-white/20 text-white/80 border border-white/10 text-sm" aria-expanded="false" aria-controls="filters-panel">Show</button>
    </div>
  </div>
  <div id="filters-panel" class="mt-2 hidden md:block">
    <div class="grid grid-cols-1 md:grid-cols-3 gap-2">
      <label class="flex items-center gap-2 px-3 py-2 rounded-full bg-black/20 border border-white/10">
        <span class="text-xs text-white/60">Language</span>
        <select id="filter-lang" class="bg-transparent text-sm text-white/90 w-full outline-none appearance-none">
          {lang_opts}
        </select>
      </label>
      <label class="flex items-center gap-2 px-3 py-2 rounded-full bg-black/20 border border-white/10">
        <span class="text-xs text-white/60">Harness</span>
        <select id="filter-harness" class="bg-transparent text-sm text-white/90 w-full outline-none appearance-none">
          {harness_opts}
        </select>
      </label>
      <label class="flex items-center gap-2 px-3 py-2 rounded-full bg-black/20 border border-white/10">
        <span class="text-xs text-white/60">Model</span>
        <select id="filter-model" class="bg-transparent text-sm text-white/90 w-full outline-none appearance-none">
          {model_opts}
        </select>
      </label>
    </div>
  </div>
</div>"#,
        lang_opts = lang_opts,
        harness_opts = harness_opts,
        model_opts = model_opts,
    );

    let body = format!(
        r#"<section class="paper-dark rounded-xl p-6 border border-white/10">
  <div class="prose prose-invert max-w-none">{}</div>
</section>

<h2 class="mt-10 text-3xl font-semibold text-white/90">🎄 Showcase</h2>
{filters}
<section id="showcase-grid" class="mt-4 grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-6">
  {}
</section>
<div id="empty-state" class="mt-4 text-white/60 text-sm" style="display:none">No implementations match the selected filters.</div>
<script>
  (function() {{
    const selLang = document.getElementById('filter-lang');
    const selHarness = document.getElementById('filter-harness');
    const selModel = document.getElementById('filter-model');
    const grid = document.getElementById('showcase-grid');
    const cards = Array.from(grid ? grid.querySelectorAll('.impl-card') : []);
    const empty = document.getElementById('empty-state');
    function val(x) {{ return (x && x.value ? x.value : '').toLowerCase(); }}
    function apply() {{
      const lv = val(selLang), hv = val(selHarness), mv = val(selModel);
      let visible = 0;
      cards.forEach(card => {{
        const ok = (!lv || (card.getAttribute('data-lang')||'').toLowerCase() === lv)
          && (!hv || (card.getAttribute('data-harness')||'').toLowerCase() === hv)
          && (!mv || (card.getAttribute('data-model')||'').toLowerCase() === mv);
        card.style.display = ok ? '' : 'none';
        if (ok) visible++;
      }});
      if (empty) empty.style.display = visible ? 'none' : '';
    }}
    [selLang, selHarness, selModel].forEach(s => s && s.addEventListener('change', apply));
    const clearBtn = document.getElementById('filters-clear');
    clearBtn?.addEventListener('click', () => {{
      if (selLang) selLang.value = '';
      if (selHarness) selHarness.value = '';
      if (selModel) selModel.value = '';
      apply();
    }});
    const toggleBtn = document.getElementById('filters-toggle');
    const panel = document.getElementById('filters-panel');
    toggleBtn?.addEventListener('click', () => {{
      if (!panel) return;
      const willShow = panel.classList.contains('hidden');
      panel.classList.toggle('hidden');
      toggleBtn.setAttribute('aria-expanded', String(willShow));
      toggleBtn.textContent = willShow ? 'Hide' : 'Show';
    }});
    apply();
  }})();
</script>"#,
        intro_html,
        cards,
        filters = filters
    );
    layout("santa-lang Workshop", &body, base_path)
}

fn render_impl(imp: &ImplInfo, tree: &BTreeMap<String, String>, base_path: &str) -> String {
    let author = if imp.journal.author.trim().is_empty() { "Unknown Elf" } else { &imp.journal.author };
    let mut entries = imp.journal.journal.clone();
    entries.sort_by(|a, b| b.written_at.cmp(&a.written_at));

    let mut journal_html = String::new();
    if entries.is_empty() {
        journal_html.push_str("<p class=\"text-white/60\">No journal entries yet.</p>");
    } else {
        for e in entries {
            // Render entry text as Markdown
            let mut opts = MdOptions::empty();
            opts.insert(MdOptions::ENABLE_TABLES);
            opts.insert(MdOptions::ENABLE_FOOTNOTES);
            let mdp = MdParser::new_ext(&e.entry, opts);
            let mut rendered = String::new();
            md_html::push_html(&mut rendered, mdp);
            journal_html.push_str(&format!(
                r#"<article class="paper-dark rounded-lg p-5 border border-white/10 mb-4">
  <div class="text-xs text-white/50">{date}</div>
  <div class="mt-3 journal-line pl-3">
    <div class="prose prose-invert max-w-none journal-body">
      {text}
    </div>
  </div>
</article>"#,
                date = html_escape::encode_text(&e.written_at),
                text = rendered
            ));
        }
    }

    // GitHub-like code browser: left list of files, right content area; client-side tabs
    let mut files_sidebar = String::new();
    let mut first_file: Option<(&str, &String)> = None;
    for (rel, _) in tree {
        if first_file.is_none() { first_file = Some((rel.as_str(), tree.get(rel).unwrap())); }
        files_sidebar.push_str(&format!(
            r#"<button class="w-full text-left px-3 py-1.5 rounded hover:bg-white/10 code text-xs" data-file="{}">{}</button>"#,
            html_escape::encode_text(rel),
            html_escape::encode_text(rel)
        ));
    }

    let initial_file = first_file.map(|(r, c)| (r.to_string(), c.clone())).unwrap_or((String::new(), String::new()));

    let code_browser = format!(
        r#"<div class="grid grid-cols-1 lg:grid-cols-4 gap-4">
  <aside class="paper-dark rounded-lg p-3 border border-white/10 lg:col-span-1 max-h-[60vh] overflow-auto">
    {sidebar}
  </aside>
  <section class="paper-dark rounded-lg p-0 border border-white/10 lg:col-span-3 overflow-hidden">
    <div class="px-4 py-2 text-xs text-white/60 border-b border-white/10 bg-black/30 flex items-center justify-between">
      <span id="code-filename">{fname}</span>
      <button id="copy-code" class="px-2 py-1 rounded bg-white/10 hover:bg-white/20 text-white/80">Copy</button>
    </div>
    <pre class="code text-sm p-4 overflow-auto max-h-[60vh] bg-black/30"><code id="code-content" class="hljs">{content}</code></pre>
  </section>
</div>
<script>
  const files = {files_json};
  function langFromFilename(name) {{
    const ext = (name.split('.').pop() || '').toLowerCase();
    switch (ext) {{
      case 'rs': return 'rust';
      case 'py': return 'python';
      case 'go': return 'go';
      case 'js': return 'javascript';
      case 'ts': return 'typescript';
      case 'json': return 'json';
      case 'md': return 'markdown';
      case 'c': return 'c';
      case 'h': return 'c';
      case 'txt': return 'plaintext';
      case 'santa': return 'plaintext';
      default: return 'plaintext';
    }}
  }}
  function setFile(name) {{
    const fname = document.getElementById('code-filename');
    const code = document.getElementById('code-content');
    fname.textContent = name;
    code.textContent = files[name] || '';
    const lang = langFromFilename(name);
    code.className = 'hljs language-' + lang;
    // Update active sidebar button
    document.querySelectorAll('[data-file]').forEach(btn => {{
      const isActive = btn.getAttribute('data-file') === name;
      btn.classList.toggle('bg-white/10', isActive);
      btn.classList.toggle('text-white', isActive);
    }});
    // Re-highlight
    if (window.hljs && window.hljs.highlightElement) {{ window.hljs.highlightElement(code); }}
  }}
  document.querySelectorAll('[data-file]').forEach(btn => {{
    btn.addEventListener('click', () => setFile(btn.getAttribute('data-file')));
  }});
  const copyBtn = document.getElementById('copy-code');
  copyBtn?.addEventListener('click', async () => {{
    try {{
      await navigator.clipboard.writeText(document.getElementById('code-content').textContent || '');
      copyBtn.textContent = 'Copied!';
      setTimeout(() => copyBtn.textContent = 'Copy', 1200);
    }} catch (e) {{
      copyBtn.textContent = 'Failed';
      setTimeout(() => copyBtn.textContent = 'Copy', 1200);
    }}
  }});
  setFile({init_name});
</script>"#,
        sidebar = files_sidebar,
        fname = html_escape::encode_text(&initial_file.0),
        content = html_escape::encode_text(&initial_file.1),
        files_json = serde_json::to_string(&tree).unwrap(),
        init_name = serde_json::to_string(&initial_file.0).unwrap()
    );

    let tabs = format!(
        r###"<div class="mt-6 mb-4">
  <div class="w-full rounded-full bg-white/10 backdrop-blur border border-white/15 p-1">
    <div class="grid grid-cols-2 gap-3">
      <button type="button" class="tab tab-active w-full text-center" data-tab="journal">Journal</button>
      <button type="button" class="tab w-full text-center" data-tab="code">Code</button>
    </div>
  </div>
</div>
<div id="tab-journal">{journal}</div>
<div id="tab-code" style="display:none">{code}</div>
<script>
  function showTab(which) {{
    const j = document.getElementById('tab-journal');
    const c = document.getElementById('tab-code');
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(t => t.classList.remove('tab-active'));
    if (which === 'journal') {{ j.style.display = ''; c.style.display = 'none'; tabs[0].classList.add('tab-active'); }}
    else {{ j.style.display = 'none'; c.style.display = ''; tabs[1].classList.add('tab-active'); }}
    // Re-run syntax highlighting when switching tabs
    if (window.hljs && window.hljs.highlightAll) {{
      window.hljs.highlightAll();
    }}
  }}
  document.querySelectorAll('.tab').forEach(b => {{
    b.addEventListener('click', () => showTab(b.getAttribute('data-tab')));
  }});
  showTab('journal');
</script>"###,
        journal = journal_html,
        code = code_browser
    );

    let gh_url = format!(
        "https://github.com/eddmann/santa-lang-workshop/tree/main/impl/{}",
        imp.dir_name
    );

    let header_img_src = if imp.elf_png_path.is_some() { "elf.png".to_string() } else { base_url(base_path, "unknown-elf.png") };

    let header = format!(
        r#"<div class="flex items-center gap-5">
  <div class="w-20 h-20 rounded-xl overflow-hidden bg-black/30 border border-white/10">
    <img class="w-full h-full object-cover" src="{img}" alt="Elf">
  </div>
  <div>
    <h1 class="text-2xl font-semibold">{author}</h1>
    <div class="mt-2 flex gap-2 text-xs">
      <span class="px-2 py-0.5 rounded-full bg-emerald-500/10 text-emerald-300 border border-emerald-300/20">{lang}</span>
      <span class="px-2 py-0.5 rounded-full bg-rose-500/10 text-rose-300 border border-rose-300/20">{harness}</span>
      <span class="px-2 py-0.5 rounded-full bg-amber-500/10 text-amber-300 border border-amber-300/20">{model}</span>
    </div>
  </div>
  <a class="ml-auto text-sm px-3 py-1 rounded bg-white/10 hover:bg-white/20 text-white/80 border border-white/10" href="{gh}" target="_blank" rel="noopener noreferrer">View on GitHub</a>
</div>"#,
        author = html_escape::encode_text(author),
        lang = html_escape::encode_text(&imp.journal.details.language),
        harness = html_escape::encode_text(&imp.journal.details.harness),
        model = html_escape::encode_text(&imp.journal.details.model),
        gh = gh_url,
        img = header_img_src
    );

    layout_impl(&format!("{} – {}", imp.journal.details.language, author), &format!("{}{}", header, tabs), base_path)
}

fn collect_code_tree(root: &Path, exclude_dirs: &[&str]) -> Result<BTreeMap<String, String>, String> {
    let mut map = BTreeMap::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(dir) = stack.pop() {
        for ent in fs::read_dir(&dir).map_err(|e| e.to_string())?.flatten() {
            let p = ent.path();
            let name = ent.file_name();
            let name = name.to_string_lossy();
            if p.is_dir() {
                if exclude_dirs.iter().any(|ex| name.eq_ignore_ascii_case(ex)) { continue; }
                if name.starts_with('.') { continue; }
                stack.push(p);
            } else {
                // skip large/binary-ish files by extension
                if let Some(ext) = p.extension().and_then(|e| e.to_str()) {
                    let ext = ext.to_ascii_lowercase();
                    if ["png","jpg","jpeg","gif","svg","webp","ico","bmp","pdf","zip","tar","gz","bz2","xz"].contains(&ext.as_str()) {
                        continue;
                    }
                }
                let rel = p.strip_prefix(root).unwrap().to_string_lossy().to_string();
                let content = fs::read_to_string(&p).unwrap_or_else(|_| "(binary or unreadable)".to_string());
                map.insert(rel, content);
            }
        }
    }
    Ok(map)
}

fn copy_assets(out_dir: &Path, impls: &[ImplInfo]) -> Result<(), String> {
    // Copy root logo-light.png to docs/
    let root_logo = repo_root().join("logo-light.png");
    if root_logo.exists() {
        let dst = out_dir.join("logo-light.png");
        if let Some(parent) = dst.parent() { ensure_dir(parent)?; }
        fs::copy(&root_logo, &dst).map_err(|e| format!("Failed to copy {} -> {}: {}", root_logo.display(), dst.display(), e))?;
    }
    // Copy unknown-elf.png fallback to docs/
    let unknown_elf = repo_root().join("unknown-elf.png");
    if unknown_elf.exists() {
        let dst = out_dir.join("unknown-elf.png");
        if let Some(parent) = dst.parent() { ensure_dir(parent)?; }
        fs::copy(&unknown_elf, &dst).map_err(|e| format!("Failed to copy {} -> {}: {}", unknown_elf.display(), dst.display(), e))?;
    }
    for ii in impls {
        if let Some(src) = &ii.elf_png_path {
            let dst = out_dir.join("impl").join(&ii.dir_name).join("elf.png");
            if let Some(parent) = dst.parent() { ensure_dir(parent)?; }
            fs::copy(src, &dst).map_err(|e| format!("Failed to copy {} -> {}: {}", src.display(), dst.display(), e))?;
        }
    }
    Ok(())
}

fn main() -> Result<(), String> {
    let args = Args::parse();
    let root = repo_root();
    let impl_dir = args.impl_dir.unwrap_or(root.join("impl"));
    let out_dir = args.out_dir.unwrap_or(root.join("docs"));
    let base_path = args.base_path.unwrap_or_default();

    let impls = read_impls(&impl_dir)?;
    ensure_dir(&out_dir)?;

    // index.html
    let intro = read_readme_intro(&root);
    let index_html = render_index(&impls, &intro, &base_path);
    write_file(&out_dir.join("index.html"), &index_html)?;

    // language page
    let lang_html = render_language_page(&root, &base_path);
    let lang_dir = out_dir.join("language");
    ensure_dir(&lang_dir)?;
    write_file(&lang_dir.join("index.html"), &lang_html)?;

    // tasks page
    let tasks_html = render_tasks_page(&root, &base_path);
    let tasks_dir = out_dir.join("tasks");
    ensure_dir(&tasks_dir)?;
    write_file(&tasks_dir.join("index.html"), &tasks_html)?;

    // per-impl pages and code assets
    for ii in &impls {
        let code_tree = collect_code_tree(&ii.abs_path, &["target", "__pycache__", "node_modules", "venv", "env", "build", "dist"])?;
        let html = render_impl(ii, &code_tree, &base_path);
        let impl_dir_out = out_dir.join("impl").join(&ii.dir_name);
        ensure_dir(&impl_dir_out)?;
        write_file(&impl_dir_out.join("index.html"), &html)?;
    }

    // Copy images per impl
    copy_assets(&out_dir, &impls)?;

    println!("Site generated at {}", out_dir.display());
    Ok(())
}
