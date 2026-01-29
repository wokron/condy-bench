import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir, data_dir, CSVSaver


file_random_read_condy = benchmark_dir / "file_random_read_condy"
file_random_read_asio = benchmark_dir / "file_random_read_asio"
file_random_read_sync = benchmark_dir / "file_random_read_sync"
file_random_read_aio = benchmark_dir / "file_random_read_aio"


def run_file_random_read(
    program, file, block_size, num_tasks, direct_io=False, fixed=False
):
    # We need to clean vm cache between runs to get accurate results
    result = subprocess.run(["sudo", "sh", "-c", "echo 3 > /proc/sys/vm/drop_caches"])
    if result.returncode != 0:
        print("Warning: Failed to drop caches. Results may be inaccurate.")

    args = [
        "taskset",
        "-c",
        "0",
        program,
        file,
        "-b",
        str(block_size),
    ]
    if num_tasks is not None:
        args += ["-t", str(num_tasks)]
    if direct_io:
        args.append("-d")
    if fixed:
        args.append("-f")
    print(args)
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout


def generate_test_file(file_path: Path, size_in_mb: int):
    with open(file_path, "wb") as f:
        f.write(b"\0" * size_in_mb * 1024 * 1024)


def run():
    test_file = Path("./test_file.bin")
    if not test_file.exists():
        generate_test_file(test_file, size_in_mb=2 * 1024)  # 2 GB test file

    default_block_size = 32 * 1024  # 32 KB
    default_num_tasks = 32

    block_sizes = [
        4 * 1024,
        8 * 1024,
        16 * 1024,
        32 * 1024,
        64 * 1024,
    ]  # 4 KB to 64 KB
    num_tasks_list = [4, 8, 16, 32, 64]

    condy_bs_results = []
    condy_nt_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=bs,
            num_tasks=default_num_tasks,
        )
        output = process_output(output)
        condy_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=default_block_size,
            num_tasks=nt,
        )
        output = process_output(output)
        condy_nt_results.append(float(output["throughput_mbps"]))

    condy_direct_bs_results = []
    condy_direct_nt_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=bs,
            num_tasks=default_num_tasks,
            direct_io=True,
        )
        output = process_output(output)
        condy_direct_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=default_block_size,
            num_tasks=nt,
            direct_io=True,
        )
        output = process_output(output)
        condy_direct_nt_results.append(float(output["throughput_mbps"]))

    condy_fixed_bs_results = []
    condy_fixed_nt_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=bs,
            num_tasks=default_num_tasks,
            direct_io=False,
            fixed=True,
        )
        output = process_output(output)
        condy_fixed_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=default_block_size,
            num_tasks=nt,
            direct_io=False,
            fixed=True,
        )
        output = process_output(output)
        condy_fixed_nt_results.append(float(output["throughput_mbps"]))

    aio_bs_results = []
    aio_nt_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_aio, str(test_file), bs, default_num_tasks
        )
        output = process_output(output)
        aio_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_aio, str(test_file), default_block_size, nt
        )
        output = process_output(output)
        aio_nt_results.append(float(output["throughput_mbps"]))

    asio_bs_results = []
    asio_nt_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_asio, str(test_file), bs, default_num_tasks
        )
        output = process_output(output)
        asio_bs_results.append(float(output["throughput_mbps"]))
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_asio, str(test_file), default_block_size, nt
        )
        output = process_output(output)
        asio_nt_results.append(float(output["throughput_mbps"]))

    sync_bs_results = []

    for bs in block_sizes:
        output = run_file_random_read(file_random_read_sync, str(test_file), bs, None)
        output = process_output(output)
        sync_bs_results.append(float(output["throughput_mbps"]))

    output = run_file_random_read(
        file_random_read_sync, str(test_file), default_block_size, None
    )
    output = process_output(output)
    sync_nt_results = [float(output["throughput_mbps"])] * len(num_tasks_list)

    sync_direct_bs_results = []

    for bs in block_sizes:
        output = run_file_random_read(
            file_random_read_sync, str(test_file), bs, None, direct_io=True
        )
        output = process_output(output)
        sync_direct_bs_results.append(float(output["throughput_mbps"]))

    output = run_file_random_read(
        file_random_read_sync, str(test_file), default_block_size, None, direct_io=True
    )
    output = process_output(output)
    sync_direct_nt_results = [float(output["throughput_mbps"])] * len(num_tasks_list)

    # block_size plots
    fig, ax = plt.subplots()
    ax.plot(block_sizes, condy_bs_results, marker="o", label="Condy")
    ax.plot(block_sizes, condy_direct_bs_results, marker="o", label="Condy Direct I/O")
    ax.plot(
        block_sizes, condy_fixed_bs_results, marker="o", label="Condy Fixed Fd & Buffer"
    )
    ax.plot(block_sizes, aio_bs_results, marker="o", label="Aio")
    ax.plot(block_sizes, asio_bs_results, marker="o", label="Asio")
    ax.plot(block_sizes, sync_bs_results, marker="o", label="Sync")
    ax.plot(block_sizes, sync_direct_bs_results, marker="o", label="Sync Direct I/O")
    ax.set_xlabel("Block Size (bytes)")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("Random Read Benchmark: Block Size vs Throughput")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    fig.savefig(fig_dir / "file_random_read_block_size.png")
    plt.close(fig)

    # num_tasks plots
    fig, ax = plt.subplots()
    ax.plot(num_tasks_list, condy_nt_results, marker="o", label="Condy")
    ax.plot(
        num_tasks_list, condy_direct_nt_results, marker="o", label="Condy Direct I/O"
    )
    ax.plot(
        num_tasks_list,
        condy_fixed_nt_results,
        marker="o",
        label="Condy Fixed Fd & Buffer",
    )
    ax.plot(num_tasks_list, aio_nt_results, marker="o", label="Aio")
    ax.plot(num_tasks_list, asio_nt_results, marker="o", label="Asio")
    ax.plot(num_tasks_list, sync_nt_results, marker="o", label="Sync")
    ax.plot(num_tasks_list, sync_direct_nt_results, marker="o", label="Sync Direct I/O")
    ax.set_xlabel("Number of Tasks")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("Random Read Benchmark: Number of Tasks vs Throughput")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    fig.savefig(fig_dir / "file_random_read_num_tasks.png")
    plt.close(fig)

    csv_saver_bs = CSVSaver(
        x_name="block_size_bytes",
        x_values=block_sizes,
        y_dict={
            "condy_throughput_mbps": condy_bs_results,
            "condy_direct_io_throughput_mbps": condy_direct_bs_results,
            "condy_fixed_throughput_mbps": condy_fixed_bs_results,
            "aio_throughput_mbps": aio_bs_results,
            "asio_throughput_mbps": asio_bs_results,
            "sync_throughput_mbps": sync_bs_results,
            "sync_direct_io_throughput_mbps": sync_direct_bs_results,
        },
    )
    csv_saver_bs.save(data_dir / "file_random_read_block_size.csv")

    csv_saver_nt = CSVSaver(
        x_name="num_tasks",
        x_values=num_tasks_list,
        y_dict={
            "condy_throughput_mbps": condy_nt_results,
            "condy_direct_io_throughput_mbps": condy_direct_nt_results,
            "condy_fixed_throughput_mbps": condy_fixed_nt_results,
            "aio_throughput_mbps": aio_nt_results,
            "asio_throughput_mbps": asio_nt_results,
            "sync_throughput_mbps": sync_nt_results,
            "sync_direct_io_throughput_mbps": sync_direct_nt_results,
        },
    )
    csv_saver_nt.save(data_dir / "file_random_read_num_tasks.csv")


if __name__ == "__main__":
    run()
