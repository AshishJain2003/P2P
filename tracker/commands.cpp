#include "commands.h"
#include "network.h"
#include "state.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <cstring>
using namespace std;

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
        cout << "[SYNC] User created: " << tokens[2] << endl;
    }
    else if (syncComm == "create_group" && tokens.size() == 4)
    {
        groups[tokens[2]].insert(tokens[3]);
        groupAdmin[tokens[2]] = tokens[3];
        cout << "[SYNC] Group created: " << tokens[2] << " by " << tokens[3] << endl;
    }
    else if (syncComm == "join_group" && tokens.size() == 4)
    {
        joinRequests[tokens[2]].insert(tokens[3]);
        cout << "[SYNC] Join request for group " << tokens[2] << " from " << tokens[3] << endl;
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
            cout << "[SYNC] Group deleted (empty): " << group_id << endl;
        }
        else if (groupAdmin[group_id] == user_id)
        {
            groupAdmin[group_id] = *groups[group_id].begin();
            cout << "[SYNC] Admin of group " << group_id << " reassigned" << endl;
        }
        cout << "[SYNC] User " << user_id << " left group " << group_id << endl;
    }
    else if (syncComm == "accept_request" && tokens.size() == 4)
    {
        joinRequests[tokens[2]].erase(tokens[3]);
        groups[tokens[2]].insert(tokens[3]);
        cout << "[SYNC] User " << tokens[3] << " accepted into group " << tokens[2] << endl;
    }
    else
    {
        cout << "[SYNC] Unknown sync command: " << syncComm << endl;
    }
    pthread_mutex_unlock(&state_mutex);
}

void handleCommand(int clientFd, string cmd, const TrackerInfo &peer_info)
{
    if (cmd == "SYNC_INIT")
    {
        if (peer < 0)
        {
            cout << "Handshake received. Connecting back to peer..." << endl;
            connectToPeerTracker(peer_info.ip.c_str(), peer_info.port);
        }
        return;
    }
    cout << "Command from " << (clientFd == peer ? "PEER" : "client") << " " << clientFd << ": " << cmd << endl;
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
    else
    {
        if (clientFd != peer)
            send_msg(clientFd, "Err: Wrong or unimplemented command");
    }
}