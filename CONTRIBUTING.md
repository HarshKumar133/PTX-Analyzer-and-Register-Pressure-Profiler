# Contributing

Use C++20 and the existing CMake/CTest structure. Keep parser syntax and semantic effect rules separate. For a new PTX construct, verify the relevant syntax/semantics against NVIDIA's official PTX ISA, document the subset change, and add a small valid case plus a nearby negative/boundary case. Unknown behavior must remain visibly incomplete.

Run `cmake --preset default`, `cmake --build --preset default`, and `ctest --preset default`. Keep reports deterministic and avoid machine-dependent absolute paths in expected output. Do not claim PTXAS or GPU validation unless those tools were actually run.
