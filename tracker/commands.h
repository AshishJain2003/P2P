#ifndef TRACKER_COMMANDS_H
#define TRACKER_COMMANDS_H

#include <string>
#include <vector>
#include "main.h" 

using namespace std;

void handleCommand(int clientFd, string cmd, const TrackerInfo& peer_info);

#endif