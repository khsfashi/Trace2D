# Sanitizer verification

Trace2D uses a narrow **Linux Clang ASan + UBSan headless CI lane** to catch memory-safety defects and undefined behavior that ordinary unit tests and static analysis may miss.

This lane is verification infrastructure only. It does **not** declare Linux a maintained Trace2D platform and does not supersede the later #78 non-MSVC/Linux toolchain work.

## Scope

The workflow is `.github/workflows/sanitizers.yml`.

It configures a Debug build with:

```text
TRACE2D_ENABLE_ASAN_UBSAN=ON
TRACE2D_WARNINGS_AS_ERRORS=ON
TRACE2D_INSTALL_SDK=OFF
BUILD_TESTING=ON
```

The CMake option deliberately accepts only Linux + Clang and applies:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
-fno-sanitize-recover=all
```

The CI runtime also makes sanitizer findings fatal and enables UBSan stack traces.

Only the test suites carrying the `sanitizer-headless` CTest label are executed:

- `trace2d_runtime_tests`
- `trace2d_scene_tests`
- `trace2d_assets_tests`

This keeps the memory/UB lane focused on deterministic CPU-side engine state and avoids turning it into a duplicate GPU/platform acceptance lane.

## Local reproduction

With Clang, CMake, and the pinned vcpkg checkout available on Linux:

```bash
export CC=clang
export CXX=clang++

cmake -S . -B build/sanitizers \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DTRACE2D_WARNINGS_AS_ERRORS=ON \
  -DTRACE2D_INSTALL_SDK=OFF \
  -DTRACE2D_ENABLE_ASAN_UBSAN=ON \
  -DBUILD_TESTING=ON

cmake --build build/sanitizers --parallel --target \
  trace2d_runtime_tests \
  trace2d_scene_tests \
  trace2d_assets_tests

ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/sanitizers -L sanitizer-headless --output-on-failure
```

## Failure policy

A sanitizer report is a correctness failure for the covered code path. Do not suppress a report merely to make CI green. If a finding is proven to originate in third-party code or an intentional low-level contract, document the evidence and keep any suppression as narrow as possible.

The sanitizer lane complements, rather than replaces:

- Windows MSVC build/tests,
- CodeQL,
- deterministic subsystem/conformance tests,
- real-GPU smoke/conformance gates.

## Primary references

- Clang AddressSanitizer documentation: https://clang.llvm.org/docs/AddressSanitizer.html
- Clang UndefinedBehaviorSanitizer documentation: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html

Decision: **ADOPT** Clang's standard compile/link sanitizer instrumentation and frame-pointer guidance; **ADAPT** it into a bounded headless Trace2D CI lane; **DEFER** ThreadSanitizer until Trace2D has a demonstrated multithreaded ownership/race-verification need and a stable compatible test surface.
