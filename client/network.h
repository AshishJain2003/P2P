#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

int connectToTracker(const char *ip, int port);
int sendToTracker(int sockfd, const char *msg);
int recvFromTracker(int sockfd, char *buffer, int size);

#endif
