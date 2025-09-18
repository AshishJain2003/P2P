#include "network.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>

using namespace std;

int connectToTracker(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Error creating socket\n";
        return -1;
    }
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    if (connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

bool send_msg(int sockfd, const string& msg) {
    if (sockfd < 0) return false;
    uint32_t len = htonl(msg.length());
    if (send(sockfd, &len, sizeof(len), 0) < 0) {
        return false;
    }
    if (send(sockfd, msg.c_str(), msg.length(), 0) < 0) {
        return false;
    }
    return true;
}

bool recv_msg(int sockfd, string& msg) {
    if (sockfd < 0) return false;
    uint32_t len;
    int n = recv(sockfd, &len, sizeof(len), 0);
    if (n <= 0) {
        if (n == 0) cout << "\nTracker disconnected." << endl;
        return false;
    }
    len = ntohl(len);
    vector<char> buffer(len);
    size_t total_received = 0;
    while (total_received < len) {
        n = recv(sockfd, buffer.data() + total_received, len - total_received, 0);
        if (n <= 0) return false;
        total_received += n;
    }
    msg.assign(buffer.data(), len);
    return true;
}