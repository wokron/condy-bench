import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir, data_dir
import pandas as pd

spawn_condy = benchmark_dir / "spawn_condy"
spawn_asio = benchmark_dir / "spawn_asio"


def run_spawn(program, num_tasks):
    args = [
        "sudo",
        "nice",
        "-n",
        "-20",
        "taskset",
        "-c",
        "0",
        str(program),
        "-n",
        str(num_tasks),
    ]
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


def draw_nt_plot(df_nt):
    import numpy as np

    markers = ["o", "s"]
    labels = ["Condy", "Asio"]
    columns = ["condy_time_ms", "asio_time_ms"]

    x = np.arange(len(df_nt))
    for i, col in enumerate(columns):
        plt.plot(
            x,
            df_nt[col],
            marker=markers[i],
            linestyle="-",
            label=labels[i],
            markersize=8,
            markerfacecolor="none",
            markeredgewidth=2,
        )

    plt.xlabel("Number of Tasks")
    plt.ylabel("Time (ms)")
    plt.xticks(x, df_nt["num_tasks"])
    plt.yscale("log")
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(
        fig_dir / "spawn_number_of_tasks.png",
        dpi=200,
        bbox_inches="tight",
    )
    plt.close()


def run():
    num_tasks = [131072, 262144, 524288, 1048576, 2097152, 4194304]

    condy_results = []
    asio_results = []

    for nt in num_tasks:
        output = run_spawn(spawn_condy, nt)
        output = process_output(output)
        condy_results.append(float(output["time_ms"]))

        output = run_spawn(spawn_asio, nt)
        output = process_output(output)
        asio_results.append(float(output["time_ms"]))

    df_nt = pd.DataFrame(
        {
            "num_tasks": num_tasks,
            "condy_time_ms": condy_results,
            "asio_time_ms": asio_results,
        }
    )
    df_nt.to_csv(data_dir / "spawn_number_of_tasks.csv", index=False)

    draw_nt_plot(df_nt)


if __name__ == "__main__":
    run()
