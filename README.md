# Condy Benchmarks

This project is designed to benchmark and compare [Condy](https://github.com/wokron/condy) with other common I/O frameworks (such as Asio, epoll, etc.) in various scenarios.

## Usage

Clone the repository:

```sh
git clone --recurse-submodules https://github.com/wokron/condy-bench.git
cd condy-bench
```

Install `libaio`:

```sh
# For Debian/Ubuntu:
sudo apt-get install libaio-dev
```

Install Rust toolchain:

```sh
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

Build the benchmark programs:

```sh
cmake -B build -S . -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j$(nproc)
```

Install Python dependencies:

```sh
pip install -r requirements.txt
```

Run all benchmarks:

```sh
python3 ./scripts/all.py
```

Test results will appear in the `./results/` directory, including raw data (in `csv` format) and plots generated from the results.

You can also run a specific benchmark, for example, the channel benchmark:

```sh
python3 ./scripts/channel.py
```

Currently available benchmarks:

- `./scripts/channel.py`
- `./scripts/echo_server.py`
- `./scripts/file_random_read.py`
- `./scripts/file_read.py`
- `./scripts/post.py`
- `./scripts/spawn.py`