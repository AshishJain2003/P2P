#ifndef TRACKER_NETWORK_H
#define TRACKER_NETWORK_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
using namespace std;

extern int trackerSocket;
extern int clientSockets[5];
extern fd_set fr, fw, fe;
extern int nMaxFd;
extern int peer;

int initServer(int port);
int acceptNewClient();
int connectToPeerTracker(const char *ip, int port);

bool send_msg(int fd, const string& msg);
bool recv_msg(int fd, string& msg);

#endif