# Hawk

**Interactive forensic log analysis for the command line.**

Hawk is a C++20 tool for exploring large log files without spinning up a SIEM, writing a script, or loading a notebook. It is designed for analysts who treat logs as evidence: deterministic operations, transparent inference, reproducible sessions, and no silent rewriting of source data.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

---

![Hawk demo](docs/assets/hawk-demo.gif)

---

## Why Hawk

- **Library-first.** Core logic lives in `libhawk`. The CLI is a thin client. The architecture supports scripting, automation, and future frontends without rewrites.
- **Zero-copy where it matters.** Memory-mapped files and `string_view` throughout. Tested on files up to 4 GB and ~2M records on modest hardware.
- **Schema-aware, not schema-heavy.** Type inference is two-phase, transparent, and overrideable. Filters are type-strict — no silent string coercion.
- **Views over mutation.** Every operation produces a derived view. The source file is never rewritten.
- **Forensic reproducibility.** Stable sort. Explicit datetime patterns. Session intent is recorded as commands, not opaque state.

Hawk is not a SIEM, an ingestion pipeline, a real-time monitor, or a database. Those are deliberate non-goals — see [`docs/design.md`](docs/design.md).

## Quick start

Clone the repository, then:

```bash
cd hawk
./build.sh
./build/bin/hawk sample_logs/auth.csv
```

If you just want to try Hawk without building from source, pre-built binaries are available on the releases page.

Inside the REPL:

```
hawk> help
hawk> columns
hawk> head 5
hawk> filter result == "FAILURE"
hawk> distinct source_ip
hawk> exit
```

## A short walkthrough

The repo ships with `sample_logs/auth.csv` — 182 SSH authentication records covering a morning of activity on a small server. Somewhere in those records is a successful compromise. Hawk's job is to help you find it.

```
$ ./build/bin/hawk sample_logs/auth.csv --delimiter ',' --header

hawk> count
Count: 182

hawk> distinct result
Found 2 distinct values for 'result' (182 total rows):

  Value     Count
  --------  -----
  SUCCESS      93
  FAILURE      89

hawk> filter result == "FAILURE"

hawk> distinct source_ip
Found 3 distinct values for 'source_ip' (89 total rows):

  Value           Count
  --------------  -----
  185.220.101.47     85
  10.0.1.87           2
  10.0.2.103          2

hawk> reset --view

hawk> filter source_ip == "185.220.101.47"

hawk> filter result == "SUCCESS"

hawk> peek 1
      Record #128
---------------------------------
 timestamp   2026-01-29 10:26:37
 source_ip   185.220.101.47
 username    root
 auth_method password
 result      SUCCESS
 message     Accepted password
 port        35557
```

That's the breach. The full walkthrough — including post-compromise pivot, second attacker IP, and evidence export — is in [`docs/walkthrough.md`](docs/walkthrough.md).

## Building

**Prerequisites**

- C++20 compiler: GCC 12+, Clang 16+, or MSVC 2022
- CMake 3.15+

**Linux / macOS**

```bash
./build.sh                       # release build with sensible defaults
```

Or manually:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Windows**

Hawk is regularly built for Windows via MinGW-w64 cross-compilation with static linking. Detailed instructions will be added to the docs.

## Project layout

```
hawk/
├── libhawk/              # Core engine. No console I/O.
│   ├── include/hawk/     # Public headers
│   └── src/              # Implementation
├── hawk-cli/             # REPL and rendering. Thin client of libhawk.
├── sample_logs/          # Realistic sample data
└── docs/                 # Design, architecture, walkthrough
```

`libhawk` knows nothing about terminals, colors, or commands. `hawk-cli` is the first frontend; the architecture supports others (scripting, automation, alternative UIs) without modifying the core.

For the full architecture, see [`docs/architecture.md`](docs/architecture.md).

## Command surface

| Command         | Purpose                                 |
|-----------------|-----------------------------------------|
| `columns`       | List columns and inferred types         |
| `head` / `tail` | Show first/last N rows                  |
| `peek`          | Show a single row vertically            |
| `count`         | Count rows in the current view          |
| `filter`        | Narrow the view by a predicate          |
| `filter+`       | Union additional matching rows          |
| `filter-`       | Subtract matching rows                  |
| `distinct`      | Show distinct values and counts         |
| `sort`          | Stable sort by a column                 |
| `select`        | Project a subset of columns             |
| `set type`      | Override an inferred column type        |
| `set name`      | Rename a column                         |
| `reset`         | Reset view, projection, or sort         |
| `export`        | Export the current view to a file       |
| `config`        | Show current session settings           |
| `history`       | Show or save the session's command log  |

Run `help <command>` inside the REPL for usage and flags.

## Design philosophy

Hawk values restraint, correctness, and clarity over breadth. A few principles that shape every design decision:

- **Logs are evidence, not just text.** Provenance and integrity matter.
- **Investigations are iterative.** Sessions support refinement, not one-shot queries.
- **Intent over output.** Replaying an analysis means replaying commands, not snapshots.
- **Predictable behaviour beats cleverness.** Surprising tools are dangerous in forensic work.

The full design document is in [`docs/design.md`](docs/design.md).

## Documentation

- [`docs/design.md`](docs/design.md) — Project philosophy, scope, and architectural principles
- [`docs/architecture.md`](docs/architecture.md) — Repository layout and developer guide
- [`docs/walkthrough.md`](docs/walkthrough.md) — Realistic forensic investigation with the sample log

## License

MIT — see [`LICENSE`](LICENSE).
