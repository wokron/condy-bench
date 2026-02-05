import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, benchmark_rust_dir, fig_dir, data_dir
import pandas as pd


post_condy = benchmark_dir / "post_condy"
post_asio = benchmark_dir / "post_asio"
post_compio = benchmark_rust_dir / "post_compio"
post_monoio = benchmark_rust_dir / "post_monoio"


def run_post(program, num):
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
        str(num),
    ]
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


def draw_nm_plot(df_nm):
    import numpy as np

    markers = ["o", "s", "d"]
    labels = ["Condy", "Asio", "Monoio"]
    columns = ["condy_time_ms", "asio_time_ms", "monoio_time_ms"]

    x = np.arange(len(df_nm))
    for i, col in enumerate(columns):
        plt.plot(
            x,
            df_nm[col],
            marker=markers[i],
            linestyle="-",
            label=labels[i],
            markersize=8,
            markerfacecolor="none",
            markeredgewidth=2,
        )

    plt.xlabel("Switch Times")
    plt.ylabel("Time (ms)")
    plt.xticks(x, df_nm["switch_times"])
    plt.yscale("log")
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(
        fig_dir / "post_switch_times.png",
        dpi=200,
        bbox_inches="tight",
    )
    plt.close()


def run():
    num_messages = [524288, 1048576, 2097152, 4194304, 8388608]

    condy_results = []
    asio_results = []
    # compio_time = []
    monoio_time = []

    for nm in num_messages:
        output = run_post(post_condy, nm)
        output = process_output(output)
        condy_results.append(float(output["time_ms"]))

        output = run_post(post_asio, nm)
        output = process_output(output)
        asio_results.append(float(output["time_ms"]))

        # output = run_post(post_compio, nm)
        # output = process_output(output)
        # compio_time.append(float(output["time_ms"]))

        output = run_post(post_monoio, nm)
        output = process_output(output)
        monoio_time.append(float(output["time_ms"]))

    df_nm = pd.DataFrame(
        {
            "switch_times": num_messages,
            "condy_time_ms": condy_results,
            "asio_time_ms": asio_results,
            # "compio_time_ms": compio_time,
            "monoio_time_ms": monoio_time,
        }
    )
    df_nm.to_csv(data_dir / "post_switch_times.csv", index=False)

    draw_nm_plot(df_nm)


if __name__ == "__main__":
    run()
