use clap::Parser;
use compio::runtime::Runtime;
use futures::SinkExt;
use futures::StreamExt;
use futures::channel::mpsc::{Receiver, Sender, channel};
use std::time::Instant;

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct Args {
    /// Channel buffer size
    #[arg(short = 'b', long, default_value_t = 1024)]
    buffer_size: usize,
    /// Number of messages per producer
    #[arg(short = 'n', long, default_value_t = 1_000_000)]
    num_messages: usize,
    /// Number of producer/consumer pairs
    #[arg(short = 'p', long, default_value_t = 1)]
    task_pair: usize,
}

async fn producer(mut tx: Sender<Option<i32>>, num_messages: usize) {
    for i in 0..num_messages as i32 {
        let _ = tx.send(Some(i)).await;
    }
    let _ = tx.send(None).await; // 发送结束信号
}

async fn consumer(mut rx: Receiver<Option<i32>>) {
    let mut _count = 0;
    while let Some(value) = rx.next().await {
        if value.is_none() {
            break;
        }
        _count += 1;
    }
}

async fn run_all(runtime: &Runtime, args: &Args) {
    let mut handles = Vec::with_capacity(args.task_pair * 2);
    for _ in 0..args.task_pair {
        let (tx, rx) = channel::<Option<i32>>(args.buffer_size);
        handles.push(runtime.spawn(producer(tx, args.num_messages)));
        handles.push(runtime.spawn(consumer(rx)));
    }
    for handle in handles {
        let _ = handle.await;
    }
}

fn main() {
    let args = Args::parse();

    let runtime = Runtime::new().unwrap();

    let start = Instant::now();

    runtime.block_on(run_all(&runtime, &args));

    let duration = start.elapsed().as_millis();
    println!("time_ms:{}", duration);
}
