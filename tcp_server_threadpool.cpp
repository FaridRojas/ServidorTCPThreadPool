#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t workers) : stop(false) {
        for (size_t i = 0; i < workers; ++i) {
            threads.emplace_back([this]() {
                for (;;) {
                    int client_fd;
                    {
                        std::unique_lock<std::mutex> lock(this->mtx);
                        this->cv.wait(lock, [this]() { return this->stop || !this->jobs.empty(); });
                        if (this->stop && this->jobs.empty()) return;
                        client_fd = this->jobs.front();
                        this->jobs.pop();
                    }
                    handle_client(client_fd);
                }
            });
        }
    }

    void enqueue(int client_fd) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            jobs.push(client_fd);
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto &t : threads) t.join();
    }

private:
    static void handle_client(int client_fd) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            std::string msg(buffer);
            if (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                msg.pop_back();

            int count = 0;
            if (!msg.empty()) {
                char last = msg.back();
                for (char c : msg) if (c == last) count++;
            }
            std::string resp = std::to_string(count) + "\n";
            send(client_fd, resp.c_str(), resp.size(), 0);
        }
        close(client_fd);
    }

    std::vector<std::thread> threads;
    std::queue<int> jobs;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

int main(int argc, char* argv[]) {
    const int PORT = 5050;
    size_t THREADS = 4; // default

    if (argc >= 2) {
        THREADS = std::stoul(argv[1]);
        if (THREADS == 0) {
            std::cerr << "Thread count must be > 0\n";
            return 1;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 50) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Thread pool server listening on port " << PORT
              << " with " << THREADS << " workers...\n";

    ThreadPool pool(THREADS);

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        pool.enqueue(client_fd);
    }

    close(server_fd);
    return 0;
}