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
#include <set>
#include <functional>

using namespace std;

extern mutex client_mutex;
extern int tracker_sock;
extern string self_listen_port;
extern map<string, string> shared_files_map;

mutex cout_mutex;

struct DownloadState
{
    string status = "[D]";
    string group_id;
    string filename;
    int pieces_downloaded = 0;
    int total_pieces = 0;
};

map<string, DownloadState> active_downloads;
mutex downloads_mutex;

void downloader_worker_func(
    int worker_id,
    vector<int> &pieces_to_download,
    mutex &pieces_mutex,
    const string &group_id,
    const string &filename,
    int dest_fd,
    const vector<string> &piece_hashes,
    const map<int, set<string>> &piece_peers,
    size_t &next_peer_idx,
    mutex &peer_idx_mutex)
{
    while (true)
    {
        int piece_index = -1;
        pieces_mutex.lock();
        if (!pieces_to_download.empty())
        {
            piece_index = pieces_to_download.back();
            pieces_to_download.pop_back();
        }
        pieces_mutex.unlock();
        if (piece_index == -1)
            return;

        bool piece_downloaded = false;
        while (!piece_downloaded)
        {
            auto it = piece_peers.find(piece_index);
            if (it == piece_peers.end() || it->second.empty())
            {
                this_thread::sleep_for(chrono::seconds(2));
                continue;
            }

            vector<string> available_peers(it->second.begin(), it->second.end());

            string peer_to_try = available_peers[rand() % available_peers.size()];

            string peer_ip = peer_to_try.substr(0, peer_to_try.find(':'));
            int peer_port = stoi(peer_to_try.substr(peer_to_try.find(':') + 1));

            int peer_sock = connectToTracker(peer_ip.c_str(), peer_port);
            if (peer_sock >= 0)
            {
                string request = "GET " + filename + " " + to_string(piece_index);
                send_msg(peer_sock, request);
                string piece_data;
                if (recv_msg(peer_sock, piece_data))
                {
                    string received_hash = sha1_from_data(piece_data.c_str(), piece_data.length());
                    if (received_hash == piece_hashes[piece_index])
                    {
                        cout_mutex.lock();
                        cout << "Downloaded piece " << piece_index << " from " << peer_to_try 
                             << " [Worker " << worker_id << "]" << endl;
                        cout_mutex.unlock();

                        off_t offset = (off_t)piece_index * 524288;
                        pwrite(dest_fd, piece_data.c_str(), piece_data.length(), offset);
                        
                        string update_cmd = "update_have_piece " + group_id + " " + filename + " " + to_string(piece_index);
                        unique_lock<mutex> lock(client_mutex);
                        send_msg(tracker_sock, update_cmd);
                        lock.unlock();
                        
                        downloads_mutex.lock();
                        active_downloads[filename].pieces_downloaded++;
                        int downloaded = active_downloads[filename].pieces_downloaded;
                        int total = active_downloads[filename].total_pieces;
                        downloads_mutex.unlock();
                        
                        if (downloaded % 5 == 0 || (downloaded * 100 / total) % 10 == 0)
                        {
                            cout_mutex.lock();
                            cout << "Progress: " << downloaded << "/" << total 
                                 << " (" << (downloaded * 100 / total) << "%)" << endl;
                            cout_mutex.unlock();
                        }
                        
                        piece_downloaded = true;
                    }
                }
                close(peer_sock);
            }
            if (!piece_downloaded)
                this_thread::sleep_for(chrono::seconds(1));
        }
    }
}

void download_thread_func(string group_id, string filename, string dest_path, string tracker_response)
{
    istringstream iss(tracker_response);
    long long file_size;
    iss >> file_size;
    int num_pieces = (file_size + 524288 - 1) / 524288;
    vector<string> piece_hashes(num_pieces);
    for (int i = 0; i < num_pieces; ++i)
        iss >> piece_hashes[i];
    
    map<int, set<string>> piece_peers;
    string token;
    int current_piece = -1;
    while (iss >> token)
    {
        if (token == "|")
            current_piece++;
        else if (current_piece >= 0)
            piece_peers[current_piece].insert(token);
    }

    int dest_fd = open(dest_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (dest_fd < 0)
        return;

    downloads_mutex.lock();
    DownloadState state;
    state.status = "[D]"; 
    state.group_id = group_id;
    state.filename = filename;
    state.total_pieces = num_pieces;
    state.pieces_downloaded = 0;
    active_downloads[filename] = state;
    downloads_mutex.unlock();

    cout << "[D] Started downloading: [" << group_id << "] " << filename 
         << " (0/" << num_pieces << " pieces)" << endl;

    vector<int> pieces_to_download;
    for (int i = 0; i < num_pieces; ++i)
        pieces_to_download.push_back(i);
    mutex pieces_mutex;

    size_t next_peer_idx = 0;
    mutex peer_idx_mutex;

    vector<thread> workers;
    for (int i = 0; i < 4; ++i)
    {
        workers.emplace_back(downloader_worker_func, i, ref(pieces_to_download), 
                           ref(pieces_mutex), group_id, filename, dest_fd, 
                           piece_hashes, piece_peers, ref(next_peer_idx), 
                           ref(peer_idx_mutex));
    }
    for (auto &t : workers)
        t.join();
    close(dest_fd);

    client_mutex.lock();
    shared_files_map[filename] = dest_path;
    client_mutex.unlock();

    downloads_mutex.lock();
    active_downloads[filename].status = "[C]"; 
    downloads_mutex.unlock();
    
    cout << "\n[C] [" << group_id << "] " << filename << " - Download Complete!" << endl
         << "> " << flush;
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
            command_to_send += " " + sha1_from_data(buffer, bytes_read);
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
    else if (command == "show_downloads")
    {
        downloads_mutex.lock();
        if (active_downloads.empty())
        {
            cout << "No active or completed downloads." << endl;
        }
        else
        {
            for (auto const &[filename, state] : active_downloads)
            {
                if (state.status == "[D]")
                {
                    int progress_percent = 0;
                    if (state.total_pieces > 0)
                    {
                        progress_percent = (state.pieces_downloaded * 100) / state.total_pieces;
                    }
                    cout << "[D] [" << state.group_id << "] " << filename
                         << " (" << state.pieces_downloaded << "/" << state.total_pieces
                         << " pieces - " << progress_percent << "%)" << endl;
                }
                else if (state.status == "[C]")
                {
                    cout << "[C] [" << state.group_id << "] " << filename << endl;
                }
            }
        }
        downloads_mutex.unlock();
        return;
    }
    else if (command == "stop_share")
    {
        if (tokens.size() != 3)
        {
            cout << "Usage: stop_share <group_id> <filename>" << endl;
            return;
        }
        string group_id = tokens[1], filename = tokens[2];
        shared_files_map.erase(filename);
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