# Trace2D portability contract

Last reviewed: 2026-08-24.

This document records what Trace2D continuously proves across its two current C++ toolchain environments and, equally importantly, what it does not claim.

## Continuously validated toolchains

Trace2D currently validates these source/build environments in CI:

| Environment | Compiler / generator | Proven scope |
| --- | --- | --- |
| Windows x64 | MSVC v143 / Visual Studio generator | source configure/build/test, clean-clone quick start, SDK install/package/external consumer, product proofs that require Windows runtime support |
| Ubuntu 24.04 x86_64 | Clang 18 / Ninja | source configure/build/headless tests, profiler environment report, SDK install, clean installed-SDK external consumer build/test |

The Linux lane pins `ubuntu-24.04` and Clang 18 instead of following a moving `ubuntu-latest` compiler contract. Dependency resolution remains the repository `vcpkg.json` plus the pinned vcpkg builtin baseline; no second Linux dependency manager owns project dependencies.

## Build and warning policy

Both maintained compiler lanes build Trace2D-owned code with `TRACE2D_WARNINGS_AS_ERRORS=ON`.

Portability findings are repaired in the owning code or represented by an explicit capability boundary. Do not globally suppress a compiler diagnostic merely to make the second toolchain green.

The dedicated `ci-linux-clang-tidy` verification preset adds build-time-only Clang-Tidy 18 analysis. Its policy is stored in the repository root `.clang-tidy` file and intentionally contains only a bounded correctness/performance set:

- `clang-analyzer-core.*`
- `clang-analyzer-cplusplus.NewDelete`
- `clang-analyzer-cplusplus.NewDeleteLeaks`
- `bugprone-string-constructor`
- `bugprone-string-integer-assignment`
- `bugprone-suspicious-memset-usage`
- `bugprone-use-after-move`
- `performance-implicit-conversion-in-loop`
- `performance-move-constructor-init`

These diagnostics are errors in the static-analysis lane. Broad readability, naming, style, and modernize families are deliberately not enabled. CodeQL remains the broader security-oriented static-analysis lane; Clang-Tidy is not a replacement for CodeQL.

Static analysis is CI/build-time verification only. It adds no production runtime branch, allocation, dependency, CPU work, or binary requirement.

## Sanitizer boundary

Linux Clang ASan + UBSan is a separate targeted headless verification lane. See `docs/SANITIZERS.md`.

A sanitizer success means the covered execution paths completed without a reported AddressSanitizer/UndefinedBehaviorSanitizer failure under that build. It does not prove absence of all memory/UB defects and it does not make hosted CI timing a performance budget.

## Determinism across toolchains

Trace2D deterministic contracts are semantic contracts: stable ordering, explicit fixed-step authority, bounded retained state, stable identifiers, transactional failure rules, canonical serialized forms where specified, and caller-supplied seeds/timestamps where those are part of the owning subsystem.

Cross-toolchain support does **not** mean that arbitrary floating-point computation is bit-identical across MSVC, Clang, different CPUs, drivers, or build modes. Unless a subsystem contract explicitly states a cross-toolchain bitwise representation guarantee, do not use raw floating-point bit equality between different environments as the portability oracle.

A reproducible evidence record should retain the source revision and relevant environment identity. The profiler report therefore records OS, architecture, compiler, and build configuration. Timing evidence is contextual measurement, not deterministic state.

## GPU and window/runtime capability boundary

The Ubuntu hosted lane proves backend-independent renderer/material/camera/resource code compiles and that applicable headless tests pass. It intentionally does not claim real hosted-runner GPU conformance or stable GPU timing.

The profile contract reports Linux headless GPU timing as an explicit unsupported/unavailable state rather than inventing zero timing. Real GPU smoke/conformance and release-tier device evidence are owned by #92.

Similarly, successful Linux source/SDK builds do not by themselves promote every window-system, physical audio-device, display-driver, or GPU-driver combination to a supported runtime matrix. Those capabilities require their own runtime evidence.

## External consumer contract

The maintained non-MSVC baseline includes installing Trace2D to a temporary SDK prefix and configuring/building/testing `examples/e0_external_game` as a clean consumer of the installed CMake package. This prevents the Linux claim from depending only on in-tree include paths or target visibility.

The Windows External Consumer SDK lane remains the corresponding packaging/consumer authority for MSVC.

## Change rule

A new compiler/platform is not supported merely because it compiles once. Promotion requires an owned configure/build/test contract, dependency/toolchain identity, public-SDK consumer evidence where applicable, and explicit runtime capability limits.

Portability fixes must not add hot-path abstraction, allocation, synchronization, or repeated discovery work without evidence that the cost is necessary. Hosted CI wall-clock duration is operational evidence only; structural profiler metrics and dedicated workload evidence remain the performance authority.

## Primary references

- Clang-Tidy 18 documentation: https://releases.llvm.org/18.1.0/tools/clang/tools/extra/docs/clang-tidy/index.html
- Clang-Tidy 18 check list: https://releases.llvm.org/18.1.6/tools/clang/tools/extra/docs/clang-tidy/checks/list.html
- CMake Clang-Tidy target property: https://cmake.org/cmake/help/latest/prop_tgt/LANG_CLANG_TIDY.html
- Trace2D sanitizer contract: `docs/SANITIZERS.md`
