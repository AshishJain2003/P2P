#include "network.h"
#include "commands.h"
#include <iostream>
#include <unistd.h>
using namespace std;

#define PORT 9909

int main() {
    int sockfd = connectToTracker("127.0.0.1", PORT);
    if (sockfd < 0) return 1;

    string input;
    while (true) {
        cout << "> ";
        getline(cin, input);
        if (input == "exit") break;
        processUserInput(sockfd, input);
    }

    close(sockfd);
    return 0;
}
