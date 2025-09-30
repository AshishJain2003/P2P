#include "commands.h"
#include "network.h"
#include "state.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>

using namespace std;

vector<string> tokenize(string cmd);
void handleSyncCommand(vector<string> &tokens);
void handleCommand(int clientFd, string cmd, const TrackerInfo &peer_info);

vector<string> tokenize(string cmd)
{
    vector<string> tokens;
    istringstream iss(cmd);
    string word;
    while (iss >> word)
        tokens.push_back(word);
    return tokens;
}

void handleSyncCommand(vector<string> &tokens)
{
    if (tokens.size() < 2)
        return;
    string syncComm = tokens[1];
    pthread_mutex_lock(&state_mutex);
    if (syncComm == "create_user" && tokens.size() == 4)
    {
        users[tokens[2]] = tokens[3];
    }
    else if (syncComm == "create_group" && tokens.size() == 4)
    {
        groups[tokens[2]].insert(tokens[3]);
        groupAdmin[tokens[2]] = tokens[3];
    }
    else if (syncComm == "join_group" && tokens.size() == 4)
    {
        joinRequests[tokens[2]].insert(tokens[3]);
    }
    else if (syncComm == "leave_group" && tokens.size() == 4)
    {
        string group_id = tokens[2], user_id = tokens[3];
        groups[group_id].erase(user_id);
        if (groups[group_id].empty())
        {
            groups.erase(group_id);
            groupAdmin.erase(group_id);
            joinRequests.erase(group_id);
        }
        else if (groupAdmin[group_id] == user_id)
        {
            groupAdmin[group_id] = *groups[group_id].begin();
        }
    }
    else if (syncComm == "accept_request" && tokens.size() == 4)
    {
        joinRequests[tokens[2]].erase(tokens[3]);
        groups[tokens[2]].insert(tokens[3]);
    }
    else if (syncComm == "upload_file")
    {
        string group_id = tokens[2];
        string filename = tokens[3];
        group_files[group_id][filename].file_size = stoll(tokens[4]);
        group_files[group_id][filename].piece_hashes.clear();
        for (size_t i = 5; i < tokens.size(); ++i)
        {
            group_files[group_id][filename].piece_hashes.push_back(tokens[i]);
        }
    }
    else if (syncComm == "update_piece_info")
    {
        string filename = tokens[2];
        int piece_index = stoi(tokens[3]);
        string peer_addr = tokens[4];
        piece_info[filename][piece_index].insert(peer_addr);
    }
    pthread_mutex_unlock(&state_mutex);
}

void handleCommand(int clientFd, string cmd, const TrackerInfo &peer_info)
{
    if (cmd == "SYNC_INIT")
    {
        if (peer < 0)
        {
            connectToPeerTracker(peer_info.ip.c_str(), peer_info.port);
        }
        return;
    }
    vector<string> tokens = tokenize(cmd);
    if (tokens.empty())
    {
        send_msg(clientFd, "Err: Empty Command");
        return;
    }
    if (tokens[0] == "SYNC")
    {
        handleSyncCommand(tokens);
        return;
    }
    string comm = tokens[0];
    if (comm == "create_user")
    {
        if (tokens.size() != 3)
        {
            send_msg(clientFd, "Err: Usage: create_user <user_id> <password>");
            return;
        }
        string user_id = tokens[1], password = tokens[2];
        pthread_mutex_lock(&state_mutex);
        if (users.count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: User already exists");
            return;
        }
        users[user_id] = password;
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC create_user " + user_id + " " + password);
        send_msg(clientFd, "OK: User Created");
    }
    else if (comm == "login")
    {
        if (tokens.size() != 3)
        {
            send_msg(clientFd, "Err: Usage: login <user_id> <password>");
            return;
        }
        string user_id = tokens[1], password = tokens[2];
        pthread_mutex_lock(&state_mutex);
        if (!users.count(user_id) || users[user_id] != password)
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Invalid credentials");
            return;
        }
        if (fd_by_username.count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: User already logged in");
            return;
        }
        session_by_fd[clientFd] = user_id;
        fd_by_username[user_id] = clientFd;
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, "OK: Login successful");
    }
    else if (comm == "logout")
    {
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You are not logged in");
            return;
        }
        string user_id = session_by_fd[clientFd];
        peer_addresses.erase(user_id);
        session_by_fd.erase(clientFd);
        fd_by_username.erase(user_id);
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, "OK: Logout successful");
    }
    else if (comm == "create_group")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Usage: create_group <group_id>");
            return;
        }
        string group_id = tokens[1];
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You must be logged in");
            return;
        }
        if (groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group already exists");
            return;
        }
        string user_id = session_by_fd[clientFd];
        groups[group_id].insert(user_id);
        groupAdmin[group_id] = user_id;
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC create_group " + group_id + " " + user_id);
        send_msg(clientFd, "OK: Group created");
    }
    else if (comm == "join_group")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Usage: join_group <group_id>");
            return;
        }
        string group_id = tokens[1];
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You must be logged in");
            return;
        }
        if (!groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group does not exist");
            return;
        }
        string user_id = session_by_fd[clientFd];
        if (groups[group_id].count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You are already in the group");
            return;
        }
        joinRequests[group_id].insert(user_id);
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC join_group " + group_id + " " + user_id);
        send_msg(clientFd, "OK: Request to join group sent");
    }
    else if (comm == "leave_group")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Usage: leave_group <group_id>");
            return;
        }
        string group_id = tokens[1];
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You must be logged in");
            return;
        }
        if (!groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group does not exist");
            return;
        }
        string user_id = session_by_fd[clientFd];
        if (!groups[group_id].count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You are not a member of this group");
            return;
        }
        groups[group_id].erase(user_id);
        if (groups[group_id].empty())
        {
            groups.erase(group_id);
            groupAdmin.erase(group_id);
            joinRequests.erase(group_id);
        }
        else if (groupAdmin[group_id] == user_id)
        {
            groupAdmin[group_id] = *groups[group_id].begin();
        }
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC leave_group " + group_id + " " + user_id);
        send_msg(clientFd, "OK: You have left the group");
    }
    else if (comm == "list_requests")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Usage: list_requests <group_id>");
            return;
        }
        string group_id = tokens[1];
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You must be logged in");
            return;
        }
        if (!groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group does not exist");
            return;
        }
        string user_id = session_by_fd[clientFd];
        if (groupAdmin[group_id] != user_id)
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Permission denied (not group owner)");
            return;
        }
        string response = "Pending requests for " + group_id + ":\n";
        if (!joinRequests.count(group_id) || joinRequests[group_id].empty())
        {
            response += "(None)";
        }
        else
        {
            for (const auto &req_user : joinRequests[group_id])
            {
                response += req_user + "\n";
            }
        }
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, response);
    }
    else if (comm == "accept_request")
    {
        if (tokens.size() != 3)
        {
            send_msg(clientFd, "Err: Usage: accept_request <group_id> <user_id>");
            return;
        }
        string group_id = tokens[1], user_to_accept = tokens[2];
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: You must be logged in");
            return;
        }
        if (!groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group does not exist");
            return;
        }
        string admin_id = session_by_fd[clientFd];
        if (groupAdmin[group_id] != admin_id)
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Permission denied (not group owner)");
            return;
        }
        if (!joinRequests.count(group_id) || !joinRequests[group_id].count(user_to_accept))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: No pending request from this user");
            return;
        }
        joinRequests[group_id].erase(user_to_accept);
        groups[group_id].insert(user_to_accept);
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC accept_request " + group_id + " " + user_to_accept);
        send_msg(clientFd, "OK: User accepted into group");
    }
    else if (comm == "list_groups")
    {
        pthread_mutex_lock(&state_mutex);
        string response = "Available groups:\n";
        if (groups.empty())
        {
            response += "(None)";
        }
        else
        {
            for (auto const &[group_id, members] : groups)
            {
                response += group_id + "\n";
            }
        }
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, response);
    }
    else if (comm == "register_port")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Invalid command");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Not logged in");
            return;
        }
        string user_id = session_by_fd[clientFd];
        string port = tokens[1];
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        getpeername(clientFd, (struct sockaddr *)&client_addr, &client_len);
        string client_ip = inet_ntoa(client_addr.sin_addr);
        peer_addresses[user_id] = client_ip + ":" + port;
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, "OK: Port registered");
    }
    else if (comm == "upload_file")
    {
        if (tokens.size() < 4)
        {
            send_msg(clientFd, "Err: Incomplete upload command");
            return;
        }
        string group_id = tokens[1], filename = tokens[2];
        long long file_size = stoll(tokens[3]);
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Not logged in");
            return;
        }
        string user_id = session_by_fd[clientFd];
        if (!groups.count(group_id) || !groups[group_id].count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Not a member of this group");
            return;
        }
        if (group_files.count(group_id) && group_files[group_id].count(filename))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: File with this name already exists in the group");
            return;
        }
        string seeder_addr = peer_addresses[user_id];
        if (seeder_addr.empty())
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Client port not registered. Please re-login.");
            return;
        }
        FileInfo info;
        info.file_size = file_size;
        for (size_t i = 4; i < tokens.size(); i++)
            info.piece_hashes.push_back(tokens[i]);
        group_files[group_id][filename] = info;
        for (size_t i = 0; i < info.piece_hashes.size(); i++)
            piece_info[filename][i].insert(seeder_addr);
        string sync_msg = "SYNC";
        for (const auto &token : tokens)
            sync_msg += " " + token;
        send_msg(peer, sync_msg);
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, "OK: File shared successfully");
    }
    else if (comm == "list_files")
    {
        if (tokens.size() != 2)
        {
            send_msg(clientFd, "Err: Usage: list_files <group_id>");
            return;
        }
        string group_id = tokens[1];
        pthread_mutex_lock(&state_mutex);
        if (!groups.count(group_id))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: Group does not exist");
            return;
        }
        string response = "Files in " + group_id + ":\n";
        if (!group_files.count(group_id) || group_files[group_id].empty())
        {
            response += "(None)";
        }
        else
        {
            for (auto const &[filename, info] : group_files[group_id])
            {
                response += filename + "\n";
            }
        }
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, response);
    }
    else if (comm == "download_file")
    {
        if (tokens.size() != 4)
        {
            send_msg(clientFd, "Err: Usage: download_file <group_id> <filename> <destination_path>");
            return;
        }
        string group_id = tokens[1];
        string filename = tokens[2];
        pthread_mutex_lock(&state_mutex);
        if (!group_files.count(group_id) || !group_files[group_id].count(filename))
        {
            pthread_mutex_unlock(&state_mutex);
            send_msg(clientFd, "Err: File not found in group");
            return;
        }
        string response = "";
        FileInfo &info = group_files[group_id][filename];
        response += to_string(info.file_size);
        for (const auto &hash : info.piece_hashes)
        {
            response += " " + hash;
        }
        set<string> peers_with_any_piece;
        if (piece_info.count(filename))
        {
            for (auto const &[piece_index, peer_set] : piece_info[filename])
            {
                for (const auto &peer_addr : peer_set)
                {
                    peers_with_any_piece.insert(peer_addr);
                }
            }
        }
        for (const auto &peer_addr : peers_with_any_piece)
        {
            response += " " + peer_addr;
        }
        pthread_mutex_unlock(&state_mutex);
        send_msg(clientFd, response);
    }
    else if (comm == "update_have_piece")
    {
        if (tokens.size() != 4)
            return;
        string group_id = tokens[1], filename = tokens[2], seeder_addr = peer_addresses[session_by_fd[clientFd]];
        int piece_index = stoi(tokens[3]);
        pthread_mutex_lock(&state_mutex);
        piece_info[filename][piece_index].insert(seeder_addr);
        pthread_mutex_unlock(&state_mutex);
        send_msg(peer, "SYNC update_piece_info " + filename + " " + to_string(piece_index) + " " + seeder_addr);
    }
    else
    {
        if (clientFd != peer)
            send_msg(clientFd, "Err: Wrong or unimplemented command");
    }
}