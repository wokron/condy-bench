import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir

channel_condy = benchmark_dir / "channel_condy"
channel_asio = benchmark_dir / "channel_asio"


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


def run():
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
    fig, ax = plt.subplots()
    ax.plot(num_messages, condy_nm_results, marker="o", label="condy")
    ax.plot(num_messages, asio_nm_results, marker="o", label="asio")
    ax.set_title("Channel Benchmark - Varying Number of Messages")
    ax.set_xlabel("Number of Messages")
    ax.set_ylabel("Time (ms)")
    ax.set_yscale("log")
    ax.legend()
    ax.grid()
    fig.savefig(fig_dir / "channel_number_of_messages.png")
    plt.close(fig)

    # task_pairs plot
    fig, ax = plt.subplots()
    ax.plot(task_pairs, condy_tp_results, marker="o", label="condy")
    ax.plot(task_pairs, asio_tp_results, marker="o", label="asio")
    ax.set_title("Channel Benchmark - Varying Number of Task Pairs")
    ax.set_xlabel("Number of Task Pairs")
    ax.set_ylabel("Time (ms)")
    ax.set_yscale("log")
    ax.legend()
    ax.grid()
    fig.savefig(fig_dir / "channel_task_pairs.png")
    plt.close(fig)


if __name__ == "__main__":
    run()
