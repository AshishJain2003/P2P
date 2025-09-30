#include "commands.h"
#include "network.h"
#include "../sha1.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <chrono>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <map>

using namespace std;

extern mutex client_mutex;
extern int tracker_sock;
extern string self_listen_port;
extern map<string, string> shared_files_map;

void download_thread_func(string group_id, string filename, string dest_path, string tracker_response)
{
    istringstream iss(tracker_response);
    long long file_size;
    iss >> file_size;

    int num_pieces = (file_size + 524288 - 1) / 524288;
    vector<string> piece_hashes(num_pieces);
    for (int i = 0; i < num_pieces; ++i)
    {
        iss >> piece_hashes[i];
    }

    vector<string> peers;
    string peer_addr;
    while (iss >> peer_addr)
    {
        peers.push_back(peer_addr);
    }

    if (peers.empty())
    {
        cout << "Error: No peers found for this file." << endl;
        return;
    }

    int dest_fd = open(dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dest_fd < 0)
    {
        cout << "Error: Could not create destination file." << endl;
        return;
    }

    for (int i = 0; i < num_pieces; ++i)
    {
        bool piece_downloaded = false;
        while (!piece_downloaded)
        {
            string peer_to_try = peers[i % peers.size()];
            string peer_ip = peer_to_try.substr(0, peer_to_try.find(':'));
            int peer_port = stoi(peer_to_try.substr(peer_to_try.find(':') + 1));

            int peer_sock = connectToTracker(peer_ip.c_str(), peer_port);
            if (peer_sock >= 0)
            {
                string request = "GET " + filename + " " + to_string(i);
                send_msg(peer_sock, request);

                string piece_data;
                if (recv_msg(peer_sock, piece_data))
                {
                    string received_hash = SHA1::from_data(piece_data.c_str(), piece_data.length());

                    if (received_hash == piece_hashes[i])
                    {
                        off_t offset = (off_t)i * 524288;
                        pwrite(dest_fd, piece_data.c_str(), piece_data.length(), offset);

                        string update_cmd = "update_have_piece " + group_id + " " + filename + " " + to_string(i);
                        unique_lock<mutex> lock(client_mutex);
                        send_msg(tracker_sock, update_cmd);
                        lock.unlock();

                        piece_downloaded = true;
                    }
                }
                close(peer_sock);
            }
            if (!piece_downloaded)
                this_thread::sleep_for(chrono::seconds(1));
        }
    }

    close(dest_fd);
    cout << "[C] [" << group_id << "] " << filename << endl;
}

void processUserInput(const string &input)
{
    if (tracker_sock < 0)
    {
        cout << "Not connected to any tracker." << endl;
        return;
    }
    vector<string> tokens;
    istringstream iss(input);
    string word;
    while (iss >> word)
        tokens.push_back(word);
    if (tokens.empty())
        return;

    string command = tokens[0];

    if (command == "upload_file")
    {
        if (tokens.size() != 3)
        {
            cout << "Usage: upload_file <group_id> <file_path>" << endl;
            return;
        }
        string group_id = tokens[1], file_path = tokens[2];
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            cout << "Error: File not found or cannot be opened." << endl;
            return;
        }
        struct stat file_stat;
        if (fstat(fd, &file_stat) < 0)
        {
            cout << "Error: Could not get file size." << endl;
            close(fd);
            return;
        }
        long long file_size = file_stat.st_size;
        string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
        string command_to_send = "upload_file " + group_id + " " + filename + " " + to_string(file_size);
        char buffer[524288];
        int bytes_read;
        lseek(fd, 0, SEEK_SET);
        while ((bytes_read = read(fd, buffer, 524288)) > 0)
        {
            command_to_send += " " + SHA1::from_data(buffer, bytes_read);
        }
        close(fd);
        unique_lock<mutex> lock(client_mutex);
        string reply;
        if (send_msg(tracker_sock, command_to_send) && recv_msg(tracker_sock, reply))
        {
            cout << reply << endl;
            if (reply.rfind("OK:", 0) == 0)
            {
                shared_files_map[filename] = file_path;
            }
        }
        return;
    }
    else if (command == "download_file")
    {
        if (tokens.size() != 4)
        {
            cout << "Usage: download_file <group_id> <filename> <dest_path>" << endl;
            return;
        }
        string group_id = tokens[1], filename = tokens[2], dest_path = tokens[3];

        string cmd_to_tracker = input;
        string tracker_response;

        unique_lock<mutex> lock(client_mutex);
        if (send_msg(tracker_sock, cmd_to_tracker) && recv_msg(tracker_sock, tracker_response))
        {
            lock.unlock();
            if (tracker_response.rfind("Err:", 0) == 0)
            {
                cout << tracker_response << endl;
            }
            else
            {
                thread downloader(download_thread_func, group_id, filename, dest_path, tracker_response);
                downloader.detach();
            }
        }
        else
        {
            lock.unlock();
            close(tracker_sock);
            tracker_sock = -1;
        }
        return;
    }

    string reply;
    unique_lock<mutex> lock(client_mutex);
    if (send_msg(tracker_sock, input))
    {
        if (recv_msg(tracker_sock, reply))
        {
            lock.unlock();
            cout << reply << endl;
            if (command == "login" && reply.rfind("OK:", 0) == 0)
            {
                this_thread::sleep_for(chrono::milliseconds(200));
                string port_cmd = "register_port " + self_listen_port;
                string port_reply;
                if (send_msg(tracker_sock, port_cmd) && recv_msg(tracker_sock, port_reply))
                {
                    cout << port_reply << endl;
                }
            }
        }
        else
        {
            lock.unlock();
            close(tracker_sock);
            tracker_sock = -1;
        }
    }
    else
    {
        lock.unlock();
        close(tracker_sock);
        tracker_sock = -1;
    }
}