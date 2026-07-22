# Contributing to Hawk

Thanks for your interest. Hawk is a solo-developed project with strong opinions about scope and architecture, but contributions are welcome within those constraints. This document explains what kinds of contributions fit, how to propose them, and how to work in the codebase.

## Before you contribute

Read [`docs/design.md`](docs/design.md) and [`docs/architecture.md`](docs/architecture.md). Hawk has deliberate non-goals — it is not a SIEM, an ingestion pipeline, a real-time monitor, or a database — and a clear architectural shape. Contributions that pull the project toward those non-goals, or that erode the layer boundaries, will not be merged regardless of how well-written the code is.

The single most important rule: **`libhawk` contains no console I/O, no rendering, no CLI assumptions.** Anything user-facing belongs in `hawk-cli`. This boundary is non-negotiable.

## What kinds of contributions are welcome

**Bug reports.** Always welcome. File an issue with steps to reproduce, expected behaviour, and actual behaviour. Sample input that demonstrates the bug helps a lot.

**Feature suggestions.** Welcome but lower-priority. Describe the *use case* rather than the proposed feature — "I was investigating X and couldn't do Y" is more useful than "please add command Z." Many features that sound reasonable in isolation are out of scope by design.

**Pull requests.** Welcome under the conditions described below.

## Pull request policy

**Discuss before you code, for anything non-trivial.** Open an issue first if your change:

- Adds a new command, flag, or other public-facing surface
- Touches the layer boundaries (libhawk/CLI separation, RecordSource/RecordParser indirection, etc.)
- Changes the shape of a public API (`CommandResult`, `Session`, `LibCommand` variants, etc.)
- Adds a dependency
- Changes inference or filter semantics

Direct PRs without prior discussion are fine for:

- Bug fixes with a clear root cause
- Small refactors with no behavioural change
- Documentation improvements
- Typos and obvious cleanup

If you skip discussion on something substantive, expect the PR to be closed with a request to open an issue first. This is not personal — it protects everyone's time. A 200-line PR that solves the wrong problem is a worse outcome than a five-minute conversation that prevented it.

PRs will be reviewed on a best-effort basis. There is no SLA. The project's first answer to any contribution is "does this fit?" — not "does this work?"

## Setting up to build

Prerequisites:

- C++20 compiler: GCC 14+ (earlier libstdc++ lacks `std::chrono::parse`), Clang 16+, or MSVC 2022
- CMake 3.15+
- IWYU (`include-what-you-use`) — required if you intend to commit; pre-commit hooks rely on it

Build:

```bash
./build.sh                  # Linux release build with sensible defaults
```

Or manually:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the tool against the sample log to verify your build:

```bash
./build/bin/hawk sample_logs/auth.csv
```

Tests are scaffolded but not yet populated. To build with tests enabled:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHAWK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Code style

Hawk does not currently use an automatic formatter. Match the surrounding code. A few conventions that are consistent throughout the codebase and that you should follow:

- **C++20** is the standard. Modern idioms are welcome where they genuinely improve clarity.
- **Naming**: `snake_case` for functions and variables, `PascalCase` for types, `ALL_CAPS` for macros.
- **Anonymous namespaces** over `static` for translation-unit-local definitions in `.cpp` files.
- **Designated initializers** for aggregate construction: `{.field = value}` rather than positional.
- **`'\n'` over `std::endl`** in rendering loops and other tight output paths.
- **`std::format` over string concatenation** for any non-trivial formatted output.
- **Header guards**, not `#pragma once`. Follow the existing naming pattern in the file you're editing.
- **Includes**: pre-commit hooks run IWYU automatically. Don't manually adjust includes to "work around" what IWYU wants; if IWYU is wrong about a specific include, raise it.
- **Comments**: prefer meaningful names over explanatory comments. Don't restate the code. Do document non-obvious *intent* or *invariants*.

Architectural constraints worth knowing before you write code:

- `libhawk` has no console I/O, no rendering, no CLI assumptions. All user interaction lives in `hawk-cli`.
- Don't bypass `RecordSource` / `RecordParser` to read fields. The layering exists for testability and for future format support.
- Don't mutate source data. Filtering, projection, and similar operations produce new `View` objects over the original records.
- Filters are type-strict. Don't introduce silent string-coercion fallbacks.
- Sort must be stable. Forensic reproducibility depends on it.

When in doubt, look at how an existing similar command is implemented and follow that pattern.

## Commit messages

Follow [Conventional Commits](https://www.conventionalcommits.org/) format:

```text
<type>(<optional scope>): <short summary>

<optional longer body>
```

Types in use: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `ci`.

Use a scope when the change is naturally scoped (`feat(history):`, `fix(render):`, `refactor(parsers):`). Omit it when the change is broad or doesn't fit one scope cleanly.

The summary line is imperative present tense ("add filter command," not "added" or "adds"), lowercase after the prefix, no trailing period.

If the body is more than a sentence or two, wrap at ~72 characters. Explain *why*, not *what* — the diff already shows what.

Examples from the project history, for reference:

```text
feat(slice): add slice command for positional view narrowing
fix(render): distribute horizontal table width based on terminal size
refactor(range): unify range resolution between RecordsCommand and SliceCommand
```

## Branches

For your own work, use slash-separated branch names by type:

- `feature/<topic>` for new commands or features
- `fix/<topic>` for bug fixes
- `refactor/<topic>` for refactors with no behaviour change
- `docs/<topic>` for documentation changes
- `tests/<topic>` for test additions

## What won't be merged

Some things are clear non-starters and worth knowing up front:

- Anything that introduces console I/O into `libhawk`.
- Anything that adds runtime dependencies for "convenience" (logging libraries, formatters, framework code).
- Features that pull Hawk toward being a SIEM, an ingestion system, a dashboard, a real-time monitor, or a database engine.
- Changes that silently rewrite or normalize source data.
- Changes that hide or remove the visibility of inference decisions ("just make it work" smartness).
- Performance optimizations that obscure correctness or make behaviour harder to reason about.
- New abstractions that exist for speculative future use rather than current concrete need.

If you're unsure whether a contribution fits, open an issue and ask before writing code.

## Licensing

By submitting a contribution, you agree it can be released under Hawk's [MIT License](LICENSE).
