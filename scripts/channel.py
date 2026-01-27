import subprocess
from matplotlib import pyplot as plt
from pathlib import Path

benchmark_dir = Path("./build/benchmarks/")

channel_condy = benchmark_dir / "channel_condy"
channel_asio = benchmark_dir / "channel_asio"

fig_dir = Path("./results/figures/")
fig_dir.mkdir(parents=True, exist_ok=True)


def run_channel(program, buffer_size, num_messages, task_pair):
    args = [
        program,
        "-b",
        str(buffer_size),
        "-n",
        str(num_messages),
        "-p",
        str(task_pair),
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
    default_buffer_size = 1024
    default_num_messages = 1048576
    default_task_pair = 1

    num_messages = [131072, 262144, 524288, 1048576, 2097152]
    task_pairs = [1, 2, 4, 8, 16, 32]

    condy_nm_results = []
    condy_tp_results = []

    for nm in num_messages:
        output = run_channel(channel_condy, default_buffer_size, nm, default_task_pair)
        output = process_output(output)
        condy_nm_results.append(float(output["time_ms"]))
    for tp in task_pairs:
        output = run_channel(
            channel_condy, default_buffer_size, default_num_messages, tp
        )
        output = process_output(output)
        condy_tp_results.append(float(output["time_ms"]))

    asio_nm_results = []
    asio_tp_results = []

    for nm in num_messages:
        output = run_channel(channel_asio, default_buffer_size, nm, default_task_pair)
        output = process_output(output)
        asio_nm_results.append(float(output["time_ms"]))

    for tp in task_pairs:
        output = run_channel(
            channel_asio, default_buffer_size, default_num_messages, tp
        )
        output = process_output(output)
        asio_tp_results.append(float(output["time_ms"]))

    num_messages = list(map(str, num_messages))
    task_pairs = list(map(str, task_pairs))

    # num_message plot
    fig1 = plt.figure(figsize=(15, 5))
    plt.plot(num_messages, condy_nm_results, marker="o", label="condy")
    plt.plot(num_messages, asio_nm_results, marker="o", label="asio")
    plt.title("Channel Benchmark - Varying Number of Messages")
    plt.xlabel("Number of Messages")
    plt.ylabel("Time (ms)")
    plt.yscale("log")
    plt.legend()
    plt.grid()
    plt.savefig(fig_dir / "channel_number_of_messages.png")
    plt.close(fig1)

    # task_pairs plot
    fig2 = plt.figure(figsize=(15, 5))
    plt.plot(task_pairs, condy_tp_results, marker="o", label="condy")
    plt.plot(task_pairs, asio_tp_results, marker="o", label="asio")
    plt.title("Channel Benchmark - Varying Number of Task Pairs")
    plt.xlabel("Number of Task Pairs")
    plt.ylabel("Time (ms)")
    plt.yscale("log")
    plt.legend()
    plt.grid()
    plt.savefig(fig_dir / "channel_task_pairs.png")
    plt.close(fig2)


if __name__ == "__main__":
    main()
