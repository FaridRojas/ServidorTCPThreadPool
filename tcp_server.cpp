#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

int main() {
    const int PORT = 5050;

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

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
            std::string msg(buffer);
            // remove possible trailing newline
            if (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                msg.pop_back();

            if (msg.empty()) {
                std::string resp = "0\n";
                send(client_fd, resp.c_str(), resp.size(), 0);
            } else {
                char last = msg.back();
                int count = 0;
                for (char c : msg) {
                    if (c == last) count++;
                }
                std::string resp = std::to_string(count) + "\n";
                send(client_fd, resp.c_str(), resp.size(), 0);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}