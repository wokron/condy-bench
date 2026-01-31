import subprocess
from matplotlib import pyplot as plt
from pathlib import Path
from utils import process_output, benchmark_dir, fig_dir, data_dir, CSVSaver


file_random_read_condy = benchmark_dir / "file_random_read_condy"
file_random_read_sync = benchmark_dir / "file_random_read_sync"
file_random_read_aio = benchmark_dir / "file_random_read_aio"
file_random_read_uring = benchmark_dir / "file_random_read_uring"


def run_file_random_read(
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
    if iopoll:
        args.append("-p")
    if sqpoll:
        args.append("-q")
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

    default_block_size = 4 * 1024  # 4 KB
    num_tasks_list = [4, 8, 16, 32, 64, 128]

    condy_nt_results = []
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=default_block_size,
            num_tasks=nt,
        )
        output = process_output(output)
        condy_nt_results.append(float(output["iops"]))

    condy_direct_nt_results = []
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_condy,
            test_file,
            block_size=default_block_size,
            num_tasks=nt,
            direct_io=True,
        )
        output = process_output(output)
        condy_direct_nt_results.append(float(output["iops"]))

    condy_fixed_nt_results = []
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
        condy_fixed_nt_results.append(float(output["iops"]))

    uring_nt_results = []
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_uring, str(test_file), default_block_size, nt
        )
        output = process_output(output)
        uring_nt_results.append(float(output["iops"]))

    aio_nt_results = []
    for nt in num_tasks_list:
        output = run_file_random_read(
            file_random_read_aio, str(test_file), default_block_size, nt
        )
        output = process_output(output)
        aio_nt_results.append(float(output["iops"]))

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
    ax.plot(num_tasks_list, uring_nt_results, marker="o", label="Uring Direct I/O")
    ax.plot(num_tasks_list, aio_nt_results, marker="o", label="Aio")
    ax.set_xlabel("Queue Depth")
    ax.set_ylabel("IOPS")
    ax.set_title("Random Read Benchmark: Queue Depth vs IOPS")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.legend()
    plt.grid()
    fig.savefig(fig_dir / "file_random_read_queue_depth.png")
    plt.close(fig)

    csv_saver_nt = CSVSaver(
        x_name="queue_depth",
        x_values=num_tasks_list,
        y_dict={
            "condy_iops": condy_nt_results,
            "condy_direct_io_iops": condy_direct_nt_results,
            "condy_fixed_iops": condy_fixed_nt_results,
            "uring_iops": uring_nt_results,
            "aio_iops": aio_nt_results,
        },
    )
    csv_saver_nt.save(data_dir / "file_random_read_queue_depth.csv")


if __name__ == "__main__":
    run()
