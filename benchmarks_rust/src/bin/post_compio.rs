use clap::Parser;
use compio::runtime::Runtime;
use futures_lite::future::yield_now;
use std::time::Instant;

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct Args {
    /// Number of operations to perform
    #[arg(short = 'n', long, default_value_t = 50_000_000)]
    num: usize,
}

async fn test(num: usize) {
    for _ in 0..num {
        yield_now().await;
    }
}

fn main() {
    let args = Args::parse();

    let runtime = Runtime::new().unwrap();

    let start = Instant::now();

    runtime.block_on(test(args.num));

    let duration = start.elapsed().as_millis();
    println!("time_ms:{}", duration);
}
