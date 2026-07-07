# Testing

Hawk's automated tests live in `tests/` and run on the core engine (`libhawk`)
via the [doctest](https://github.com/doctest/doctest) framework. doctest is
vendored as a single header at `third_party/doctest/doctest.h` (no submodule or
network fetch required).

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
filtering flags:

```bash
./build/bin/hawk-tests
```

## Status

The suite currently covers:

- utility helpers (`hawk/utils/utils.hpp`)
- datetime parsing (`hawk/utils/datetime_parser.hpp`)
- `Schema`
- `Projection`
- range resolution (`resolve_range`)
- filter (`FilterPredicate`, `RowSearchPredicate`, `prepare_filter`, `resolve_columns`)

Coverage of remaining subsystems — type inference, format inference, and view —
is added in follow-up phases.
