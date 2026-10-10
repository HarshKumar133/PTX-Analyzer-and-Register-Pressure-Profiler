# PTXScope benchmark run

- Date: 2026-10-10 (Asia/Kolkata development context)
- Host: macOS 27.0.1, arm64
- Compiler: Homebrew Clang 21.1.4
- CMake: 4.1.2
- Build: CMake `default` preset, Release
- Command: `python3 benchmarks/benchmark.py build-default/ptxscope --repetitions 3`
- Corpus: six checked-in examples (including one malformed negative case) and generated valid 1,000/10,000 instruction straight-line cases
- Process time is the median of 3 subprocess runs. Internal phase times are from the final repetition. All values are milliseconds.

| Input | Bytes | Instructions | Blocks | Parse | CFG | Liveness | Analysis | Process median | Parse valid | Complete effects |
|---|---:|---:|---:|---:|---:|---:|---:|---:|:---:|:---:|
| conditional_branch.ptx | 225 | 6 | 4 | 0.033 | 0.009 | 0.006 | 0.029 | 3.723 | yes | yes |
| loop_kernel.ptx | 205 | 5 | 3 | 0.033 | 0.005 | 0.009 | 0.030 | 4.036 | yes | yes |
| malformed_input.ptx | 71 | 0 | 0 | 0.014 | 0.003 | 0.001 | 0.008 | 3.569 | no | no |
| multiple_functions.ptx | 143 | 4 | 2 | 0.021 | 0.005 | 0.001 | 0.021 | 3.660 | yes | yes |
| predicated_instructions.ptx | 163 | 3 | 1 | 0.029 | 0.005 | 0.006 | 0.030 | 3.809 | yes | yes |
| vector_add.ptx | 690 | 15 | 3 | 0.067 | 0.007 | 0.020 | 0.050 | 4.071 | yes | yes |
| synthetic_medium_1000.ptx | 25,097 | 1,002 | 1 | 1.893 | 0.015 | 0.515 | 0.758 | 7.497 | yes | yes |
| synthetic_large_10000.ptx | 250,097 | 10,002 | 1 | 18.817 | 0.078 | 5.156 | 7.380 | 40.415 | yes | yes |

This is a local tool-throughput sample, not a GPU performance measurement or a cross-platform baseline. The generated inputs are straight-line stress fixtures. No peak-memory measurement was collected.
