import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir, data_dir
import pandas as pd


post_condy = benchmark_dir / "post_condy"
post_asio = benchmark_dir / "post_asio"


def run_post(program, num):
    args = [
        str(program),
        "-n",
        str(num),
    ]
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


def draw_nm_plot(df_nm):
    fig, ax = plt.subplots()
    ax.plot(
        df_nm["switch_times"],
        df_nm["condy_time_ms"],
        marker="o",
        label="Condy",
    )
    ax.plot(
        df_nm["switch_times"],
        df_nm["asio_time_ms"],
        marker="o",
        label="Asio",
    )
    ax.set_title("Post Benchmark - Varying Switch Times")
    ax.set_xlabel("Switch Times")
    ax.set_ylabel("Time (ms)")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    ax.grid()
    fig.savefig(fig_dir / "post_switch_times.png")
    plt.close(fig)


def run():
    num_messages = [524288, 1048576, 2097152, 4194304, 8388608]

    condy_results = []
    asio_results = []

    for nm in num_messages:
        output = run_post(post_condy, nm)
        output = process_output(output)
        condy_results.append(float(output["time_ms"]))

        output = run_post(post_asio, nm)
        output = process_output(output)
        asio_results.append(float(output["time_ms"]))

    df_nm = pd.DataFrame(
        {
            "switch_times": num_messages,
            "condy_time_ms": condy_results,
            "asio_time_ms": asio_results,
        }
    )
    df_nm.to_csv(data_dir / "post_switch_times.csv", index=False)

    draw_nm_plot(df_nm)


if __name__ == "__main__":
    run()
