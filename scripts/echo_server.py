import subprocess
import time
from matplotlib import pyplot as plt
from pathlib import Path
import os
from utils import process_output, benchmark_dir, fig_dir


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
        "taskset",
        "-c",
        "0",
        program,
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
            echo_stress,
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

    print("condy_conn_results:", condy_conn_results)
    print("condy_fixed_conn_results:", condy_fixed_conn_results)
    print("asio_conn_results:", asio_conn_results)
    print("epoll_conn_results:", epoll_conn_results)

    # num_connections plot
    fig, ax = plt.subplots()
    ax.plot(num_connections, condy_conn_results, marker="o", label="Condy")
    ax.plot(
        num_connections, condy_fixed_conn_results, marker="o", label="Condy (fixed fd)"
    )
    ax.plot(num_connections, asio_conn_results, marker="o", label="ASIO")
    ax.plot(num_connections, epoll_conn_results, marker="o", label="Epoll")
    ax.set_xlabel("Number of Connections")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("Echo Server Throughput vs Number of Connections")
    ax.set_xscale("log", base=2)
    ax.legend()
    ax.grid(True)
    fig.savefig(fig_dir / "echo_server_num_connections.png")
    plt.close(fig)


if __name__ == "__main__":
    run()
