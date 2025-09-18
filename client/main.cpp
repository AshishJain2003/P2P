#include "network.h"
#include "commands.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

struct TrackerInfo
{
    string ip;
    int port;
};

TrackerInfo parseAddress(const string &addr)
{
    TrackerInfo info;
    size_t colon_pos = addr.find(':');
    info.ip = addr.substr(0, colon_pos);
    info.port = stoi(addr.substr(colon_pos + 1));
    return info;
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);

    if (argc != 3)
    {
        cerr << "Usage: ./client.out <IP>:<PORT> tracker_info.txt" << endl;
        return 1;
    }
    string tracker_file = argv[2];

    vector<TrackerInfo> trackers;
    int fd = open(tracker_file.c_str(), O_RDONLY);
    if (fd < 0)
    {
        perror("Error opening tracker_info.txt");
        return 1;
    }
    char buffer[1024];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read < 0)
    {
        perror("Error reading tracker_info.txt");
        return 1;
    }
    buffer[bytes_read] = '\0';

    stringstream ss(buffer);
    string line;
    while (getline(ss, line))
    {
        if (!line.empty())
        {
            trackers.push_back(parseAddress(line));
        }
    }

    if (trackers.empty())
    {
        cerr << "Error: tracker_info.txt could not be read or is empty." << endl;
        return 1;
    }

    int sockfd = -1;
    int current_tracker_idx = 0;

    while (true)
    {
        if (sockfd < 0)
        {
            cout << "\nAttempting to connect to trackers..." << endl;
            bool connected = false;
            for (size_t i = 0; i < trackers.size(); ++i)
            {
                current_tracker_idx = (current_tracker_idx + i) % trackers.size();
                TrackerInfo current_tracker = trackers[current_tracker_idx];
                cout << "Trying " << current_tracker.ip << ":" << current_tracker.port << "..." << endl;
                sockfd = connectToTracker(current_tracker.ip.c_str(), current_tracker.port);
                if (sockfd >= 0)
                {
                    cout << "Connection successful to " << current_tracker.ip << ":" << current_tracker.port << endl;
                    cout << "NOTE: You may need to log in again." << endl;
                    connected = true;
                    break;
                }
            }
            if (!connected)
            {
                cerr << "Error: All trackers are down. Please try again later." << endl;
                return 1;
            }
        }

        cout << "> ";
        string input;
        if (!getline(cin, input))
            break;
        if (input == "exit")
            break;
        if (input.empty())
            continue;

        if (!processUserInput(sockfd, input))
        {
            close(sockfd);
            sockfd = -1;
            current_tracker_idx++;
        }
    }

    if (sockfd >= 0)
        close(sockfd);
    cout << "Client shutting down." << endl;
    return 0;
}