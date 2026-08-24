# Sanitizer verification

Trace2D uses a narrow **Linux Clang ASan + UBSan headless CI lane** to catch memory-safety defects and undefined behavior that ordinary unit tests and static analysis may miss.

This lane is verification infrastructure within the maintained Ubuntu/Clang toolchain contract documented in `docs/PORTABILITY.md`. It does not replace the ordinary Linux build/SDK consumer gate, static analysis, or real-GPU validation.

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

The CI runtime also makes sanitizer findings fatal, enables leak detection, and enables UBSan stack traces.

The workflow builds the profile CLI plus the sanitizer-covered test executables, then executes only CTest cases carrying the `sanitizer-headless` label. The current covered test executables are:

- `trace2d_profile_tests`
- `trace2d_runtime_tests`
- `trace2d_scene_tests`
- `trace2d_physics_tests`
- `trace2d_assets_tests`
- `trace2d_audio_tests`

`trace2d_profile_cli` is also built in the sanitizer configuration so its public/report composition closure remains compiler/link checked; it is not itself a CTest suite.

This keeps the memory/UB lane focused on deterministic/headless CPU-side state and avoids turning it into a duplicate GPU/platform acceptance lane.

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
  trace2d_profile_cli \
  trace2d_profile_tests \
  trace2d_runtime_tests \
  trace2d_scene_tests \
  trace2d_physics_tests \
  trace2d_assets_tests \
  trace2d_audio_tests

ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/sanitizers -L sanitizer-headless --output-on-failure
```

## Failure policy

A sanitizer report is a correctness failure for the covered code path. Do not suppress a report merely to make CI green. If a finding is proven to originate in third-party code or an intentional low-level contract, document the evidence and keep any suppression as narrow as possible.

Sanitizer success means the covered executions produced no ASan/UBSan finding in that run. It is not proof that all memory or undefined-behavior defects are absent.

The sanitizer lane complements, rather than replaces:

- Windows MSVC and Ubuntu/Clang ordinary build/tests,
- the installed-SDK external consumer gates,
- bounded Clang-Tidy correctness/performance checks,
- CodeQL security analysis,
- deterministic subsystem/conformance tests,
- real-GPU smoke/conformance gates.

## Primary references

- Clang AddressSanitizer documentation: https://clang.llvm.org/docs/AddressSanitizer.html
- Clang UndefinedBehaviorSanitizer documentation: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
- Trace2D portability contract: `docs/PORTABILITY.md`

Decision: **ADOPT** Clang's standard compile/link sanitizer instrumentation and frame-pointer guidance; **ADAPT** it into a bounded headless Trace2D CI lane; **DEFER** ThreadSanitizer until Trace2D has a demonstrated multithreaded ownership/race-verification need and a stable compatible test surface.
