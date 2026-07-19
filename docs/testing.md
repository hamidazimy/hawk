# Testing

Hawk's automated tests live in `tests/` and run on the core engine
(`libhawk`) — plus one self-contained CLI helper unit (`hawk-cli`'s
`helpers/utils`) — via the [doctest](https://github.com/doctest/doctest)
framework. doctest is vendored as a single header at
`third_party/doctest/doctest.h` (no submodule or network fetch required).

Tests are **opt-in**: the default build does not compile or run them. Enable
them with the `HAWK_BUILD_TESTS` CMake option (default `OFF`).

## Build with tests enabled

```bash
cmake -S . -B build -DHAWK_BUILD_TESTS=ON
cmake --build build -j
```

This produces the test runner at `build/bin/hawk-tests` alongside the normal
`hawk` binary.

## Run the tests

```bash
ctest --test-dir build --output-on-failure
```

You can also run the executable directly for doctest's native output and
filtering flags — `--test-case` takes comma-separated wildcards and is the
fastest way to iterate on one area:

```bash
./build/bin/hawk-tests
./build/bin/hawk-tests --test-case='*filter*,*Filter*'
```

## Verifying CLI-side changes

The suite covers `libhawk` plus one self-contained CLI helper TU — it does
**not** exercise the CLI's parsers, argument handling, or renderers. Changes
there are verified behaviourally by piping commands into the real binary:

```bash
printf 'filter category == auth\ncount\nexit\n' | \
    ./build/bin/hawk tests/fixtures/basic.csv --no-confirm
```

Note that in a piped (non-TTY) context the detected terminal width falls
back to 80 columns, color is disabled, and the inference spinner suppresses
itself. For renderer changes where byte-exact output matters (e.g. UTF-8
handling), redirect to a file and inspect with `hexdump`/`xxd` rather than
eyeballing a terminal.

## Fixtures

Session integration tests read checked-in CSVs from `tests/fixtures/`
(absolute path injected via the `HAWK_TEST_FIXTURE_DIR` compile
definition). Notable entries:

- `basic.csv` — the primary fixture: 16 data rows, columns
  `timestamp, id, category, count, value` (indices 0–4). `category` holds
  7×`auth`, 5×`net`, 4×`sys`; `count` has deliberately tied frequencies
  (used by the distinct tie-break tests) — recount before relying on exact
  numbers in new assertions.
- `empty.csv` — **header-only** (8 bytes), not zero-byte.
- `zero_byte.csv` — the genuinely 0-byte fixture (empty-file error tests).

Test helpers live in `tests/support/session_fixture.hpp`:
`make_session(fixture, case_sensitive)`, `payload_as<T>(result)`,
`column_index`, `view_column`, and the projection helpers
(`projection_columns`, `projection_is_identity`).

## Status

The suite currently covers:

- utility helpers (`hawk/utils/utils.hpp`)
- CLI string helpers (`hawk-cli/src/helpers/utils.hpp` — currently the two
  UTF-8 truncation primitives; compiled into the suite as a standalone TU)
- datetime parsing (`hawk/utils/datetime_parser.hpp`)
- `Schema`
- `Projection`
- range resolution (`resolve_range`)
- filter (`FilterPredicate`, `RowSearchPredicate`, `prepare_filter`, `resolve_columns`)
- `View`
- type inference (`TypeInferrer`)
- format inference (`FormatInferer` and its detectors)
- Session integration — command execution basics (config, columns, count,
  records), filter, sort (with stability verification), distinct, count, raw
  records access, filter composition (`filter+`/`filter-`), slice, projection
  (`select`/`select+`/`select-`), schema mutation (`set type`/`set name`),
  reset in all its forms, and composed command sequences, all driven through
  `Session::execute` against real CSV fixtures

Session integration coverage is complete (phases 7a and 7b).
