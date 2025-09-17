#include "commands.h"
#include "network.h"
#include <iostream>
using namespace std;

void processUserInput(int sockfd, const string &input) {
    sendToTracker(sockfd, input.c_str());

    char buffer[1024];
    int n = recvFromTracker(sockfd, buffer, sizeof(buffer));
    if (n > 0) {
        cout << "Reply from tracker: " << buffer << endl;
    }
}
