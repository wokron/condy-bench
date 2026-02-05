from pathlib import Path
import subprocess


def process_output(output: str):
    result = {}
    lines = output.strip().split("\n")
    for line in lines:
        split = line.split(":", maxsplit=1)
        key = split[0].strip()
        value = split[1].strip()
        result[key] = value
    return result


def generate_test_file(file_path: Path, size_in_mb: int):
    subprocess.run(
        ["dd", "if=/dev/zero", f"of={file_path}", "bs=1M", f"count={size_in_mb}"],
        check=True,
    )


benchmark_dir = Path("./build/benchmarks/")
benchmark_rust_dir = Path("./build/benchmarks_rust/release/")

results_dir = Path("./results/")

fig_dir = Path("./results/figures/")
fig_dir.mkdir(parents=True, exist_ok=True)

data_dir = Path("./results/data/")
data_dir.mkdir(parents=True, exist_ok=True)
