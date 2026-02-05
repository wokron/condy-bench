use clap::Parser;
use monoio::fs::File;
use monoio::spawn;
use std::os::unix::fs::OpenOptionsExt;
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
    offset: Rc<AtomicUsize>,
    file_size: usize,
) {
    loop {
        let current_offset = offset.fetch_add(block_size, Ordering::Relaxed);
        if current_offset >= file_size {
            break;
        }
        let to_read = std::cmp::min(block_size, file_size - current_offset);
        let buffer = Vec::with_capacity(to_read);
        let _ = file.read_at(buffer, current_offset as u64).await;
    }
}

#[monoio::main]
async fn main() {
    let args = Args::parse();

    let mut open_opts = monoio::fs::OpenOptions::new();
    open_opts.read(true);
    if args.direct_io {
        open_opts.custom_flags(libc::O_DIRECT);
    }
    let file = open_opts
        .open(&args.filename)
        .await
        .expect("open file failed");

    let file_size = file.metadata().await.unwrap().len() as usize;

    let file = Rc::new(file);
    let offset = Rc::new(AtomicUsize::new(0));

    let start = Instant::now();

    let mut handles = Vec::with_capacity(args.num_tasks);
    for i in 0..args.num_tasks {
        let file = file.clone();
        let offset = offset.clone();
        let block_size = args.block_size;
        let file_size = file_size;
        handles.push(spawn(do_reads(i, file, block_size, offset, file_size)));
    }

    for handle in handles {
        handle.await;
    }

    let duration = start.elapsed().as_millis();
    let throughput = file_size as f64 / (start.elapsed().as_secs_f64()) / (1024.0 * 1024.0);
    println!("time_ms:{}", duration);
    println!("throughput_mbps:{:.2}", throughput);
}
