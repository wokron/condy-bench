import subprocess
from matplotlib import pyplot as plt
from pathlib import Path

benchmark_dir = Path("./build/benchmarks/")

file_read_condy = benchmark_dir / "file_read_condy"
file_read_asio = benchmark_dir / "file_read_asio"
file_read_sync = benchmark_dir / "file_read_sync"

fig_dir = Path("./results/figures/")
fig_dir.mkdir(parents=True, exist_ok=True)


def run_file_read(program, file, block_size, num_tasks, direct_io=False):
    # We need to clean vm cache between runs to get accurate results
    result = subprocess.run(["sudo", "sh", "-c", "echo 3 > /proc/sys/vm/drop_caches"])
    if result.returncode != 0:
        print("Warning: Failed to drop caches. Results may be inaccurate.")

    args = [
        program,
        file,
        "-b",
        str(block_size),
    ]
    if num_tasks is not None:
        args += ["-t", str(num_tasks)]
    if direct_io:
        args.append("-d")
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


def generate_test_file(file_path: Path, size_in_mb: int):
    with open(file_path, "wb") as f:
        f.write(b"\0" * size_in_mb * 1024 * 1024)


def main():
    test_file = Path("./test_file.bin")
    if not test_file.exists():
        generate_test_file(test_file, size_in_mb=8 * 1024)  # 8 GB test file

    default_block_size = 1024 * 1024  # 1 MB
    default_num_tasks = 16

    block_sizes = [
        64 * 1024,
        256 * 1024,
        1 * 1024 * 1024,
        4 * 1024 * 1024,
    ]  # 64 KB to 4 MB
    num_tasks_list = [1, 2, 4, 8, 16, 32, 64]

    condy_bs_results = []
    condy_nt_results = []

    for bs in block_sizes:
        output = run_file_read(file_read_condy, str(test_file), bs, default_num_tasks)
        output = process_output(output)
        condy_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_read(file_read_condy, str(test_file), default_block_size, nt)
        output = process_output(output)
        condy_nt_results.append(float(output["throughput_mbps"]))

    condy_direct_bs_results = []
    condy_direct_nt_results = []

    for bs in block_sizes:
        output = run_file_read(
            file_read_condy, str(test_file), bs, default_num_tasks, direct_io=True
        )
        output = process_output(output)
        condy_direct_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_read(
            file_read_condy, str(test_file), default_block_size, nt, direct_io=True
        )
        output = process_output(output)
        condy_direct_nt_results.append(float(output["throughput_mbps"]))

    asio_bs_results = []
    asio_nt_results = []

    for bs in block_sizes:
        output = run_file_read(file_read_asio, str(test_file), bs, default_num_tasks)
        output = process_output(output)
        asio_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_read(file_read_asio, str(test_file), default_block_size, nt)
        output = process_output(output)
        asio_nt_results.append(float(output["throughput_mbps"]))

    sync_bs_results = []

    for bs in block_sizes:
        output = run_file_read(file_read_sync, str(test_file), bs, None)
        output = process_output(output)
        sync_bs_results.append(float(output["throughput_mbps"]))

    output = run_file_read(file_read_sync, str(test_file), default_block_size, None)
    output = process_output(output)
    sync_nt_results = [float(output["throughput_mbps"])] * len(num_tasks_list)

    sync_direct_bs_results = []

    for bs in block_sizes:
        output = run_file_read(file_read_sync, str(test_file), bs, None, direct_io=True)
        output = process_output(output)
        sync_direct_bs_results.append(float(output["throughput_mbps"]))

    output = run_file_read(
        file_read_sync, str(test_file), default_block_size, None, direct_io=True
    )
    output = process_output(output)
    sync_direct_nt_results = [float(output["throughput_mbps"])] * len(num_tasks_list)

    # block_size plots
    fig, ax = plt.subplots()
    ax.plot(block_sizes, condy_bs_results, marker="o", label="Condy")
    ax.plot(block_sizes, condy_direct_bs_results, marker="o", label="Condy Direct I/O")
    ax.plot(block_sizes, asio_bs_results, marker="o", label="Asio")
    ax.plot(block_sizes, sync_bs_results, marker="o", label="Sync")
    ax.plot(block_sizes, sync_direct_bs_results, marker="o", label="Sync Direct I/O")
    ax.set_xlabel("Block Size (bytes)")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("File Read Benchmark: Block Size vs Throughput")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    plt.savefig(fig_dir / "file_read_block_size.png")
    plt.close(fig)

    # num_tasks plots
    fig, ax = plt.subplots()
    ax.plot(num_tasks_list, condy_nt_results, marker="o", label="Condy")
    ax.plot(
        num_tasks_list, condy_direct_nt_results, marker="o", label="Condy Direct I/O"
    )
    ax.plot(num_tasks_list, asio_nt_results, marker="o", label="Asio")
    ax.plot(num_tasks_list, sync_nt_results, marker="o", label="Sync")
    ax.plot(num_tasks_list, sync_direct_nt_results, marker="o", label="Sync Direct I/O")
    ax.set_xlabel("Number of Tasks")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("File Read Benchmark: Number of Tasks vs Throughput")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    plt.savefig(fig_dir / "file_read_num_tasks.png")
    plt.close(fig)


if __name__ == "__main__":
    main()
