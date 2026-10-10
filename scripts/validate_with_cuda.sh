#!/bin/sh
set -eu
analyzer=${1:-build-default/ptxscope}
if ! command -v nvcc >/dev/null 2>&1; then
    echo "SKIP: nvcc is unavailable; CUDA PTX generation was not run."
    if command -v ptxas >/dev/null 2>&1; then ptxas --version; else echo "ptxas: unavailable"; fi
    exit 0
fi
nvcc --version
if command -v ptxas >/dev/null 2>&1; then ptxas --version; else echo "ptxas: unavailable"; fi
if [ ! -x "$analyzer" ]; then
    echo "error: PTXScope executable not found: $analyzer" >&2
    exit 2
fi
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/ptxscope-cuda.XXXXXX")
trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM
cat > "$temp_dir/sample.cu" <<'EOF'
extern "C" __global__ void add_one(int *out, const int *in) {
    const int i = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    out[i] = in[i] + 1;
}
EOF
nvcc --ptx "$temp_dir/sample.cu" -o "$temp_dir/sample.ptx"
echo "Running PTXScope on compiler-generated PTX (subset diagnostics are expected)."
"$analyzer" analyze "$temp_dir/sample.ptx" --verbose
