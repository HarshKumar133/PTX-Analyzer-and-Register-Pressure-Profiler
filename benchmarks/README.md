# Analyzer benchmark

Run `python3 benchmarks/benchmark.py build-default/ptxscope --repetitions 5`. The harness reports the six checked-in example inputs and generates two valid straight-line inputs in a temporary directory (1,000 and 10,000 arithmetic instructions). It measures fixture size, function/instruction/block counts, lex+parse time, per-function CFG and liveness time, total analysis time, median process time, parse status, and whether modeled effects are complete. Phase timings are enabled only by `--timings` and are not included in ordinary stable JSON reports.

The synthetic generator uses one `.entry` function, two `.b32` registers, a single defining `mov`, repeated valid `add.u32` instructions, and `ret`. This stresses front-end and liveness throughput but is not representative of compiler-generated control-flow diversity. Peak memory is omitted because this portable harness does not measure child-process high-water memory reliably across platforms.

One local run is recorded in [results/2026-10-10-macos-clang21-release.md](results/2026-10-10-macos-clang21-release.md), including host/compiler/build details. It is a single machine's analyzer-throughput sample, not a cross-platform baseline or GPU-performance claim.
