// #include "network.h"
// #include "commands.h"
// #include "state.h"
// #include <iostream>
// #include <unistd.h>
// using namespace std;

// #define PORT 9909

// int main() {
//     string peer_ip = "127.0.0.1";
//     int peer_port = 9910;

//     if (initServer(PORT) < 0) return 1;

//     connectToPeerTracker(peer_ip.c_str(), peer_port);

//     while (true) {
//         FD_ZERO(&fr);
//         FD_ZERO(&fw);
//         FD_ZERO(&fe);
//         FD_SET(trackerSocket, &fr);

//         for (int i = 0; i < 5; i++) {
//             if (clientSockets[i] > 0) {
//                 FD_SET(clientSockets[i], &fr);
//             }
//         }

//         if (peer >= 0) {
//             FD_SET(peer, &fr);
//         }

//         int activity = select(nMaxFd + 1, &fr, &fw, &fe, NULL);
//         if (activity < 0) {
//             cerr << "Select error\n";
//             break;
//         }

//         if (FD_ISSET(trackerSocket, &fr)) {
//             acceptNewClient();
//         }

//         char buffer[1024];

//         for (int i = 0; i < 5; i++) {
//             int fd = clientSockets[i];
//             if (fd > 0 && FD_ISSET(fd, &fr)) {
//                 int n = recvFromClient(fd, buffer, sizeof(buffer));
//                 if (n > 0) {
//                     handleCommand(fd, buffer);
//                 } else {
//                     pthread_mutex_lock(&state_mutex);
//                     if (session_by_fd.count(fd)) {
//                         string user_id = session_by_fd[fd];
//                         session_by_fd.erase(fd);
//                         fd_by_username.erase(user_id);
//                         cout << "User " << user_id
//                              << " logged out due to disconnect (fd=" << fd << ")" << endl;
//                     }
//                     pthread_mutex_unlock(&state_mutex);
//                     cout << "Client disconnected: " << fd << endl;
//                     close(fd);
//                     clientSockets[i] = 0;
//                 }
//             }
//         }

//         if (peer >= 0 && FD_ISSET(peer, &fr)) {
//             int n = recvFromPeerTracker(buffer, sizeof(buffer));
//             if (n > 0) {
//                 handleCommand(peer, buffer); 
//             }
//         }
//     }

//     return 0;
// }

















#include "network.h"
#include "commands.h"
#include "state.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <cstdlib>

using namespace std;

// Helper function to parse "IP:PORT" strings from the config file
pair<string, int> parseAddress(const string& address) {
    size_t colon_pos = address.find(':');
    if (colon_pos == string::npos) {
        cerr << "FATAL: Invalid address format in tracker_info.txt: " << address << endl;
        exit(1);
    }
    string ip = address.substr(0, colon_pos);
    int port = stoi(address.substr(colon_pos + 1));
    return {ip, port};
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " tracker_info.txt <tracker_no>" << endl;
        return 1;
    }

    string config_file_path = argv[1];
    int tracker_number = atoi(argv[2]);

    // Read tracker addresses from the config file
    ifstream config_file(config_file_path);
    if (!config_file.is_open()) {
        cerr << "Error: Could not open file " << config_file_path << endl;
        return 1;
    }

    vector<string> tracker_addresses;
    string line;
    while (getline(config_file, line)) {
        if (!line.empty()) {
            tracker_addresses.push_back(line);
        }
    }
    config_file.close();

    if (tracker_addresses.size() < 2) {
        cerr << "Error: tracker_info.txt must contain at least two tracker addresses." << endl;
        return 1;
    }
    
    if (tracker_number != 1 && tracker_number != 2) {
        cerr << "Error: tracker number must be 1 or 2." << endl;
        return 1;
    }

    // Determine this tracker's port and the peer's IP/port
    string my_address_str = tracker_addresses[tracker_number - 1];
    string peer_address_str = tracker_addresses[tracker_number == 1 ? 1 : 0]; // If I am 1, peer is 2 (index 1). If I am 2, peer is 1 (index 0).

    pair<string, int> my_details = parseAddress(my_address_str);
    pair<string, int> peer_details = parseAddress(peer_address_str);
    
    int listening_port = my_details.second;
    string peer_ip = peer_details.first;
    int peer_port = peer_details.second;

    // Initialize server on our designated port
    if (initServer(listening_port) < 0) {
        return 1;
    }
    
    // Give the other tracker a moment to start up before connecting
    sleep(1);

    // Connect to the peer tracker
    connectToPeerTracker(peer_ip.c_str(), peer_port);


    // Your original main loop starts here, with all variables unchanged
    while (true) {
        FD_ZERO(&fr);
        FD_ZERO(&fw);
        FD_ZERO(&fe);
        FD_SET(trackerSocket, &fr);

        for (int i = 0; i < 5; i++) {
            if (clientSockets[i] > 0) {
                FD_SET(clientSockets[i], &fr);
            }
        }

        if (peer >= 0) {
            FD_SET(peer, &fr);
        }

        int activity = select(nMaxFd + 1, &fr, &fw, &fe, NULL);
        if (activity < 0) {
            cerr << "Select error\n";
            break;
        }

        if (FD_ISSET(trackerSocket, &fr)) {
            acceptNewClient();
        }

        char buffer[1024];

        for (int i = 0; i < 5; i++) {
            int fd = clientSockets[i];
            if (fd > 0 && FD_ISSET(fd, &fr)) {
                int n = recvFromClient(fd, buffer, sizeof(buffer));
                if (n > 0) {
                    handleCommand(fd, buffer);
                } else {
                    pthread_mutex_lock(&state_mutex);
                    if (session_by_fd.count(fd)) {
                        string user_id = session_by_fd[fd];
                        session_by_fd.erase(fd);
                        fd_by_username.erase(user_id);
                        cout << "User " << user_id
                             << " logged out due to disconnect (fd=" << fd << ")" << endl;
                    }
                    pthread_mutex_unlock(&state_mutex);
                    cout << "Client disconnected: " << fd << endl;
                    close(fd);
                    clientSockets[i] = 0;
                }
            }
        }

        if (peer >= 0 && FD_ISSET(peer, &fr)) {
            int n = recvFromPeerTracker(buffer, sizeof(buffer));
            if (n > 0) {
                handleCommand(peer, buffer); 
            }
        }
    }

    return 0;
}