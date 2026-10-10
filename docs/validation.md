# Validation

The deterministic C++ unit executable covers lexing, comments/locations, register declaration arrays, multi-function parsing, loop CFG/liveness, branch diagnostics, conservative pattern checks, unknown effects, and JSON fields. Python CLI checks exercise help/version, text and JSON output, JSON decoding, and missing input exit status.

Run `ctest --test-dir build-default --output-on-failure`. An UndefinedBehaviorSanitizer configuration is available through `cmake --preset debug-sanitizers`, `cmake --build --preset debug-sanitizers`, and `ctest --preset debug-sanitizers`. AddressSanitizer binaries built with the local Homebrew Clang 21 runtime did not start to completion, including the version-only CLI path, so the preset uses UBSan and no ASan success is claimed.

Optional NVIDIA validation is isolated in `scripts/validate_with_cuda.sh`. It checks `nvcc` and `ptxas`, compiles a small CUDA source to PTX only when possible, and then runs PTXScope. It does not execute GPU code. If utilities are absent the script exits successfully with an explicit skip. The current development host has neither `nvcc` nor `ptxas`; no NVIDIA-toolchain compatibility result is claimed.
