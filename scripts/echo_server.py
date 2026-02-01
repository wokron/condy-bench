import subprocess
import time
from matplotlib import pyplot as plt
from pathlib import Path
import os
from utils import process_output, benchmark_dir, fig_dir, data_dir
import pandas as pd

echo_server_condy = benchmark_dir / "echo_server_condy"
echo_server_asio = benchmark_dir / "echo_server_asio"
echo_server_epoll = benchmark_dir / "echo_server_epoll"

echo_stress = benchmark_dir / "echo_stress"

next_port = 12345


def run_echo_server(program, message_size, num_connections, duration, fixed_fd=False):
    global next_port
    port = next_port
    next_port += 1

    args_server = [
        "sudo",
        "nice",
        "-n",
        "-20",
        "taskset",
        "-c",
        "0",
        str(program),
        "0.0.0.0",
        str(port),
    ]
    if fixed_fd:
        args_server.append("-f")
    print(args_server)
    proc = subprocess.Popen(args_server)
    time.sleep(0.5)  # Give the server time to start, this may fail but is simpler
    try:
        cpu_count = os.cpu_count()
        cpus = ",".join(str(i) for i in range(1, cpu_count))
        args_stress = [
            "taskset",
            "-c",
            cpus,
            str(echo_stress),
            "-a",
            "127.0.0.1",
            "-p",
            str(port),
            "-l",
            str(message_size),
            "-c",
            str(num_connections),
            "-t",
            str(duration),
        ]
        print(args_stress)
        result = subprocess.run(args_stress, capture_output=True, text=True)
        if result.returncode != 0:
            print("Error running echo_stress:")
            print(result.stderr)
            raise RuntimeError("echo_stress failed")
        return result.stdout
    finally:
        proc.terminate()
        proc.wait()


def draw_conn_plot(df_conn):
    import numpy as np

    markers = ["o", "s", "^", "D"]
    labels = ["Condy", "Condy Fixed Fd", "Asio", "Epoll"]
    columns = ["condy_mbps", "condy_fixed_fd_mbps", "asio_mbps", "epoll_mbps"]

    x = np.arange(len(df_conn))
    for i, col in enumerate(columns):
        plt.plot(
            x,
            df_conn[col],
            marker=markers[i],
            linestyle="-",
            label=labels[i],
            markersize=8,
            markerfacecolor="none",
            markeredgewidth=2,
        )

    plt.xlabel("Number of Connections")
    plt.ylabel("Throughput (MB/s)")
    plt.xticks(x, df_conn["num_connections"])
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(
        fig_dir / "echo_server_num_connections.png",
        dpi=200,
        bbox_inches="tight",
    )
    plt.close()


def run():
    default_message_size = 1024  # 1 KB
    default_duration = 10  # seconds

    num_connections = [4, 8, 16, 32, 64]

    condy_conn_results = []
    for conn in num_connections:
        output = run_echo_server(
            echo_server_condy,
            message_size=default_message_size,
            num_connections=conn,
            duration=default_duration,
        )
        output = process_output(output)
        condy_conn_results.append(float(output["resp_bytes_per_sec"]))

    condy_fixed_conn_results = []
    for conn in num_connections:
        output = run_echo_server(
            echo_server_condy,
            message_size=default_message_size,
            num_connections=conn,
            duration=default_duration,
            fixed_fd=True,
        )
        output = process_output(output)
        condy_fixed_conn_results.append(float(output["resp_bytes_per_sec"]))

    asio_conn_results = []
    for conn in num_connections:
        output = run_echo_server(
            echo_server_asio,
            message_size=default_message_size,
            num_connections=conn,
            duration=default_duration,
        )
        output = process_output(output)
        asio_conn_results.append(float(output["resp_bytes_per_sec"]))

    epoll_conn_results = []
    for conn in num_connections:
        output = run_echo_server(
            echo_server_epoll,
            message_size=default_message_size,
            num_connections=conn,
            duration=default_duration,
        )
        output = process_output(output)
        epoll_conn_results.append(float(output["resp_bytes_per_sec"]))

    def bps_to_mbps(bps):
        return bps / (1024 * 1024)

    condy_conn_results = list(map(bps_to_mbps, condy_conn_results))
    condy_fixed_conn_results = list(map(bps_to_mbps, condy_fixed_conn_results))
    asio_conn_results = list(map(bps_to_mbps, asio_conn_results))
    epoll_conn_results = list(map(bps_to_mbps, epoll_conn_results))

    df_conn = pd.DataFrame(
        {
            "num_connections": num_connections,
            "condy_mbps": condy_conn_results,
            "condy_fixed_fd_mbps": condy_fixed_conn_results,
            "asio_mbps": asio_conn_results,
            "epoll_mbps": epoll_conn_results,
        }
    )
    df_conn.to_csv(data_dir / "echo_server_num_connections.csv", index=False)
    draw_conn_plot(df_conn)


if __name__ == "__main__":
    run()
