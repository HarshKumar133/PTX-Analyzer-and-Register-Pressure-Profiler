#!/usr/bin/env python3
"""Measure PTXScope process and reported phase times on fixtures and generated inputs."""

import argparse
import json
import pathlib
import statistics
import subprocess
import tempfile
import time


def run_benchmarks(analyzer, fixtures, repetitions):
    print("fixture,bytes,functions,instructions,blocks,parse_ms,cfg_ms,liveness_ms,analysis_ms,"
          "process_median_ms,parse_valid,complete")
    for fixture in fixtures:
        samples = []
        wall_ms = []
        for _ in range(repetitions):
            start = time.perf_counter()
            result = subprocess.run(
                [str(analyzer), "analyze", str(fixture), "--format", "json", "--timings"],
                text=True, capture_output=True, check=False,
            )
            wall_ms.append((time.perf_counter() - start) * 1000.0)
            if result.returncode not in (0, 3, 4):
                raise SystemExit(f"analysis failed for {fixture.name}: {result.stderr}{result.stdout}")
            samples.append(json.loads(result.stdout))
        report = samples[-1]
        functions = report["functions"]
        total_instructions = sum(f["instruction_count"] for f in functions)
        total_blocks = sum(f["basic_block_count"] for f in functions)
        cfg_ms = sum(f["timings_ms"]["cfg"] for f in functions)
        live_ms = sum(f["timings_ms"]["liveness"] for f in functions)
        complete = report["parse_valid"] and all(
            f["effects_complete"] and f["liveness_converged"] for f in functions
        )
        parse_ms = report["timings_ms"]["parse"]
        analysis_ms = report["timings_ms"]["analysis"]
        print(f"{fixture.name},{fixture.stat().st_size},{len(functions)},{total_instructions},"
              f"{total_blocks},{parse_ms:.3f},{cfg_ms:.3f},{live_ms:.3f},{analysis_ms:.3f},"
              f"{statistics.median(wall_ms):.3f},{str(report['parse_valid']).lower()},"
              f"{str(complete).lower()}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("analyzer", type=pathlib.Path)
    parser.add_argument("--repetitions", type=int, default=5)
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be at least one")
    root = pathlib.Path(__file__).resolve().parents[1]
    fixtures = sorted((root / "examples").glob("*.ptx"))
    with tempfile.TemporaryDirectory(prefix="ptxscope-benchmark-") as temp_dir:
        for label, instruction_count in (("medium", 1000), ("large", 10000)):
            fixture = pathlib.Path(temp_dir) / f"synthetic_{label}_{instruction_count}.ptx"
            lines = [".version 7.0", ".target sm_70", ".entry synthetic() {",
                     "  .reg .b32 %r0, %r1;", "  mov.u32 %r0, 0;"]
            lines.extend("  add.u32 %r0, %r0, %r1;" for _ in range(instruction_count))
            lines.extend(["  ret;", "}"])
            fixture.write_text("\n".join(lines) + "\n", encoding="utf-8")
            fixtures.append(fixture)
        run_benchmarks(args.analyzer, fixtures, args.repetitions)


if __name__ == "__main__":
    main()
