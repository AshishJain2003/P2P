#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H
using namespace std;

#include <string>

int connectToTracker(const char *ip, int port);
bool send_msg(int sockfd, const string& msg);
bool recv_msg(int sockfd, string& msg);

#endif