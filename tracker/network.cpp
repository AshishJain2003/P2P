#include "network.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

int trackerSocket;
int clientSockets[5] = {0};
fd_set fr, fw, fe;
int nMaxFd;
int peer = -1;

int initServer(int port) {
    trackerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (trackerSocket < 0) {
        cerr << "Failed to open socket\n";
        return -1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = INADDR_ANY;
    memset(&(srv.sin_zero), 0, 8);

    int opt = 1;
    setsockopt(trackerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(trackerSocket, (sockaddr*)&srv, sizeof(srv)) < 0) {
        cerr << "Bind failed\n";
        return -1;
    }

    if (listen(trackerSocket, 5) < 0) {
        cerr << "Listen failed\n";
        return -1;
    }

    nMaxFd = trackerSocket;
    cout << "Tracker listening on port " << port << endl;
    return 0;
}

int acceptNewClient() {
    socklen_t len = sizeof(sockaddr);
    int newSock = accept(trackerSocket, NULL, &len);
    if (newSock > 0) {
        for (int i = 0; i < 5; i++) {
            if (clientSockets[i] == 0) {
                clientSockets[i] = newSock;

                // // directly use send() instead of wrapper
                // const char *msg = "Connected to tracker";
                // send(newSock, msg, strlen(msg), 0);

                if (newSock > nMaxFd) nMaxFd = newSock;
                cout << "Client connected: fd = " << newSock << endl;
                return newSock;
            }
        }
        cerr << "Tracker full, rejecting client\n";
        close(newSock);
    }
    return -1;
}

int recvFromClient(int clientFd, char *buffer, int size) {
    int n = recv(clientFd, buffer, size-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        return n;
    }
    return n;
}

int connectToPeerTracker(const char *ip, int port) {
    peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peer < 0) {
        cerr << "Failed to open socket\n";
        return -1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = inet_addr(ip);
    memset(&(srv.sin_zero), 0, 8);

    if(connect(peer, (sockaddr*)&srv, sizeof(srv)) < 0) {
        cerr << "Failed to connect" << endl;
        close(peer);
        peer = -1;
        return -1;
    }

    cout << "Connected to peer tracker at " << ip << ":" << port << endl;
    if(peer > nMaxFd) {
        nMaxFd = peer;
    }

    return 0;
}

int sendToPeerTracker(char *msg) {
    if(peer >= 0) {
        int bytesReturned = send(peer, msg, strlen(msg), 0);
        return bytesReturned;
    }else {
        cerr << "Error Occured!!" << endl;
        return -1;
    }
}

int recvFromPeerTracker(char *buffer, int size) {
    if (peer < 0) return -1;   

    int bytesReturned = recv(peer, buffer, size - 1, 0);
    if (bytesReturned > 0) {
        buffer[bytesReturned] = '\0';
    }
    return bytesReturned;
}

