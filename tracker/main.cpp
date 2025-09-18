#include "network.h"
#include "commands.h"
#include "state.h"
#include "main.h" 
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h> 
#include <sys/stat.h>

using namespace std;

TrackerInfo parseAddress(const string& addr) {
    TrackerInfo info;
    size_t colon_pos = addr.find(':');
    if (colon_pos == string::npos) {
        cerr << "Invalid address format: " << addr << endl;
        exit(1);
    }
    info.ip = addr.substr(0, colon_pos);
    info.port = stoi(addr.substr(colon_pos + 1));
    return info;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./tracker.out <tracker_info_file> <tracker_no>" << endl;
        return 1;
    }
    string tracker_file_path = argv[1];
    int tracker_num = atoi(argv[2]);
    if (tracker_num != 1 && tracker_num != 2) {
        cerr << "Error: tracker_no must be 1 or 2." << endl;
        return 1;
    }
    
    vector<TrackerInfo> trackers;
    int fd = open(tracker_file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("Error opening tracker_info.txt");
        return 1;
    }
    char buffer[1024];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes_read < 0) {
        perror("Error reading tracker_info.txt");
        return 1;
    }
    buffer[bytes_read] = '\0';
    stringstream ss(buffer);
    string line;
    while (getline(ss, line)) {
        if (!line.empty()) {
            trackers.push_back(parseAddress(line));
        }
    }

    if (trackers.size() != 2) {
        cerr << "Error: tracker_info.txt must contain exactly two tracker addresses." << endl;
        return 1;
    }
    
    TrackerInfo my_info = trackers[tracker_num - 1];
    TrackerInfo peer_info = trackers[tracker_num == 1 ? 1 : 0];

    if (initServer(my_info.port) < 0) return 1;
    connectToPeerTracker(peer_info.ip.c_str(), peer_info.port);
    
    while (true) {
        FD_ZERO(&fr);
        FD_SET(trackerSocket, &fr);
        for (int i = 0; i < 5; i++) {
            if (clientSockets[i] > 0) FD_SET(clientSockets[i], &fr);
        }
        if (peer >= 0) FD_SET(peer, &fr);

        nMaxFd = trackerSocket;
        if(peer > nMaxFd) nMaxFd = peer;
        for(int i=0; i<5; ++i) {
            if(clientSockets[i] > nMaxFd) nMaxFd = clientSockets[i];
        }

        struct timeval tv = {1, 0};
        int activity = select(nMaxFd + 1, &fr, NULL, NULL, &tv);

        if (activity < 0) { cerr << "Select error\n"; break; }
        if (activity == 0) continue; 

        if (FD_ISSET(trackerSocket, &fr)) acceptNewClient();
        
        string received_cmd;

        for (int i = 0; i < 5; i++) {
            int sock_fd = clientSockets[i];
            if (sock_fd > 0 && FD_ISSET(sock_fd, &fr)) {
                if (!recv_msg(sock_fd, received_cmd)) {
                    pthread_mutex_lock(&state_mutex);
                    if (session_by_fd.count(sock_fd)) {
                        string user_id = session_by_fd[sock_fd];
                        session_by_fd.erase(sock_fd);
                        fd_by_username.erase(user_id);
                        cout << "User " << user_id << " logged out due to disconnect (fd=" << sock_fd << ")" << endl;
                    }
                    pthread_mutex_unlock(&state_mutex);
                    cout << "Client disconnected: " << sock_fd << endl;
                    close(sock_fd);
                    clientSockets[i] = 0;
                } else {
                     handleCommand(sock_fd, received_cmd, peer_info);
                }
            }
        }
        
        if (peer >= 0 && FD_ISSET(peer, &fr)) {
            if (!recv_msg(peer, received_cmd)) {
                cout << "Peer tracker disconnected." << endl;
                close(peer);
                peer = -1;
            } else {
                handleCommand(peer, received_cmd, peer_info);
            }
        }
    }
    return 0;
}