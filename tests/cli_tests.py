import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

binary, fixture = sys.argv[1:]

def run(*args):
    return subprocess.run([binary, *args], text=True, capture_output=True, check=False)

assert run("help").returncode == 0
assert "PTXScope" in run("version").stdout
text_result = run("analyze", fixture)
assert text_result.returncode == 0, text_result.stderr + text_result.stdout
assert "Function vector_add" in text_result.stdout
json_result = run("analyze", fixture, "--format", "json")
assert json_result.returncode == 0, json_result.stderr
decoded = json.loads(json_result.stdout)
assert decoded["schema_version"] == 1
assert decoded["functions"][0]["name"] == "vector_add"
assert json_result.stdout == run("analyze", fixture, "--format", "json").stdout
timed = run("analyze", fixture, "--format", "json", "--timings")
assert json.loads(timed.stdout)["timings_ms"]["parse"] >= 0
bad = run("analyze", fixture + ".does-not-exist")
assert bad.returncode == 3
malformed = run("analyze", str(pathlib.Path(fixture).with_name("malformed_input.ptx")))
assert malformed.returncode == 3
assert run("analyze", fixture, "--format", "yaml").returncode == 2
with tempfile.TemporaryDirectory(prefix="ptxscope-cli-") as temp_dir:
    temp = pathlib.Path(temp_dir)
    odd_name = temp / 'input"quoted.ptx'
    shutil.copyfile(fixture, odd_name)
    escaped = run("analyze", str(odd_name), "--format", "json")
    assert json.loads(escaped.stdout)["input"] == str(odd_name)
    report_path = temp / "report.json"
    assert run("analyze", fixture, "--format", "json", "--output", str(report_path)).returncode == 0
    assert json.loads(report_path.read_text(encoding="utf-8"))["schema_version"] == 1
    assert run("analyze", fixture, "--output", str(temp / "missing" / "out.txt")).returncode == 5
    branch = temp / "bad_branch.ptx"
    branch.write_text(".entry f() { bra MISSING; ret; }\n", encoding="utf-8")
    assert run("analyze", str(branch)).returncode == 4
print("All PTXScope CLI checks passed.")
