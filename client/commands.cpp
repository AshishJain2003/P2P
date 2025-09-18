#include "commands.h"
#include "network.h"
#include <iostream>
using namespace std;

bool processUserInput(int sockfd, const std::string &input) {
    if (!send_msg(sockfd, input)) {
        cerr << "\nError: Connection to tracker lost." << endl;
        return false;
    }
    string reply;
    if (!recv_msg(sockfd, reply)) {
        cerr << "\nError: Connection to tracker lost." << endl;
        return false;
    }
    cout << reply << endl;
    return true;
}