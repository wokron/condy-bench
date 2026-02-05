use clap::Parser;
use compio::runtime::{Runtime, spawn};
use std::time::Instant;

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct Args {
    /// Number of tasks to spawn
    #[arg(short = 'n', long, default_value_t = 1_000_000)]
    num_tasks: usize,
}

async fn task_func() {}

async fn spawner(num_tasks: usize) {
    let mut handles = Vec::with_capacity(num_tasks);
    for _ in 0..num_tasks {
        // compio::runtime::Runtime::spawn 返回 JoinHandle
        handles.push(spawn(task_func()));
    }
    // 等待所有任务完成
    for handle in handles {
        let _ = handle.await;
    }
}

fn main() {
    let args = Args::parse();

    let runtime = Runtime::new().unwrap();

    let start = Instant::now();

    runtime.block_on(spawner(args.num_tasks));

    let duration = start.elapsed().as_millis();
    println!("time_ms:{}", duration);
}
