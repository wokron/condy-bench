use clap::Parser;
use compio::fs::File;
use compio::io::AsyncReadAt;
use compio::runtime::spawn;
use rand::SeedableRng;
use rand::rngs::StdRng;
use rand::seq::SliceRandom;
use std::rc::Rc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::time::Instant;

#[derive(Parser, Debug)]
#[command(author, version, about)]
struct Args {
    /// Block size of each read operation in bytes
    #[arg(short = 'b', long, default_value_t = 1024 * 1024)]
    block_size: usize,
    /// Number of concurrent tasks
    #[arg(short = 't', long, default_value_t = 32)]
    num_tasks: usize,
    /// Seed for random number generator
    #[arg(short = 's', long, default_value_t = 42)]
    seed: u64,
    /// Use direct I/O
    #[arg(short = 'd', long, default_value_t = false)]
    direct_io: bool,
    /// File name
    filename: String,
}

async fn do_reads(
    _id: usize,
    file: Rc<File>,
    block_size: usize,
    index: Rc<AtomicUsize>,
    offsets: Rc<Vec<usize>>,
) {
    loop {
        let current_index = index.fetch_add(1, Ordering::Relaxed);
        if current_index >= offsets.len() {
            break;
        }
        let current_offset = offsets[current_index];
        let buffer = Vec::with_capacity(block_size);
        let _ = file.read_at(buffer, current_offset as u64).await;
    }
}

#[compio::main]
async fn main() {
    let args = Args::parse();

    let mut open_opts = compio::fs::OpenOptions::new();
    open_opts.read(true);
    if args.direct_io {
        open_opts.custom_flags(libc::O_DIRECT);
    }
    let file = open_opts
        .open(&args.filename)
        .await
        .expect("open file failed");

    let file_size = file.metadata().await.unwrap().len() as usize;

    let num_blocks = (file_size + args.block_size - 1) / args.block_size;
    let mut offsets: Vec<usize> = (0..num_blocks).map(|i| i * args.block_size).collect();

    let mut rng = StdRng::seed_from_u64(args.seed);
    offsets.shuffle(&mut rng);

    let file = Rc::new(file);
    let offsets = Rc::new(offsets);
    let index = Rc::new(AtomicUsize::new(0));

    let start = Instant::now();

    let mut handles = Vec::with_capacity(args.num_tasks);
    for i in 0..args.num_tasks {
        let file = file.clone();
        let offsets = offsets.clone();
        let index = index.clone();
        let block_size = args.block_size;
        handles.push(spawn(do_reads(i, file, block_size, index, offsets)));
    }

    for handle in handles {
        handle.await.unwrap();
    }

    let duration = start.elapsed().as_millis();
    let iops = num_blocks as f64 / start.elapsed().as_secs_f64();
    println!("time_ms:{}", duration);
    println!("iops:{:.2}", iops);
}
