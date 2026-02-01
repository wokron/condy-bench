import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir, data_dir, CSVSaver


file_read_condy = benchmark_dir / "file_read_condy"
file_read_uring = benchmark_dir / "file_read_uring"
file_read_aio = benchmark_dir / "file_read_aio"


def run_file_read(
    program,
    file,
    block_size,
    num_tasks,
    direct_io=False,
    fixed=False,
    iopoll=False,
    sqpoll=False,
):
    # We need to clean vm cache between runs to get accurate results
    result = subprocess.run(
        ["sudo", "sh", "-c", "echo 3 > /proc/sys/vm/drop_caches"], check=True
    )

    args = [
        "sudo",
        "nice",
        "-n",
        "-20",
        "taskset",
        "-c",
        "0,2",
        str(program),
        str(file),
        "-b",
        str(block_size),
    ]
    if num_tasks is not None:
        args += ["-t", str(num_tasks)]
    if direct_io:
        args.append("-d")
    if fixed:
        args.append("-f")
    if iopoll:
        args.append("-p")
    if sqpoll:
        args.append("-q")
    print(args)
    result = subprocess.run(args, capture_output=True, text=True, check=True)
    return result.stdout


def generate_test_file(file_path: Path, size_in_mb: int):
    subprocess.run(
        ["dd", "if=/dev/zero", f"of={file_path}", "bs=1M", f"count={size_in_mb}"],
        check=True,
    )


def run():
    test_file = Path("./test_file.bin")
    if not test_file.exists():
        generate_test_file(test_file, size_in_mb=8 * 1024)  # 8 GB test file

    default_block_size = 64 * 1024  # 64 KB

    num_tasks_list = [4, 8, 16, 32, 64, 128]

    condy_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(file_read_condy, str(test_file), default_block_size, nt)
        output = process_output(output)
        condy_nt_results.append(float(output["throughput_mbps"]))

    condy_fixed_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(
            file_read_condy,
            str(test_file),
            default_block_size,
            nt,
            direct_io=False,
            fixed=True,
        )
        output = process_output(output)
        condy_fixed_nt_results.append(float(output["throughput_mbps"]))

    condy_fixed_direct_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(
            file_read_condy,
            str(test_file),
            default_block_size,
            nt,
            fixed=True,
            direct_io=True,
        )
        output = process_output(output)
        condy_fixed_direct_nt_results.append(float(output["throughput_mbps"]))

    condy_fixed_direct_iopoll_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(
            file_read_condy,
            str(test_file),
            default_block_size,
            nt,
            fixed=True,
            direct_io=True,
            iopoll=True,
        )
        output = process_output(output)
        condy_fixed_direct_iopoll_nt_results.append(float(output["throughput_mbps"]))

    uring_all_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(
            file_read_uring,
            str(test_file),
            default_block_size,
            nt,
            fixed=True,
            direct_io=True,
            iopoll=True,
        )
        output = process_output(output)
        uring_all_nt_results.append(float(output["throughput_mbps"]))

    aio_nt_results = []
    for nt in num_tasks_list:
        output = run_file_read(file_read_aio, str(test_file), default_block_size, nt)
        output = process_output(output)
        aio_nt_results.append(float(output["throughput_mbps"]))

    # num_tasks plots
    fig, ax = plt.subplots()
    ax.plot(num_tasks_list, condy_nt_results, marker="o", label="Condy")
    ax.plot(num_tasks_list, condy_fixed_nt_results, marker="o", label="Condy(Fixed)")
    ax.plot(
        num_tasks_list,
        condy_fixed_direct_nt_results,
        marker="o",
        label="Condy(Fixed+Direct)",
    )
    ax.plot(
        num_tasks_list,
        condy_fixed_direct_iopoll_nt_results,
        marker="o",
        label="Condy(Fixed+Direct+IOPoll)",
    )
    ax.plot(
        num_tasks_list,
        uring_all_nt_results,
        marker="o",
        label="Uring(Fixed+Direct+IOPoll)",
    )
    ax.plot(num_tasks_list, aio_nt_results, marker="o", label="Aio")
    ax.set_xlabel("Queue Depth")
    ax.set_ylabel("Throughput (MB/s)")
    ax.set_title("File Read Benchmark: Queue Depth vs Throughput")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    plt.savefig(fig_dir / "file_read_queue_depth.png")
    plt.close(fig)

    csv_saver_nt = CSVSaver(
        x_name="queue_depth",
        x_values=num_tasks_list,
        y_dict={
            "condy_throughput_mbps": condy_nt_results,
            "condy_fixed_throughput_mbps": condy_fixed_nt_results,
            "condy_fixed_direct_throughput_mbps": condy_fixed_direct_nt_results,
            "condy_fixed_direct_iopoll_throughput_mbps": condy_fixed_direct_iopoll_nt_results,
            "uring_all_throughput_mbps": uring_all_nt_results,
            "aio_throughput_mbps": aio_nt_results,
        },
    )
    csv_saver_nt.save(data_dir / "file_read_queue_depth.csv")


if __name__ == "__main__":
    run()
