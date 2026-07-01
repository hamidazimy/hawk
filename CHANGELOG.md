# Changelog

All notable changes to Hawk are documented here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Note on early versioning.** Before v0.6.0, versioning was inconsistent — patch numbers often contained work that warranted minor or major bumps, and several early commits were tagged or messaged in ways that don't reflect their actual scope. The history is preserved as-tagged. From v0.6.0 onward, version numbers, commit messages, and changelog entries follow consistent conventions.

---

## [0.6.9] — 2026-06-27

### Added

- `export` now prints a confirmation message reporting the number of records written and the destination path.

## [0.6.8] — 2026-06-27

### Changed

- Horizontal table rendering now distributes column widths dynamically based on terminal size. Each column gets its natural width when there's room; when the total exceeds the terminal, budget is fair-shared starting from the narrowest columns. Replaces the previous hardcoded 20-character cap, which truncated useful content even on wide terminals.

### Fixed

- Falls back to a minimum column width when the terminal can't hold the index gutter and separators, wrapping rows rather than truncating to nothing.

## [0.6.7] — 2026-06-10

### Fixed

- Inference spinner no longer leaves `\r` overwrite artifacts when stderr is redirected to a file or pipe. The spinner now checks `isatty` on stderr before starting its thread.

### Added

- `console::is_tty`, `stdin_is_tty`, `stdout_is_tty`, `stderr_is_tty` helpers wrapping platform-specific `isatty` calls.

## [0.6.6] — 2026-06-10

### Changed

- Errors and warnings now route to stderr. Analyst-facing output (payloads, success messages, info, execution time) stays on stdout. Matches standard Unix conventions and what comparable tools do.
- `RenderContext` gains a `serr` field alongside `sout`. Both streams are threaded explicitly, paving the way for upcoming test infrastructure.

## [0.6.5] — 2026-06-10

### Changed

- Range resolution unified between `RecordsCommand` (head/tail/peek) and `SliceCommand`. Both now hold a `Range { optional<RangeBound> start, end }` and resolve through the same helper. CLI uses 1-based inclusive convention; lib uses 0-based half-open; translation happens at the boundary.

### Fixed

- `head N` / `tail N` with N larger than the current view now return what's available plus a "Range clamped" warning, matching slice's behaviour. Previously these commands had inconsistent handling.
- `peek #-N` (negative index on raw file paths) now works correctly.
- Inverted ranges (e.g. `5:3`) and fully-out-of-bounds ranges produce consistent error messages across slice, peek, head, and tail.
- Inversion is checked against pre-clamp signed values, so cases like `slice 4:2` on a 1-row view correctly error rather than masquerading as clamped-empty.

## [0.6.4] — 2026-06-09

### Added

- `slice` command for positional view narrowing, complementing `filter`'s predicate-based narrowing. Syntax: `slice N`, `slice -N`, `slice i:j`, `slice :j`, `slice i:`, `slice -i:-j`. Range syntax is 1-based and inclusive at the CLI; the lib speaks Python-style half-open intervals. Slice composes with filter, sort, projection, and export; `reset --view` undoes it.
- `RangeBound` type in `types.hpp` carrying range endpoints through the lib. The `resolve_range` helper performs negative resolution, clamping, and inversion/out-of-bounds detection.

## [0.6.3] — 2026-06-08

### Added

- `history` command. Records all commands typed at the REPL into an in-memory vector. `history` prints them to stdout; `history --save <path>` (or `-s <path>`) writes them to a file, one per line. Saved files are directly replayable via stdin redirect: `hawk logs.csv < session.hawk`.
- Comment support: lines beginning with `#` are recorded but not dispatched, so saved scripts can be annotated.
- `history`, `help`, and `exit` (plus aliases) are excluded from history via dynamic alias resolution through `command_table`. New aliases are excluded automatically.

### Notes

- History capture and replxx's line-editor history are deliberately separate. Comments go to ours but not replxx; excluded commands go to replxx but not ours.
- The richer history design (mutating-vs-readonly distinction, filtered replay, undo foundation) is deferred to a future release.

## [0.6.2] — 2026-05-26

### Changed

- Renderer now sanitizes control characters and strips CSV quote characters from displayed field values.
- Rendering symbols (separators, ellipsis, indicators) extracted into named constants.

## [0.6.1] — 2026-05-21

### Added

- `config` command surfacing the session settings established at build time: delimiter, header presence, line endings, case sensitivity. Implemented as a lib-backed command (`ConfigCommand` → `ConfigResult`) rather than a CLI-only accessor read, keeping the public surface uniform for future language bindings.

### Notes

- Session configuration is assumed immutable for the lifetime of a session; mid-session config changes are an explicit non-goal.

## [0.6.0] — 2026-05-20

A structural rework of the CLI layer. All analyst-facing commands now route through a single command table and dispatch loop. This is the version where the architecture documented in `docs/architecture.md` settled.

### Changed

- All analyst-facing commands are now `CliCommand` variants. The previous split between `LibCommand` and `CliCommand` dispatch loops is replaced by a single command table and a single dispatch loop in the REPL.
- `cli_commands.hpp`, `command_info.hpp`, and `command_tables.hpp` collapsed into `commands.hpp` and `command_table.hpp`.
- All lib commands are wrapped in corresponding `Cli*` structs (`CliFilter`, `CliSort`, `CliDistinct`, etc.).
- `HeadCommand`, `TailCommand`, `PeekCommand`, and `RowsCommand` replaced by `RecordsCommand` with optional start/end/raw fields. `RowsResult` renamed to `RecordsResult`.
- `RenderContext` introduced to carry schema, terminal width, and output stream through the rendering pipeline.
- `RenderOptions` introduced for per-call rendering preferences (`DisplayMode`).
- `dispatch()` helper unifies lib command execution and result rendering.
- `ExitRequested` exception replaces the previous bool return value for exit signalling.

### Added

- Vertical and untruncated display modes for `peek`, `head`, and `tail`.
- Command aliases supported in the dispatch loop and help system.

## [0.5.4] — 2026-05-19

### Changed

- `HeadCommand`, `TailCommand`, `PeekCommand`, and `RowsCommand` consolidated into a unified `RecordsCommand`. `RowsResult` renamed to `RecordsResult`. Preparatory refactor for the v0.6.0 CLI rework.

## [0.5.3] — 2026-05-17

### Added

- replxx integration for line editing, command recall, and history at the REPL prompt.
- Windows console setup for ANSI and UTF-8 support.
- `--no-color` flag, `NO_COLOR` environment variable support, and `isatty` detection for automatic color suppression in non-interactive contexts.
- Inference spinner shown during file format inference.
- Column numbers in row headers, raw file index alongside view index in row output, and Unicode ellipsis for truncation indicators.
- `peek` command with view-based indexing and `#N` syntax for raw file index access.

## [0.5.2] — 2026-05-15

### Added

- `reset --view`, `reset --proj`, and `reset --sort` flags for targeted state reset.
- `filter *` and `select *` aliases for `reset --view` and `reset --proj`.

## [0.5.1] — 2026-05-15

### Added

- `--ignore-case` startup flag for global case-insensitive operation. Affects `filter`, `distinct`, and column name resolution. Threaded explicitly through `resolve_columns` and `prepare_filter` rather than relying on global state.

### Notes

- Case sensitivity is fixed at session start; mid-session changes are a deliberate non-goal.

## [0.5.0] — 2026-05-14

### Added

- `distinct` command. Shows distinct values for a column with their counts, type-aware sorting, and a warning for high-cardinality output.
- `sort` command. Stable sort (Schwartzian transform); `active_sort_` state maintained by `Session` and re-applied after `filter+`. Stability is required — forensic reproducibility depends on it.
- `filter+` and `filter-` commands for incremental view expansion (union with full file) and exclusion (subtract matching rows).
- `select+` and `select-` commands for incremental projection modification.

## [0.4.26] — 2026-05-12

### Changed

- Parser tokenizer is now quote-aware, with improved validation and column range expansion (`$colN:M` syntax).

## [0.4.25] — 2026-05-11

### Added

- Detailed help system with per-command detail pages and `--help` flag interception at the REPL.

## [0.4.24] — 2026-05-01

### Changed

- `export` moved from libhawk to the CLI layer, consistent with the "no console I/O in libhawk" boundary. `RowsCommand` added; line endings and projection handling fixed in the process.

## [0.4.23] — 2026-04-29

### Added

- `has` operator for substring search on string columns.
- `$row` sentinel for full-row substring search via `RowSearchPredicate`, which scans raw record bytes and bypasses field parsing entirely.

### Fixed

- Datetime pattern shadowing in `FilterPredicate` constructor.

## [0.4.22] — 2026-04-28

### Changed

- `CommandResult` wrapped in a structured envelope: `{ payload, error, warnings, execution_time_ms }`. Established invariants: `error` set implies no payload; absent payload and absent error means silent success; `execution_time_ms` is always populated.
- Free functions in `session.cpp` moved into anonymous namespaces, replacing `static`.

## [0.4.21] — 2026-04-24

### Added

- Pattern-based datetime parsing via `std::chrono::parse`. User-facing pattern vocabulary (`YYYY MM DD hh mm ss ss.f+ Z +tz`) replaces the previous internal `DateTimeFormat` enum. `ColumnSchema` stores a pattern string instead of an enum value.
- Two-phase type inference for datetime columns: full parse on a bounded sample, then a cheap structural pre-screen on all rows. **Reduces startup time on a 620k-row file from 3612 ms to 287 ms.**
- `parse_int` helper in `hawk::utils`.
- `SetColumnTypeCommand` extended to accept a datetime pattern.

### Changed

- `FilterPredicate` is now fully type-aware. `Integer` uses `int64_t` throughout, `Float` uses `double`, `DateTime` compares as `int64_t` ticks.

## [0.4.20] — 2026-04-24

### Changed

- Upgraded to C++20 throughout.

## [0.4.0] — 2026-04-18

A foundational restructuring. Session bootstrapping, schema typing, and the groundwork for strict type-aware filtering all landed together.

### Added

- `SessionBuilder` as the single entry point for file access and session construction. File is opened once and shared between inference and the session.
- `SessionConfig` as an always-complete struct. Partial config states removed.
- `TypeInferrer` for full-scan column type inference (`Integer`, `Float`, `DateTime`, `String`) with nullable detection and datetime format identification.
- `ColumnSchema` and `DateTimeFormat` (`column.hpp`). Datetime precision covers seconds through 100-nanosecond ticks, epoch variants, and common log formats.

### Changed

- `Schema` redesigned around `vector<ColumnSchema>`. Redundant mutators removed.
- `Session` constructor now accepts a fully constructed `Schema`. No inference or construction logic lives inside `Session`.
- `FormatInferer` sampling fixed (was disabled). Detectors unified to a `bool` / output-parameter signature. Detection order corrected.
- Public API surface in `hawk.hpp` trimmed; internal headers removed.

### Removed

- `Session::create_from_file()` — use `SessionBuilder::open()` + `build()`. `SessionConfig` no longer has optional fields or inference methods.

### Breaking changes

- Session construction API; `SessionConfig` shape.

## Pre-0.4.0 (exploratory)

The project's first month was exploratory. The core data flow (`LogFile` → `RecordSource` → `RecordParser` → `Row` → `View`) took shape, and a small set of commands (`filter`, `head`, `tail`, `column`, `count`, `peek`, `export`) was implemented. Several refactors during this period weren't versioned, and the tagged versions in this range don't always correspond to meaningful release boundaries.

Notable work during this phase, in roughly chronological order:

- **0.1.0** (2026-03-06) — Initial commit.
- **0.2.0** (2026-03-16) — Core architecture restructured. `LogFile` simplified to physical file access only. `RecordSource` abstraction introduced for logical record access. `CSVRecordSource` and `CSVRecordParser` added.
- **0.2.1** (2026-03-17) — `View` refactored; identity view added.
- **0.2.2** (2026-03-17) — `Row` refactored to use `string_view` throughout.
- **0.2.3** (2026-03-30) — `column`, `count`, `peek`, `tail`, and `export` commands added. Pre-commit IWYU hooks added.
- **0.2.4** (2026-03-30) — Lazy `Row` field parsing; `FilterPredicate` moved out of `Session`.
- **0.3.0** (2026-04-13) — `Projection` class introduced; `select` command added.

Cross-platform file mapping, the initial filter command, UTF-8 BOM handling, and the Windows cross-compile script all landed during this phase.
