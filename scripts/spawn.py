import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir


spawn_condy = benchmark_dir / "spawn_condy"
spawn_asio = benchmark_dir / "spawn_asio"


def run_spawn(program, num_tasks):
    args = [
        program,
        "-n",
        str(num_tasks),
    ]
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


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

    # Plotting results
    fig, ax = plt.subplots()
    ax.plot(num_tasks, condy_results, marker="o", label="Condy")
    ax.plot(num_tasks, asio_results, marker="o", label="Asio")
    ax.set_xlabel("Number of Tasks")
    ax.set_ylabel("Time (ms)")
    ax.set_title("Spawn Benchmark: Number of Tasks vs Time")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    ax.grid()
    fig_path = fig_dir / "spawn_benchmark.png"
    plt.savefig(fig_path)
    plt.close(fig)


if __name__ == "__main__":
    run()
