#include "network.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <vector>

using namespace std;

int trackerSocket;
int clientSockets[5] = {0};
fd_set fr, fw, fe;
int nMaxFd;
int peer = -1;

int initServer(int port)
{
    trackerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (trackerSocket < 0)
    {
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
    if (bind(trackerSocket, (sockaddr *)&srv, sizeof(srv)) < 0)
    {
        cerr << "Bind failed on port " << port << endl;
        return -1;
    }
    if (listen(trackerSocket, 5) < 0)
    {
        cerr << "Listen failed\n";
        return -1;
    }
    nMaxFd = trackerSocket;
    cout << "Tracker listening on port " << port << endl;
    return 0;
}

int acceptNewClient()
{
    socklen_t len = sizeof(sockaddr);
    int newSock = accept(trackerSocket, NULL, &len);
    if (newSock > 0)
    {
        for (int i = 0; i < 5; i++)
        {
            if (clientSockets[i] == 0)
            {
                clientSockets[i] = newSock;
                if (newSock > nMaxFd)
                    nMaxFd = newSock;
                cout << "Client connected: fd = " << newSock << endl;
                return newSock;
            }
        }
        cerr << "Tracker full, rejecting client\n";
        close(newSock);
    }
    return -1;
}

int connectToPeerTracker(const char *ip, int port)
{
    peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peer < 0)
    {
        cerr << "Failed to open socket for peer\n";
        return -1;
    }
    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = inet_addr(ip);
    memset(&(srv.sin_zero), 0, 8);
    if (connect(peer, (sockaddr *)&srv, sizeof(srv)) < 0)
    {
        cerr << "Failed to connect to peer" << endl;
        close(peer);
        peer = -1;
        return -1;
    }
    cout << "Connected to peer tracker at " << ip << ":" << port << endl;
    if (peer > nMaxFd)
    {
        nMaxFd = peer;
    }
    send_msg(peer, "SYNC_INIT");
    return 0;
}

bool send_msg(int fd, const string &msg)
{
    if (fd < 0)
        return false;
    uint32_t len = htonl(msg.length());
    if (send(fd, &len, sizeof(len), 0) < 0)
        return false;
    if (send(fd, msg.c_str(), msg.length(), 0) < 0)
        return false;
    return true;
}

bool recv_msg(int fd, string &msg)
{
    if (fd < 0)
        return false;
    uint32_t len;
    int n = recv(fd, &len, sizeof(len), 0);
    if (n <= 0)
        return false;
    len = ntohl(len);
    vector<char> buffer(len);
    size_t total_received = 0;
    while (total_received < len)
    {
        n = recv(fd, buffer.data() + total_received, len - total_received, 0);
        if (n <= 0)
            return false;
        total_received += n;
    }
    msg.assign(buffer.data(), len);
    return true;
}