# Changelog

All notable changes to Hawk are documented here. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Note on early versioning.** Before v0.6.0, versioning was inconsistent — patch numbers often contained work that warranted minor or major bumps, and several early commits were tagged or messaged in ways that don't reflect their actual scope. The history is preserved as-tagged. From v0.6.0 onward, version numbers, commit messages, and changelog entries follow consistent conventions.

---

## [0.7.0] — 2026-07-21

The first release with a real test suite. Two sessions of systematic auditing turned up a batch of correctness bugs across parsing, inference, filtering, and rendering — all fixed here — and the codebase gained doctest-based coverage of libhawk, its first user-facing documentation, and native Windows build support. Three dead or misleadingly-named CLI flags were removed or renamed, making this the first release since v0.6.0 with breaking CLI changes.

### Breaking changes

- `distinct --desc` is renamed to `distinct --reverse`. The old spelling is no longer accepted; saved `.hawk` scripts using `--desc` must be updated.
- `--script <file>` is removed. It was parsed but never executed — a silent no-op that the help text actively advertised with an example. It is now rejected as an unknown option. Script mode remains planned work; the flag will return when it does something.
- `--output <file>` is removed. Like `--script` it was parsed and then silently ignored. It is now rejected as an unknown option. Writing results to a file is already covered by `export`.

### Changed

- `peek N` now shows exactly row N. The bare-index form was previously interpreted as a start-open range and printed rows 1–N, contradicting the documented `peek i — row i in current view`. `peek -N` likewise now shows exactly the Nth row from the end rather than the last N rows.
- An invalid `--delimiter` value is now a startup error that names the value and the accepted spellings, and exits 1. Previously the flag was silently discarded and the delimiter inferred instead, replacing an explicitly stated assumption with a heuristic — invisibly so under `--no-confirm`.
- A column whose sampled values are all empty now infers as `String` rather than `Integer`. Empty fields no longer count as evidence of a numeric type, so "no evidence of any type" yields the honest default instead of implying numeric data.
- `distinct` output is now deterministic when two values share a count. Count-mode ties break on value ascending, so repeated runs produce byte-identical output; previously tied entries could reorder between runs, builds, and platforms.
- `set type` now warns whenever it changes a column while the view is non-identity (filtered or sliced), not only when the changed column is the active sort key. The view is deliberately left intact — the warning flags that its membership was computed under the previous type's semantics.
- Non-finite floating-point literals are rejected. `filter <float-column> > inf` (and `-inf`, `infinity`, `nan`) now fails with the usual "cannot be parsed as float" error instead of silently constructing a comparison against a non-finite value, and such values in data count as unparseable rather than flowing into filter comparisons and sort keys.
- Builds default to `Release` when `CMAKE_BUILD_TYPE` is not set.

### Fixed

- Opening a 0-byte file now reports `File is empty (0 bytes)` instead of the opaque `mmap failed`. Both the POSIX and Windows mapping paths guard the empty case before attempting to map.
- Delimiter, column-count, and header detection are now quote-aware. A legitimately quoted field containing the delimiter — `a,"b,c",d` — is no longer treated as a field boundary, so that row infers as 3 columns rather than 4, and a quoted field full of some other delimiter can no longer skew delimiter choice. Inference now reuses the same quote-aware parser the record layer already used, so the whole pipeline agrees on what a field is.
- Field text is truncated and wrapped on UTF-8 codepoint boundaries. Multi-byte characters in long values are no longer split mid-sequence into garbled output, either by the horizontal renderer's truncation or by `--untruncated` wrapping.
- `parse_datetime` rejects trailing content after an otherwise-complete match. A value like `2024-01-01 extra` no longer parses as valid under `YYYY-MM-DD`, so datetime filters no longer match on malformed values that merely start valid.
- `+tz` patterns accept both compact (`-0500`) and colon-separated (`-05:00`) UTC offset spellings, producing the same instant for either.
- Renaming a column to a name another column already holds is rejected. Previously it silently succeeded, leaving two columns sharing a name with subsequent lookups resolving to whichever came first — the wrong column for at least one of them.
- `set type <column> DateTime <pattern>` validates the pattern and rejects an invalid one at set time, instead of accepting it and failing later in an unrelated command with an error disconnected from the mistake.
- `select-` can no longer empty the projection. Repeating a column name inflated the count of columns to drop past the projection's size, underflowing the guard that keeps at least one column; the guard now counts unique remaining columns.
- `peek <range> -u` — range before the flag, the order shown in the documentation — no longer fails with a spurious bound error. The parser stored a view into a destroyed loop-local string, making both argument orders undefined behaviour.

### Added

- A doctest-based test suite covering libhawk broadly: utils, datetime parsing, schema, projection, range resolution, filter, view, type and format inference, and Session integration, plus targeted coverage of the CLI's UTF-8 truncation helpers. Build with `-DHAWK_BUILD_TESTS=ON` and run via `ctest`. Every fix in this release is pinned by tests.
- User-facing documentation: `README.md`, this changelog, `CONTRIBUTING.md`, and a forensic walkthrough.
- `resolve_range` / `ResolvedRange` and `prepare_filter` / `resolve_columns` are now part of libhawk's public surface, so range resolution and filter preparation can be tested and reused directly rather than being reimplemented per command.
- Hawk now builds and runs natively on Windows with MSVC, alongside the pre-existing MinGW cross-compile path. Two portability defects surfaced and were fixed: `trim(std::string_view)` built its result in a way that only compiles where `string_view::iterator` is a raw pointer, and the test target needs `<iostream>` force-included on MSVC because doctest only forward-declares `std::ostream`. Neither changes behaviour on any platform.

### Removed

- The `split` string utility, both overloads. Its two overloads had divergent edge-case semantics, and its last production callers moved to the quote-aware record parser — a byte-based splitter is the wrong tool in a pipeline that is quote-aware end to end.
- The unused `TEST_BUILD` compile flag.

### Notes

- One known divergence on native MSVC builds: Microsoft's STL implementation of `std::chrono::parse` does not apply the day-of-month range check libstdc++ applies, so `parse_datetime` accepts impossible dates such as `2023-02-29` or `2024-04-31` there while rejecting them on Linux. Three assertions in the datetime test suite cover this and currently fail on MSVC only. Native Windows support is new in this release and this is its one known correctness gap.

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
