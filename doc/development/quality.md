# Development Quality

This document defines formatting, diagnostics, testing, and verification rules.

## Verification baseline

At minimum, run the test preset matching the build being changed:

```sh
cmake --build --preset debug-gui --target gui_tests gui_model_tests gui -j2
cmake --build --preset debug-uomul --target uomul_tests image_codec_tests -j2
ctest --test-dir build/debug-gui --output-on-failure
ctest --test-dir build/debug-uomul --output-on-failure
```

For headless or release changes, use the corresponding `release-headless` or
`release-gui` build and CTest presets. Package changes must also pass the
`package_consumer` test registered by CMake.

Documentation changes must pass a relative-link check and `git diff --check`.

## Formatting

Format all C++ source and header files with
[clang-format](https://clang.llvm.org/docs/ClangFormat.html) using the project
configuration in [`.clang-format`](../../.clang-format). 

## Compiler diagnostics

Project targets compile with warnings as errors. Fix diagnostics in
the code instead of weakening the target warning policy.

## Clang-tidy

The project uses the [`.clang-tidy`](../../.clang-tidy) configuration.

The CMake configuration excludes the `logger` target from clang-tidy because its
distribution `spdlog` and `fmt` headers are outside the project lint scope.

## Test placement

Place tests under the directory that owns the code under test.

Test files use the `<subject>_tests.cpp` suffix. Keep helper implementations in
the same test area and use a descriptive helper name when they are not test
cases.

Register production source files in the owning CMake target. Test directories
are discovered by the test CMake configuration, but new tests still belong in
the correct target area.

## Test case naming

Catch2 test cases describe observable behavior. Use this form:

```text
<subject>_Should_<ExpectedBehavior>[_If_<Condition>]
```

Use a lower-camel-case subject for a function or component and a PascalCase
subject for a public type when the type is the contract under test. Include a
method name between the subject and `Should` when it clarifies the behavior.
Use `_If_` for an important precondition or scenario.

Examples:

```cpp
TEST_CASE("imageCodec_Should_RoundTripRgbPixelsThroughPng")
TEST_CASE("TexMapsReader_open_Should_RejectMissingIndexFile")
TEST_CASE("mapEditing_Should_RejectMaskedEditsWithoutSelection")
TEST_CASE("RadarColReader_open_Should_RejectMissingFile_If_PathDoesNotExist")
```

Use one behavior per test case. Name the result and relevant condition rather
than the implementation steps. Keep Catch2 tags separate from the test name
and use lowercase component or feature tags when filtering is useful.

## Magic numbers and suppressions

Prefer named constants in production code and tests. Intentional test fixture
values may use scoped suppressions for both configured magic-number checks:

```cpp
REQUIRE(locks.acquire(7U, identity)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
```

Use `NOLINTNEXTLINE` for one following statement or `NOLINTBEGIN` and
`NOLINTEND` around a small related block. Keep the suppression next to the
intentional values. Do not use file-wide or broad suppressions to hide unrelated
diagnostics.
