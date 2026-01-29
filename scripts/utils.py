from pathlib import Path


def process_output(output: str):
    result = {}
    lines = output.strip().split("\n")
    for line in lines:
        split = line.split(":", maxsplit=1)
        key = split[0].strip()
        value = split[1].strip()
        result[key] = value
    return result


benchmark_dir = Path("./build/benchmarks/")

results_dir = Path("./results/")

fig_dir = Path("./results/figures/")
fig_dir.mkdir(parents=True, exist_ok=True)
