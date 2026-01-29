import channel
import echo_server
import file_random_read
import file_read
import post
import spawn

if __name__ == "__main__":
    channel.run()
    echo_server.run()
    file_random_read.run()
    file_read.run()
    post.run()
    spawn.run()
