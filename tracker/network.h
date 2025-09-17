#ifndef TRACKER_NETWORK_H
#define TRACKER_NETWORK_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern int trackerSocket;
extern int clientSockets[5];
extern fd_set fr, fw, fe;
extern int nMaxFd;
extern int peer;

int initServer(int port);
int acceptNewClient();
int recvFromClient(int clientFd, char *buffer, int size);
int sendToClient(int clientFd, char *msg);
int connectToPeerTracker(const char *ip, int port);
int sendToPeerTracker(char *msg);
int recvFromPeerTracker(char *buffer, int size);

#endif
