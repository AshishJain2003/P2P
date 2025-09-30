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
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <chrono>
#include <map>

using namespace std;

mutex client_mutex;
int tracker_sock = -1;
string self_seeder_address;
string self_listen_port;
map<string, string> shared_files_map;

struct TrackerInfo
{
    string ip;
    int port;
};

void processUserInput(const string &input);
int connectToTracker(const char *ip, int port);

string read_file_contents(const string &path, bool &success)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        success = false;
        return "";
    }
    char buffer[1024];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes_read < 0)
    {
        success = false;
        return "";
    }
    buffer[bytes_read] = '\0';
    success = true;
    return string(buffer);
}

void handle_peer_connection(int peer_sock)
{
    string request_str;
    if (recv_msg(peer_sock, request_str))
    {
        vector<string> tokens;
        istringstream iss(request_str);
        string word;
        while (iss >> word)
            tokens.push_back(word);

        if (tokens.size() == 3 && tokens[0] == "GET")
        {
            string filename = tokens[1];
            int piece_index = stoi(tokens[2]);

            unique_lock<mutex> lock(client_mutex);
            string file_path = shared_files_map[filename];
            lock.unlock();

            if (!file_path.empty())
            {
                int fd = open(file_path.c_str(), O_RDONLY);
                if (fd >= 0)
                {
                    char piece_buffer[524288];
                    off_t offset = (off_t)piece_index * 524288;
                    int bytes_read = pread(fd, piece_buffer, 524288, offset);

                    if (bytes_read > 0)
                    {
                        string piece_data(piece_buffer, bytes_read);
                        send_msg(peer_sock, piece_data);
                    }
                    close(fd);
                }
            }
        }
    }
    close(peer_sock);
}

void seeder_thread_func()
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
        return;
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(0);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        return;
    sockaddr_in bound_addr;
    socklen_t addr_len = sizeof(bound_addr);
    if (getsockname(listen_sock, (struct sockaddr *)&bound_addr, &addr_len) == -1)
        return;
    self_listen_port = to_string(ntohs(bound_addr.sin_port));
    self_seeder_address = "127.0.0.1:" + self_listen_port;
    if (listen(listen_sock, 10) < 0)
        return;
    cout << "Client now listening for other peers on port " << self_listen_port << endl;
    while (true)
    {
        int peer_sock = accept(listen_sock, NULL, NULL);
        if (peer_sock >= 0)
        {
            thread peer_handler(handle_peer_connection, peer_sock);
            peer_handler.detach();
        }
    }
}

TrackerInfo parseAddress(const string &addr)
{
    TrackerInfo info;
    size_t colon_pos = addr.find(':');
    info.ip = addr.substr(0, colon_pos);
    info.port = stoi(addr.substr(colon_pos + 1));
    return info;
}

void connect_to_tracker_with_failover(const string &tracker_info_file)
{
    bool read_success = false;
    string file_contents = read_file_contents(tracker_info_file, read_success);
    if (!read_success)
        return;
    stringstream ss(file_contents);
    string line;
    vector<TrackerInfo> trackers;
    while (getline(ss, line))
    {
        if (!line.empty())
            trackers.push_back(parseAddress(line));
    }
    if (trackers.empty())
        return;
    for (size_t i = 0; i < trackers.size(); ++i)
    {
        TrackerInfo current_tracker = trackers[i];
        int new_sock = connectToTracker(current_tracker.ip.c_str(), current_tracker.port);
        if (new_sock >= 0)
        {
            cout << "Connection successful to " << current_tracker.ip << ":" << current_tracker.port << endl;
            tracker_sock = new_sock;
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    if (argc != 3)
    {
        cerr << "Usage: ./client.out <tracker_ip:port> <tracker_info_file>" << endl;
        return 1;
    }
    string tracker_info_file = argv[2];
    thread seeder(seeder_thread_func);
    seeder.detach();
    while (true)
    {
        if (tracker_sock < 0)
        {
            connect_to_tracker_with_failover(tracker_info_file);
            if (tracker_sock < 0)
            {
                this_thread::sleep_for(chrono::seconds(5));
                continue;
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
        processUserInput(input);
    }
    if (tracker_sock >= 0)
        close(tracker_sock);
    return 0;
}