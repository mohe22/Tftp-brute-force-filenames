#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace std;

const uint8_t TFTP_RRQ[] = { 0x00, 0x01 };

void worker(queue<string> &q, mutex &q_mtx, const string &ip, int port, mutex &cout_mtx) {
    string fileName;
    char buffer[516];

    while (true) {
        {
            lock_guard<mutex> lock(q_mtx);
            if (q.empty()) break;
            fileName = q.front();
            q.pop();
        }

        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            {
                lock_guard<mutex> lock(cout_mtx);
                cerr << "socket() failed for " << fileName << "\n";
            }
            continue;
        }

        struct timeval tv{2, 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server.sin_addr);

        string rrq;
        rrq.append(reinterpret_cast<const char*>(TFTP_RRQ), 2);
        rrq += fileName + '\0' + "octet" + '\0';

        socklen_t addr_len = sizeof(server);
        if (sendto(sock, rrq.c_str(), rrq.size(), 0,
                   (struct sockaddr*)&server, addr_len) < 0) {
            close(sock);
            continue;
        }

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&server, &addr_len);

        string status = "Not found";
        string hex_dump;

        if (n > 0) {
            uint16_t opcode = ntohs(*(uint16_t*)buffer);
            if (opcode == 3) status = "Exist";        // DATA
            else if (opcode == 5) status = "Not found"; // ERROR
        } else {
            hex_dump = "<timeout / no reply>";
        }

        close(sock);

        {
            lock_guard<mutex> lock(cout_mtx);

            if (status == "Exist")
                cout << "\033[31m"; // red

            cout << "File: " << fileName << ", " << status << "\033[0m" << endl;
        }

    }
}

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5) {
        cerr << "Usage: " << argv[0] << " <ip> <port> <wordlist> [threads]\n";
        cerr << "Example: " << argv[0] << " 10.10.11.87 69 tftp.txt 10\n";
        return 1;
    }

    string ip = argv[1];
    int port = stoi(argv[2]);
    string wordlist = argv[3];
    int threads = (argc == 5) ? stoi(argv[4]) : 10;

    if (threads <= 0) threads = 1;
    if (port <= 0 || port > 65535) {
        cerr << "Invalid port: " << port << "\n";
        return 1;
    }

    ifstream file(wordlist);
    if (!file.is_open()) {
        cerr << "Error: Cannot open wordlist '" << wordlist << "'\n";
        return 1;
    }

    queue<string> tasks;
    string line;
    while (getline(file, line)) {
        if (!line.empty()) {
            tasks.push(line);
        }
    }
    file.close();

    if (tasks.empty()) {
        cout << "No files to test.\n";
        return 0;
    }

    cout << "Starting TFTP enumeration on " << ip << ":" << port
         << " (" << tasks.size() << " files, " << threads << " threads)\n\n";

    mutex q_mtx, cout_mtx;
    vector<thread> pool;

    for (int i = 0; i < threads; ++i) {
        pool.emplace_back(worker, ref(tasks), ref(q_mtx), ip, port, ref(cout_mtx));
    }

    for (auto &t : pool) {
        t.join();
    }

    cout << "All done.\n";
    return 0;
}
