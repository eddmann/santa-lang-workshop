# TASKS: Implement `elf-lang` in Python

This document guides an LLM agent to implement `elf-lang` in idiomatic, modular, and readable Python, progressing through stages 1-5. Each stage builds on the previous one. Keep all earlier stages green as you advance.

## Details

Language: Python
Requirements: Use Python 3.13, without any external packages or dependencies.

## Workflow overview

- **Author (first action)**: verify the author. If unset, set a festive elf name using the tool.
  - Check: `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 author`
  - If not set, set: `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 author set "<christmas-themed elf name>"`
- **Developer tooling setup (Dockerized toolchain) (if missing)**: if your implementation directory does not have developer tooling set up, create the initial project structure under `./impl/python-claudecode-sonnet4-1757409766` with Dockerized toolchain and CLI (see Developer tooling setup section below). Verify images build before proceeding.
- **Review the journal before starting**: run `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entries` to align with context and decisions.
- **Keep the journal updated**: after meaningful progress and before ending the session, update progress via `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 progress` and add notes with `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entry`.
- **Read the full spec**: open `specs/LANG.md` to understand the complete language.
- **Read the current stage brief**: open `tests/stage-*/README.md` for the stage you are on.
- **Study examples**: review `tests/stage-*/*.santat` to see exact expected behavior and outputs.
- **Implement incrementally**: code the minimum needed to pass the current failing tests for your stage.
- **Run tests frequently** to steer development using the Makefile in your implementation directory:
  - All tests for a stage: `make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-N`
  - Single test file: `make -C ./impl/python-claudecode-sonnet4-1757409766 test-file FILE=tests/stage-N/<testname>`
- **Advancement rule**: You may proceed to the next stage only when all tests in the current stage and all prior stages pass.

All invocations MUST use Docker via the standardized Makefile.

## Required CLI contract (must not change)

- **Run program**: `<bin> <file>`
- **Print AST**: `<bin> ast <file>`
- **Print tokens**: `<bin> tokens <file>`
- **Exit code**: not prescribed; tests validate stdout content only
- **Streams**: results on stdout, diagnostics on stdout
- **File encoding**: UTF-8; treat input as LF newlines (`\n`)
- **Source files**: elf-lang programs use the `.santa` extension; pass a `.santa` file path to the CLI. `.santat` files are used by the `santa-test` runner only and are not valid CLI inputs.
- **Determinism/formatting**: no banners, prompts, or ANSI color; exact spacing; LF newlines; stable key order; stable ordering across runs; no timing or extra logs

## Repository boundaries and tests (strict)

- **Allowed**: read and review `tests/**/*.santat` to understand the spec.
- **Allowed**: add temporary `.santat` test files under your own `./impl/python-claudecode-sonnet4-1757409766/tmp/*` solely for local debugging.
  - These temporary files must be removed before you finish; they are not part of the shared suite.
- **Forbidden**: any changes outside `./impl/python-claudecode-sonnet4-1757409766/**`.
  - Do not edit files in `./tests/**`, `./tools/**`, `./README.md`, or any other repository path outside your implementation directory.
  - Do not modify the shared tests or the test runner in any way.

Additional notes:

- Use `specs/LANG.md` and the `.santat` tests as your source of truth.
- Ensure Docker images exist and are used for all invocations: `local/santa-python-claudecode-sonnet4-1757409766:build` and `local/santa-python-claudecode-sonnet4-1757409766:cli`. Always build and run them via the standardized Makefile targets.
- For any ad-hoc/example debug runs you create locally, always wrap the command with `timeout 5` to avoid hangs. Example:
  - `timeout 5 make -C ./impl/python-claudecode-sonnet4-1757409766 run ARGS=./impl/python-claudecode-sonnet4-1757409766/tmp/example.santa`

## Project layout for your implementation

Create your implementation under `/impl/python-claudecode-sonnet4-1757409766`:

- `./impl/python-claudecode-sonnet4-1757409766/cli` — the CLI entrypoint your `:cli` image invokes (optional for host use; tests use Docker)
- Any source files/modules as needed (you choose structure)
- `./impl/python-claudecode-sonnet4-1757409766/.gitignore` — language-specific ignores for the chosen Python, MUST include `/impl/python-claudecode-sonnet4-1757409766/tmp` directory

Notes:

- Your `Dockerfile.cli` must set `ENTRYPOINT` to the CLI. You may keep a local `cli` script/binary for convenience, but the test runner will always use the Docker image.

## Developer tooling setup (Dockerized toolchain)

If `./impl/python-claudecode-sonnet4-1757409766` lacks a Dockerized toolchain, create the following before you start coding:

- `./impl/python-claudecode-sonnet4-1757409766/Dockerfile.build` — full toolchain image (non-root, `WORKDIR=/work`).
- `./impl/python-claudecode-sonnet4-1757409766/Dockerfile.cli` — minimal runtime image with `ARG BUILDER_IMAGE=local/santa-python-claudecode-sonnet4-1757409766:build` and an `ENTRYPOINT` invoking the CLI.
- An initial CLI entrypoint at `./impl/python-claudecode-sonnet4-1757409766/cli` that the `:cli` image runs.

- Use the existing per-implementation Makefile targets (scoped to `./impl/python-claudecode-sonnet4-1757409766`):
  - `build-image`: builds/tags `local/santa-python-claudecode-sonnet4-1757409766:build`
  - `cli-image` (depends on `build-image`): builds `local/santa-python-claudecode-sonnet4-1757409766:cli` using `--build-arg BUILDER_IMAGE=local/santa-python-claudecode-sonnet4-1757409766:build`
  - `shell`: interactive shell in the builder image
  - `exec`: run a single command in the builder image (e.g., `make exec CMD="go test ./..."`)
  - `run`: run the CLI image with args (e.g., `ARGS="tokens tests/stage-1/01_basic_tokens.santat"`)
  - `print-uri`: print `docker://` URI for santa-test
  - `test`, `test-stage-1..5`, `test-file`: run conformance tests via santa-test

Then build and validate using Make:

```bash
make -C ./impl/python-claudecode-sonnet4-1757409766 build-image
make -C ./impl/python-claudecode-sonnet4-1757409766 cli-image
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-1
```

Test integration: use the provided Makefile targets which invoke santa-test with the dockerized CLI.

Acceptance:

- `make -C ./impl/python-claudecode-sonnet4-1757409766 build-image` produces `local/santa-python-claudecode-sonnet4-1757409766:build` locally.
- `make -C ./impl/python-claudecode-sonnet4-1757409766 cli-image` produces `local/santa-python-claudecode-sonnet4-1757409766:cli`.
- `make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-1` runs successfully (substitute any stage or file target as needed).
- `make -C ./impl/python-claudecode-sonnet4-1757409766 shell` provides a development shell with the toolchain without modifying the host environment.

Reference patterns (abridged examples; adapt per language):

- Rust `Dockerfile.build` uses `rust:<version>` with non-root `dev` user, `WORKDIR=/work`, and cache mounts for Cargo registry/git; `Dockerfile.cli` uses `ARG BUILDER_IMAGE=local/santa-rust:build`, builds in the builder stage, and copies the release binary into `gcr.io/distroless/cc-debian12` with an `ENTRYPOINT` to the binary.
- Go `Dockerfile.build` uses `golang:<version>` with non-root `dev` user, `WORKDIR=/work`, `GOPATH` configured; `Dockerfile.cli` builds with cache mounts for `go mod` and `go-build` caches and copies the static binary into `gcr.io/distroless/base-debian12`.
- Python `Dockerfile.build` includes full tooling (pip/uv/poetry); `Dockerfile.cli` installs only runtime deps (e.g., `requirements.txt`). If build artifacts are needed, set `ARG BUILDER_IMAGE=local/santa-python:build` and copy from the builder stage.

## How to run tests

From the repository root, always use the per-impl Makefile:

```bash
# Build the CLI Docker image for your implementation first
make -C ./impl/python-claudecode-sonnet4-1757409766 cli-image

# Run tests using the Dockerized CLI
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-1
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-2
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-3
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-4
make -C ./impl/python-claudecode-sonnet4-1757409766 test-stage-5

# Run an individual test file
make -C ./impl/python-claudecode-sonnet4-1757409766 test-file FILE=tests/stage-1/01_basic_tokens.santat
```

## Stage overviews and success criteria

Use `specs/LANG.md` for authoritative definitions and each `tests/stage-N/README.md` for stage scope.

Before stage 3, the run program mode may be incomplete and tests will only invoke the `tokens` and `ast` subcommands. When advancing stages, do not regress behavior or formatting for earlier stages.

### Stage 1 — Lexing

- **Scope**: tokens for literals, operators/symbols, keywords/identifiers, numeric underscores.
- **Outputs**: `tokens` subcommand prints JSON Lines with exact `type` and `value` slices in stable order.
- **Pass condition**: all `tests/stage-1/**` pass.

### Stage 2 — Parsing

- **Scope**: parse literals, lets, infix ops, lists/sets/dicts, if-expr, function literals/calls, threads/composition (AST-only).
- **Outputs**: `ast` subcommand prints a single JSON document for the program with pretty-printed JSON (2-space indentation) and ordered keys.
- **Pass condition**: all `tests/stage-1/**` and `tests/stage-2/**` pass.

### Stage 3 — Basic evaluation

- **Scope**: runtime with `puts`/CLI integration, arithmetic, variables, errors, string escapes, numeric underscores, precedence, unicode literals.
- **Outputs**: running `<bin> <file>` prints program output exactly; errors go to stdout and exit non-zero.
- **Pass condition**: all `tests/stage-1..3/**` pass with no regressions.

### Stage 4 — Collections & indexing

- **Scope**: list/set/dict operations, string ops (size, indexing), indexing edge cases, mixed-type collection behaviors. Printed order for collections is deterministic: Dictionaries print in ascending order by key; Sets print in ascending order by value.
- **Outputs**: evaluation semantics must match tests precisely, including error messages and indexing rules.
- **Pass condition**: all `tests/stage-1..4/**` pass with no regressions.

### Stage 5 — Higher-order & composition

- **Scope**: recursion, `map`/`filter`/`fold`, arity handling, complex nested expressions.
- **Outputs**: correct evaluation results and exact error messages for arity/type issues.
- **Pass condition**: all `tests/stage-1..5/**` pass with no regressions.

### Output formats (strict)

Tokens (JSON Lines; one object per line):

```json
{"type":"INT","value":"123"}
{"type":"DEC","value":"456.789"}
{"type":"STR","value":"\"hello\""}
{"type":"TRUE","value":"true"}
{"type":"FALSE","value":"false"}
{"type":"NIL","value":"nil"}
```

- Required keys: `type` (token kind), `value` (exact slice)
- Tokens must be minified JSON Lines: no spaces, exactly one JSON object per line, exactly as shown above
- Determinism: stable ordering and whitespace; LF newlines; no ANSI color; no extra logs

AST (single JSON document; abridged example):

```json
{
  "statements": [
    {
      "type": "Expression",
      "value": {
        "name": { "name": "x", "type": "Identifier" },
        "type": "Let",
        "value": { "type": "Integer", "value": "42" }
      }
    }
  ],
  "type": "Program"
}
```

AST output must be pretty-printed JSON with ordered keys, 2-space indentation, and no trailing commas

Program output: exact stdout of running `<file>` with no extra logs.

## Completion criteria

- All tests pass via the Makefile:
  ```bash
  make -C ./impl/python-claudecode-sonnet4-1757409766 test
  ```
- CLI contract is respected and stable across runs.
- Output formats and error messages match expectations exactly. Exact error strings live in `specs/LANG.md`; mismatches (even punctuation or casing) will fail tests.
- `local/santa-python-claudecode-sonnet4-1757409766:build` and `local/santa-python-claudecode-sonnet4-1757409766:cli` images build successfully; `:cli` ENTRYPOINT invokes the CLI.
- Code is idiomatic, readable, and maintainable in Python.

## Agent handoff and progress tracking

- Purpose: enable multiple agents to continue work seamlessly, knowing exactly what was completed and what remains.
- Location (required): keep continuity data inside your implementation directory only, managed via `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766`.
  - Do not modify files outside `./impl/python-claudecode-sonnet4-1757409766/**`.

Required update cadence (MUST):

- At session start: review entries with `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entries` and align your plan; then set Current focus via a new `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entry`.
- Immediately after setting the author: add a kickoff entry noting the chosen implementation directory, initial focus, and the exact command to resume (e.g., `make -C ./impl/python-claudecode-sonnet4-1757409766 cli-image`).
- After completing Developer tooling setup (Dockerized toolchain): add an entry noting images were built and the CLI is runnable, including the exact commands used (e.g., `make -C ./impl/python-claudecode-sonnet4-1757409766 build-image`, `cli-image`, and a sample `run`/`test-stage-1`).
- Before starting work on a stage: set its progress to in-progress:
  - `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 progress <stage-*> set in-progress`
- When all tests pass for a stage and you are moving on: mark it complete:
  - `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 progress <stage-*> set complete`
- During a stage: after meaningful progress (e.g., a group of tests turns green), add an explanatory `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entry`. Do not change stage status unless transitioning state (e.g., to complete).
- Before ending your session: add a `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entry` with current status and next steps.
- Only update stage status when transitioning (not-started → in-progress, in-progress → complete): `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 progress <stage-*> set <status>`.

Session end checklist (do these before you stop):

- Confirm which stages/tests pass and which fail; reflect in `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 progress`.
- Note current focus, immediate next steps, and any blockers as a `tools/bin/santa-journal -d ./impl/python-claudecode-sonnet4-1757409766 entry`.
- List important design decisions and file paths touched in the entry.
- Include the exact command(s) to resume testing in the entry.
