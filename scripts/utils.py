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

data_dir = Path("./results/data/")
data_dir.mkdir(parents=True, exist_ok=True)


class CSVSaver:
    def __init__(self, x_name: str, x_values: list, y_dict: dict[str, list]):
        self.x_name = x_name
        self.x_values = x_values
        self.y_dict = y_dict

    def save(self, path: Path):
        with open(path, "w") as f:
            # Write header
            header = [self.x_name] + list(self.y_dict.keys())
            f.write(",".join(header) + "\n")

            # Write data rows
            num_rows = len(self.x_values)
            for i in range(num_rows):
                row = [str(self.x_values[i])]
                for y_values in self.y_dict.values():
                    row.append(str(y_values[i]))
                f.write(",".join(row) + "\n")
