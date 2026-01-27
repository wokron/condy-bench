import subprocess
from matplotlib import pyplot as plt
from pathlib import Path

benchmark_dir = Path("./build/benchmarks/")

post_condy = benchmark_dir / "post_condy"
post_asio = benchmark_dir / "post_asio"

fig_dir = Path("./results/figures/")
fig_dir.mkdir(parents=True, exist_ok=True)


def run_post(program, num):
    args = [
        program,
        "-n",
        str(num),
    ]
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


def process_output(output: str):
    result = {}
    lines = output.strip().split("\n")
    for line in lines:
        split = line.split(":", maxsplit=1)
        key = split[0].strip()
        value = split[1].strip()
        result[key] = value
    return result


def main():
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

    # Plotting results
    fig, ax = plt.subplots()
    ax.plot(num_messages, condy_results, marker="o", label="Condy")
    ax.plot(num_messages, asio_results, marker="o", label="Asio")
    ax.set_xlabel("Number of Messages")
    ax.set_ylabel("Time (ms)")
    ax.set_title("Post Benchmark: Number of Messages vs Time")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    ax.grid()
    plt.savefig(fig_dir / "post_number_of_messages.png")
    plt.close(fig)


if __name__ == "__main__":
    main()
