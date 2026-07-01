# Hawk Repository Layout & Developer Guide

Hawk separates the CLI shell (`hawk-cli`) from the core analysis engine (`libhawk`), following the 'library-first architecture' principle. Core domain logic lives in libhawk with no console I/O. The CLI handles interaction, rendering, and command parsing as a thin adapter.

## hawk-cli: Interactive Shell

**Purpose:** User-facing REPL and command surface. Parses input, dispatches to `libhawk::Session`, renders structured results.

```text
hawk-cli/
├── CMakeLists.txt                  # Executable build
├── src/
│   ├── main.cpp                    # Entry: parse args → SessionBuilder → build config → Session → REPL
│   ├── args.hpp/cpp                # CLI args: log_file, --delimiter, --header, --ignore-case, --no-color
│   ├── constants.hpp               # Shared constants
│   ├── helpers/                    # Helpers, including utils, etc.
│   │   ├── config_builder.hpp/cpp      # merge_config (args + inference + confirm), confirm_schema
│   │   ├── console.hpp/cpp             # console:: sub-namespace — terminal width query,
│   │   │                               #  TTY detection (is_tty / stdin_is_tty / stdout_is_tty /
│   │   │                               #  stderr_is_tty), Windows console setup (ANSI + UTF-8)
│   │   ├── output_decorator.hpp/cpp    # SGR colors (info/error/warning). Global color enable flag.
│   │   ├── spinner.hpp                 # RAII spinner for long-running operations (header-only)
│   │   └── utils.hpp/cpp               # Delimiter parsing, quote-aware tokenize, misc
│   └── cli/                        # REPL + command surface
│       ├── repl.hpp/cpp                # REPL: owns Session and replxx editor, runs loop, executes CliCommand
│       ├── renderers.hpp/cpp           # Formats CommandResult via RenderContext + RenderOptions
│       ├── parsers.hpp/cpp             # Raw input → typed CliCommand
│       ├── commands.hpp                # CliCommand structs, CliCommand variant, Modes, CommandInfo struct
│       └── command_table.hpp           # Static command_table array (includes commands.hpp + parsers.hpp)
└── version.hpp.in                  # Version injection
```

**Key flow (`main.cpp`):**

```text
setup_console()                          # Windows: enable ANSI, set UTF-8 code page
set_color_enabled(color_enabled); → bool         # set global color flag based on --no-color, NO_COLOR env, isatty
parse_args() → Args
SessionBuilder::open(log_file) → SessionBuilder
build_config(args, record_source) → SessionConfig (inference + confirm, spinner shown during inference)
builder.build(config) → Session (type inference runs here)
confirm_schema(session, args) → analyst reviews inferred column types
REPL(session).run() → interactive loop (replxx-backed)
```

**Commands (current set from `command_table.hpp`):**

All commands are `CliCommand` variants. The distinction between lib-backed and CLI-only is an
implementation detail of `REPL::execute_impl`, not a user-facing concept.

```text
config                                                      Show current session settings
columns                                                     List columns with types
set type <col> <type> [pattern]                             Override inferred column type
set name <col> <newname>                                    Rename a column
select <cols>                                               Set column projection
select+ <cols>                                              Add columns to projection
select- <cols>                                              Remove columns from projection
count                                                       Count rows in current view
peek [i|-i|i:j|-i:-j|#i|#-i|#i:j|#-i:-j] [-u|--untruncated] Show one or more rows in vertical layout
head [n] [-v|--vertical] [-u|--untruncated]                 Show first n rows
tail [n] [-v|--vertical] [-u|--untruncated]                 Show last n rows
filter <col> <op> <value>                                   Narrow view by predicate
filter+ <col> <op> <value>                                  Expand view (union with full file)
filter- <col> <op> <value>                                  Shrink view (subtract matching rows)
sort <col> [--desc|-r]                                      Sort current view
distinct <col> [-v|--sort-by-value] [-r|--desc]             Show distinct values and counts
slice [n|-n|i:j|:j|i:|-i:-j]                                Narrow view by row position
reset [--view|-v] [--proj|-p] [--sort|-s]                   Reset session state
export <path> [--projected|-p]                              Export current view to file
history [--save|-s <path>]                                  Show or save command history
help [command]                                              Show help
exit                                                        Exit Hawk
```

**Aliases:**

- `cols` → `columns`, `first` → `head`, `last` → `tail`, `uniq` → `distinct`
- `h`, `?` → `help`, `quit` → `exit`
- `filter *` → `reset --view`, `select *` → `reset --proj`, `sort --default|-d` → `reset --sort`

**Parser notes:**

- `utils::tokenize` is quote-aware by default.
- `select`, `select+`, `select-` accept comma/space/mixed column lists. Range syntax `$colN:M` expanded at parse time.
- `--help` flag intercepted by REPL before dispatch — any command supports `<cmd> --help`.
- `--ignore-case` at startup sets global case-insensitive mode for all string operations.
- `--no-color` disables SGR color output. `NO_COLOR` env var and non-tty stdout also disable color.

## libhawk: Core Engine

**Purpose:** Reusable, deterministic log analysis library. Memory-efficient, zero-copy where possible (string_views). No I/O or UI assumptions.

```text
libhawk/
├── CMakeLists.txt          # Library build
├── iwyu.imp                # Include-what-you-use linting
├── include/hawk/
│   ├── hawk.hpp            # Main entry (forwards to core/)
│   ├── utils/
│   │   ├── utils.hpp               # String helpers (trim/split/tokenize/parse_int/parse_double/
│   │   │                           #  contains/compare_strings/to_lower)
│   │   ├── format_inference.hpp    # Delimiter/header detection + diagnostics
│   │   ├── type_inference.hpp      # Two-phase column type inference (sample-based datetime
│   │   │                           #  detection + pre-screen validation pass)
│   │   └── datetime_parser.hpp     # Pattern-based datetime parsing via std::chrono::parse.
│   │                               # User-facing vocabulary: YYYY MM DD hh mm ss ss.f+ Z +tz
│   ├── platform/
│   │   └── file_mapping.hpp        # OS abstraction (POSIX/Windows)
│   └── core/                   # Domain model
│       ├── types.hpp               # FileOffset, RecordIndex, RecordCount, RangeBound, Range, etc.
│       ├── log_file.hpp            # Raw bytes (BOM/CRLF detection). Exposes has_crlf().
│       ├── record_source.hpp       # Logical records (CSV impl indexes offsets)
│       ├── record_parser.hpp       # Fields from record (CSV impl)
│       ├── row.hpp                 # Lazy field access (memoized parse)
│       ├── column.hpp              # ColumnSchema, ColumnType
│       ├── schema.hpp              # vector<ColumnSchema> backed. find_column() is case-sensitivity aware.
│       ├── session_config.hpp      # Always-complete config struct (delimiter, has_header, use_crlf,
│       │                           #  case_sensitive)
│       ├── session_builder.hpp     # File ownership, inference, construction
│       ├── session.hpp             # Orchestration: config → source/parser → view/projection/sort
│       ├── view.hpp                # Derived indices (identity/filtered/sorted). sort_to_file_order().
│       ├── projection.hpp          # Column subset. Supports add/drop/select/reset/is_identity.
│       ├── commands.hpp            # LibCommand variant (pure data). FilterArgs base for filter variants.
│       │                           #  Range-using commands carry hawk::Range.
│       ├── filter.hpp              # FilterPredicate + RowSearchPredicate + FilterPredicateVariant.
│       │                           #  Type-strict, no silent fallback.
│       └── results.hpp             # CommandResult wrapper + ResultPayload variant
└── src/                    # .cpp matching headers
    ├── core/, platform/, utils/
```

### Core data flow

```text
SessionBuilder::open(path) → RecordSource (CSV offsets, file owned)
FormatInferer::infer(record_source) → FormatInferenceResult
SessionConfig (confirmed by CLI, includes case_sensitive)
SessionBuilder::build(config) →
    RecordParser (CSV fields)
    TypeInferrer::infer() → Schema (typed columns)
    Session (source + parser + schema + identity_view + current_view + projection + active_sort)
Session::execute(LibCommand) → CommandResult
```

### CommandResult structure

All commands return a `CommandResult` wrapper. The core library never returns formatted text.

```cpp
struct CommandResult {
    std::optional<ResultPayload> payload;             // absent for commands with no data to return
    std::optional<std::string>   error;               // set means command did not execute
    std::vector<std::string>     warnings;            // non-fatal diagnostics, always safe to populate
    uint64_t                     execution_time_ms;   // set by Session::execute(), never execute_impl()
};
```

Invariants:

- `error` set → `payload` absent. `warnings` may still be present.
- `payload` absent + `error` absent → silent success (e.g. `reset`, `set name`, `set type`).
- `execution_time_ms` is always populated, even for failed commands.

`ResultPayload` is a variant of: `RecordsResult`, `CountResult`, `FilterResult`, `ColumnsResult`,
`DistinctResult`, `SortResult`, `SliceResult`, `ConfigResult`.

Export is a CLI concern — `CliExport::execute_impl` calls `RecordsCommand::all_view_records()`,
fetches rows, and writes via `render_export`.

### Key invariants

- **Zero-copy**: `string_view` everywhere (bytes → records → fields).
- **Views over mutation**: `View` maps indices, never rewrites data.
- **Structured results**: Typed `CommandResult` wrapper, no formatted text in library.
- **Explicit**: Config overrides inference; predicates show coercion path.
- **Declarative**: Commands are pure data, not methods.
- **Type-strict filters**: `FilterPredicate` dispatches purely on `ColumnType`. RHS validated before
  scan. Unparseable fields counted in `FilterResult::skipped` and surfaced as warnings.
- **Row search**: `RowSearchPredicate` handles `$row has <value>` — searches raw record bytes,
  bypasses field parsing entirely, ignores projection.
- **Filter variants**: `FilterExpandCommand` scans identity view, unions matches into current view,
  re-sorts to file order, re-applies `active_sort_`. `FilterExcludeCommand` negates predicate.
- **Sort state**: `Session` maintains `std::optional<SortKey> active_sort_`. Re-applied after
  `FilterExpandCommand`. Cleared on `reset --view` and full `reset`.
- **Pattern-based datetime**: `ColumnSchema::datetime_pattern` holds user-facing pattern string.
  `parse_datetime` translates to `std::chrono::parse` format strings at parse time.
- **Projection**: `Projection` tracks active column subset. `is_identity()` true when all columns
  selected in order. `add`/`drop` support incremental modification. `reset()` restores all columns.
- **Case sensitivity**: `SessionConfig::case_sensitive` governs all string operations. Passed
  explicitly through `resolve_columns` and `prepare_filter`; never assumed.
- **Records access**: `RecordsCommand{range, raw}` replaces Head/Tail/Peek/Rows commands.
  `raw=true` bypasses view and accesses the file directly. The CLI translates user-facing
  1-based inclusive ranges to the lib's 0-based half-open Python-style convention before
  constructing the command.
- **Range handling**: `Range { optional<RangeBound> start, end }` is shared by `RecordsCommand`
  and `SliceCommand`. `RangeBound` is signed; negative values count from the end (Python
  semantics). `resolve_range(range, total)` performs negative resolution, clamping, and
  inversion detection in one place. Callers decide how to treat inverted or fully-out-of-bounds
  ranges (slice and records both treat them as errors).
- **Slice**: `SliceCommand{range}` narrows the current view positionally. Composes with filter,
  sort, projection, and export. `reset --view` undoes it. To slice the file directly rather
  than the current view, use `reset --view` first.

## Session: The Coordination Layer

**Single entry point**: `SessionBuilder::build(config)` → `Session` owns
source/parser/schema/identity_view/current_view/projection/active_sort.

**Execute flow (typed dispatch):**

```text
Session::execute(LibCommand)
    → measures execution time
    → std::visit → execute_impl(cmd)
    → CommandResult (payload + warnings + error + execution_time_ms)
```

Current `execute_impl` dispatch:

- `ColumnsCommand` → `ColumnsResult`
- `CountCommand` → `CountResult`
- `RecordsCommand` → `RecordsResult` (view or raw file slice, respects `raw` flag)
- `FilterCommand` → `prepare_filter` → `view_.filter(predicate)` → new View, `FilterResult`
- `FilterExpandCommand` → scans identity view, unions matches, re-sorts, re-applies sort → `FilterResult`
- `FilterExcludeCommand` → `view_.filter(!predicate)` → new View, `FilterResult`
- `SortCommand` → Schwartzian transform → stable sort → sets `active_sort_` → `SortResult`
- `DistinctCommand` → frequency map over current view → `DistinctResult`
- `SelectCommand` → `projection_.select(indices)` → silent success
- `SelectAddCommand` → `projection_.add(indices)` → silent success
- `DeselectCommand` → `projection_.drop(indices)` → silent success
- `SetColumnTypeCommand` → `schema_.set_column_type(...)` → silent success
- `SetColumnNameCommand` → `schema_.set_column_name(...)` → silent success
- `ResetCommand` → resets view/projection/sort per flags → silent success
- `SliceCommand` → `view_.slice(...)` → new View, `SliceResult`
- `ConfigCommand` → `ConfigResult` (snapshot of session config)

Note: `history` is CLI-only. The REPL maintains the command log in memory and
writes it directly when requested; there is no `HistoryCommand` in libhawk
and no corresponding `execute_impl`.

### Session private helpers

**Member functions** (need `this`):

- `find_column(name)` — delegates to `schema_.find_column(name, config_.case_sensitive)`
- `apply_predicate(pred_variant, idx)` — dispatches `FilterPredicate` or `RowSearchPredicate`
- `apply_sort(key)` → `RecordCount` — Schwartzian transform, returns unparseable count

**Free functions in anonymous namespace** (pure, no `this`):

- `resolve_columns(schema, names, out, case_sensitive)` — batch column name → index resolution
- `prepare_filter(schema, args, case_sensitive)` → `PrepareFilterResult` — validates and builds predicate
- `make_sort_key(field, col_type, col_schema)` → `ComparableKey` — type-aware key for sorting
- `compare_sort_keys(ka, kb, is_desc)` → `bool` — null-safe typed key comparison

## CLI Rendering Pipeline

The CLI rendering pipeline is driven by two context objects:

```cpp
struct RenderContext {
    const hawk::Schema& schema;          // for column name/type lookup
    std::size_t         terminal_width;
    std::ostream&       sout;            // analyst-facing output (stdout)
    std::ostream&       serr;            // diagnostics (stderr)
};

struct RenderOptions {
    DisplayMode display_mode = DisplayMode::Horizontal;
    // future: line numbers, type annotations, max rows, etc.
};
```

**`render_result(ctx, result, options)`** is the primary entry point. It handles error, payload
dispatch via `std::visit`, success, warnings, and execution time uniformly.

**`render_impl` overloads** handle each `ResultPayload` type. All take `RenderOptions` for
interface consistency even if unused.

**`RecordsResult` rendering** dispatches on `options.display_mode`:

- `Horizontal` — tabular with truncation, column numbers in header, raw row index
- `Vertical` — one field per line, truncated to terminal width
- `VerticalUntruncated` — one field per line, full value with line wrapping

**`render_export`** writes directly to a file stream via a separate `RenderContext` with `sout`
pointing to the file. Respects source file delimiter and line endings.

**Utility renderers** (`render_info`, `render_success`, `render_execution_time`) take
`std::ostream&` directly — no `RenderContext` needed.

**Diagnostic renderers** (`render_error`, `render_warning`, `render_warnings`) have a dual
signature: a single-argument form that writes to `std::cerr`, and a two-argument form that
writes to the stream the caller provides. `render_result` uses the two-argument form with
`ctx.serr`; REPL exception handlers and startup errors in `main.cpp` use the single-argument
form. Errors and warnings always go to stderr; payload, success, info, and execution time
always go to stdout.

## REPL: Dispatch and Execution

**Single dispatch loop** — one `command_table`, one loop, aliases checked via `matches()`.

**`REPL::dispatch(LibCommand, RenderOptions)`** — executes a lib command and renders the result.
Used by trivial `execute_impl` functions that are pure CLI→lib translations:

```cpp
void REPL::execute_impl(const CliCount&) { dispatch(CountCommand{}); }
void REPL::execute_impl(const CliSort& cmd) { dispatch(SortCommand{cmd.column, cmd.is_desc}); }
```

**Non-trivial `execute_impl` functions** do additional work before dispatching:

- `CliPeek` / `CliSlice` — apply `translate_to_lib(CliRange)` to convert the analyst-convention
  range into a lib-convention `hawk::Range`, then dispatch. The translation helper lives in
  `repl.cpp` anonymous namespace; the parsers produce `CliRange` directly.
- `CliHead` / `CliTail` — construct `RecordsCommand` with `range = {nullopt, n}` (head) or
  `range = {-n, nullopt}` (tail), letting the lib handle clamping if `n` exceeds view size.
- `CliExport` — opens file, sets write buffer, calls `render_export`, prints
  "Exported N records to \<path\>" on success, surfaces warnings to stderr.
- `CliHistory` — prints recorded commands to stdout, or writes them to a file with `--save`.
- `CliHelp` — renders command detail or summary from `command_table` directly.

### Extension Points

#### Add a command

1. Define `CliXxx` struct in `commands.hpp`. Add to `CliCommand` variant.
2. Add parser to `parsers.hpp/cpp`. Add entry to `command_table.hpp`.
3. If it calls a lib command: add lib struct to `libhawk/commands.hpp`, add to `LibCommand` variant,
   implement `Session::execute_impl`. Add result type to `results.hpp` if needed.
4. Implement `REPL::execute_impl(CliXxx)` in `repl.cpp` — use `dispatch()` if it is a
   straight lib translation, or custom logic if it needs pre/post-processing.
5. Add `render_impl` overload in `renderers.cpp` if a new result type was introduced.

#### Add format support

1. New `RecordSource` subclass (e.g., `JSONLinesRecordSource`).
2. New `RecordParser` subclass.
3. Extend `SessionConfig` + inference.

#### Platform extension

Subclass `platform::FileMapping` for new OS.

## Developer Mental Model

**Think in layers — never bypass:**

```text
user input → parsers → CliCommand → REPL::execute_impl
                                         ↓ (most commands)
                               REPL::dispatch(LibCommand)
                                         ↓
                               Session::execute(LibCommand) → CommandResult
                                         ↓
                               renderers::render_result(ctx, result, options)
                                         ↓
                                      console
                                         ↑
                                 libhawk boundary
                                         ↓
bytes(LogFile) → records(RecordSource) → fields(Row) → schema → view → projection
```

**Debugging path:**

```text
$ hawk logs.csv --ignore-case        # inference with spinner → SessionConfig → Session
> columns                            # CliColumns → ColumnsCommand → ColumnsResult
> head 5                             # CliHead → RecordsCommand{{nullopt,5},false} → RecordsResult
> head 3 --vertical                  # CliHead → RecordsCommand{{nullopt,3},false} → vertical
> peek 42                            # CliPeek → translate_to_lib(CliRange{42,42}) → vertical
> peek #100                          # CliPeek → translate_to_lib(...) raw=true → file row 100
> peek -1                            # CliPeek → last row of view
> filter timestamp > "2024-01-01"    # CliFilter → FilterCommand → view.filter() → new View
> filter $row has "error"            # CliFilter → FilterCommand(row_search) → RowSearchPredicate
> filter+ event_id == 4625           # CliFilterExp → FilterExpandCommand → union → re-sort
> filter- source_ip == 10.0.0.1      # CliFilterExc → FilterExcludeCommand → subtract
> select event_id, message           # CliSelect → SelectCommand → projection_.select()
> select+ source_ip                  # CliSelectAdd → SelectAddCommand → projection_.add()
> sort timestamp                     # CliSort → SortCommand → Schwartzian → active_sort_ set
> slice 100                          # CliSlice → SliceCommand{{nullopt,100}} → first 100 rows
> slice -100:                        # CliSlice → SliceCommand{{-100,nullopt}} → last 100 rows
> config                             # CliConfig → ConfigCommand → ConfigResult
> history                            # CliHistory → recorded commands to stdout
> history --save script.hawk         # CliHistory → recorded commands to file
> distinct level                     # CliDistinct → DistinctCommand → frequency map
> count                              # CliCount → CountCommand → current_view_.size()
> export results.csv --projected     # CliExport → RecordsCommand::all_view_records() → file
> reset --view                       # CliReset → ResetCommand → identity view, clear sort
> reset                              # CliReset → ResetCommand → reset everything
```

### Common Pitfalls

- ❌ Don't put IO/rendering in libhawk.
- ❌ Don't bypass RecordSource/RecordParser.
- ❌ Don't mutate source data — return new View.
- ❌ Don't hide inference — expose notes/guesses.
- ❌ Don't add export logic to libhawk — export is a CLI concern.
- ❌ Don't call `schema_.find_column` directly in `execute_impl` — use `Session::find_column`.
- ❌ Don't forget to clear `active_sort_` on `reset --view` and re-apply after `filter+`.
- ❌ Don't add new `HeadCommand`/`TailCommand`/`PeekCommand` variants — use `RecordsCommand`.
- ❌ Don't bake CLI conventions (1-based, inclusive end) into lib types. The lib speaks
  0-based half-open Python convention; CLI translation happens at the boundary via
  `translate_to_lib(CliRange)`.
- ❌ Don't write a custom range resolver in a new lib command — use `resolve_range`.
- ✅ Use `dispatch()` for straight CLI→lib translations in `execute_impl`.
- ✅ Use `CommandResult::ok()` and `CommandResult::err()` factory methods.
- ✅ Timing is measured in `Session::execute()`, never in `execute_impl()`.
- ✅ Pass `case_sensitive` explicitly through `resolve_columns` and `prepare_filter`.
- ✅ New filter commands inherit from `FilterArgs` and use `prepare_filter`.
- ✅ `RenderOptions` carries per-call rendering preferences — do not put them in `RenderContext`.
- ✅ Use the single-argument `render_error(msg)` / `render_warning(msg)` when no context
  is available (REPL exception handlers, startup errors). The two-argument form is for
  callers that have an explicit stream (typically `ctx.serr`).

This structure scales: new commands stay within layer boundaries, preserving reproducibility
and testability.
