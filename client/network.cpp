#include "network.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
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
        cerr << "Error connecting to tracker\n";
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int sendToTracker(int sockfd, const char *msg) {
    return send(sockfd, msg, strlen(msg), 0);
}

int recvFromTracker(int sockfd, char *buffer, int size) {
    int n = recv(sockfd, buffer, size-1, 0);
    if (n > 0) buffer[n] = '\0';
    return n;
}
