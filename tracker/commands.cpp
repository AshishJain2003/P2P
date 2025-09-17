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
    {
        tokens.push_back(word);
    }
    return tokens;
}

void sendMsg(int fd, string msg)
{
    send(fd, msg.c_str(), msg.size(), 0);
}

void handleSyncCommand(vector<string> &tokens)
{
    if (tokens.size() < 2)
        return;
    string syncComm = tokens[1];

    if (syncComm == "create_user" && tokens.size() == 4)
    {
        string user_id = tokens[2];
        string password = tokens[3];
        pthread_mutex_lock(&state_mutex);
        users[user_id] = password;
        pthread_mutex_unlock(&state_mutex);
        cout << "[SYNC] User created: " << user_id << endl;
    }

    // else if (syncComm == "login" && tokens.size() == 4) {
    //     string user_id = tokens[2];
    //     string fd_str = tokens[3];
    //     int fakeFd = stoi(fd_str);
    //     pthread_mutex_lock(&state_mutex);
    //     fd_by_username[user_id] = fakeFd;
    //     session_by_fd[fakeFd] = user_id;
    //     pthread_mutex_unlock(&state_mutex);
    //     cout << "[SYNC] User logged in: " << user_id << endl;
    // }

    // else if (syncComm == "logout" && tokens.size() == 3) {
    //     string user_id = tokens[2];
    //     pthread_mutex_lock(&state_mutex);
    //     if (fd_by_username.count(user_id)) {
    //         int fd = fd_by_username[user_id];
    //         fd_by_username.erase(user_id);
    //         session_by_fd.erase(fd);
    //     }
    //     pthread_mutex_unlock(&state_mutex);
    //     cout << "[SYNC] User logged out: " << user_id << endl;
    // }

    else if (syncComm == "create_group" && tokens.size() == 4)
    {
        string grp_id = tokens[2];
        string user_id = tokens[3];
        pthread_mutex_lock(&state_mutex);
        groups[grp_id].insert(user_id);
        groupAdmin[grp_id] = user_id;
        pthread_mutex_unlock(&state_mutex);
        cout << "[SYNC] Group created: " << grp_id << " by " << user_id << endl;
    }

    else if (syncComm == "join_group" && tokens.size() == 4)
    {
        string grp_id = tokens[2];
        string user_id = tokens[3];
        pthread_mutex_lock(&state_mutex);
        joinRequests[grp_id].insert(user_id);
        pthread_mutex_unlock(&state_mutex);
        cout << "[SYNC] " << user_id << " requested to join group " << grp_id << endl;
    }

    else if (syncComm == "leave_group" && tokens.size() == 4)
    {
        string grp_id = tokens[2];
        string user_id = tokens[3];
        pthread_mutex_lock(&state_mutex);
        if (groups.count(grp_id))
        {
            groups[grp_id].erase(user_id);
            if (groups[grp_id].empty())
            {
                groups.erase(grp_id);
                groupAdmin.erase(grp_id);
                joinRequests.erase(grp_id);
                cout << "[SYNC] Group " << grp_id << " deleted (empty)" << endl;
            }
            else if (groupAdmin[grp_id] == user_id)
            {
                groupAdmin[grp_id] = *groups[grp_id].begin();
            }
        }
        pthread_mutex_unlock(&state_mutex);
        cout << "[SYNC] " << user_id << " left group " << grp_id << endl;
    }

    else if (syncComm == "accept_request" && tokens.size() == 4)
    {
        string grp_id = tokens[2];
        string final_user = tokens[3];
        pthread_mutex_lock(&state_mutex);
        groups[grp_id].insert(final_user);
        joinRequests[grp_id].erase(final_user);
        pthread_mutex_unlock(&state_mutex);
        cout << "[SYNC] User " << final_user << " added to group " << grp_id << endl;
    }

    else
    {
        cout << "[SYNC] Unknown sync command: " << syncComm << endl;
    }
}

void handleCommand(int clientFd, string cmd)
{
    cout << "Command from client " << clientFd << ": " << cmd << endl;

    vector<string> tokens = tokenize(cmd);
    if (tokens.empty())
    {
        sendMsg(clientFd, "Err: Wrong Syntax");
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
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }

        string user_id = tokens[1];
        string password = tokens[2];

        pthread_mutex_lock(&state_mutex);
        if (users.count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : User already exists");
            return;
        }

        users[user_id] = password;
        pthread_mutex_unlock(&state_mutex);

        string sync_msg = "SYNC create_user " + user_id + " " + password;
        sendToPeerTracker((char *)sync_msg.c_str());

        cout << "User created : " << user_id << endl;
        sendMsg(clientFd, "OK User Created");
    }
    else if (comm == "login")
    {
        if (tokens.size() != 3)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        string user_id = tokens[1], password = tokens[2];
        string reply_msg; 

        pthread_mutex_lock(&state_mutex);
        if (!users.count(user_id))
        {
            reply_msg = "Err : User doesn't exists";
        }
        else if (users[user_id] != password)
        {
            reply_msg = "Err : Wrong password";
        }
        else if (fd_by_username.count(user_id))
        {
            reply_msg = "Err : Already Logged in";
        }
        else
        {
            session_by_fd[clientFd] = user_id;
            fd_by_username[user_id] = clientFd;
            cout << "User logged in: " << user_id << " (fd=" << clientFd << ")" << endl;
            reply_msg = "OK Login successful";
        }
        pthread_mutex_unlock(&state_mutex);

        sendMsg(clientFd, reply_msg);
    }
    else if (comm == "logout")
    {
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }
        string user_id = session_by_fd[clientFd];
        session_by_fd.erase(clientFd);
        fd_by_username.erase(user_id);
        pthread_mutex_unlock(&state_mutex);

        cout << "User logged out : " << user_id << " (fd=" << clientFd << ")" << endl;
        sendMsg(clientFd, "OK Logout successfully!");
    }
    else if (comm == "create_group")
    {
        if (tokens.size() != 2)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }

        string grp_id = tokens[1];
        string user_id = session_by_fd[clientFd];

        if (groups.count(grp_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Group Already Exists");
            return;
        }

        groups[grp_id].insert(user_id);
        groupAdmin[grp_id] = user_id;
        pthread_mutex_unlock(&state_mutex);

        string sync_msg = "SYNC create_group " + grp_id + " " + user_id;
        sendToPeerTracker((char *)sync_msg.c_str());

        cout << "Group created: " << grp_id << " by " << user_id << endl;
        sendMsg(clientFd, "OK Group created");
    }
    else if (comm == "join_group")
    {
        if (tokens.size() != 2)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }
        string grp_id = tokens[1];
        string user_id = session_by_fd[clientFd];

        if (!groups.count(grp_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No such group");
            return;
        }

        joinRequests[grp_id].insert(user_id);
        pthread_mutex_unlock(&state_mutex);

        string sync_msg = "SYNC join_group " + grp_id + " " + user_id;
        sendToPeerTracker((char *)sync_msg.c_str());

        cout << user_id << " requested to join group " << grp_id << endl;
        sendMsg(clientFd, "OK Join request sent");
    }
    else if (comm == "leave_group")
    {
        if (tokens.size() != 2)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }
        string grp_id = tokens[1];
        string user_id = session_by_fd[clientFd];

        if (!groups.count(grp_id) || !groups[grp_id].count(user_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not In Group");
            return;
        }

        groups[grp_id].erase(user_id);
        if (groups[grp_id].empty())
        {
            groups.erase(grp_id);
            groupAdmin.erase(grp_id);
            joinRequests.erase(grp_id);
            cout << "Group " << grp_id << " deleted (empty)" << endl;
        }
        else if (groupAdmin[grp_id] == user_id)
        {
            groupAdmin[grp_id] = *groups[grp_id].begin();
        }

        pthread_mutex_unlock(&state_mutex);

        string sync_msg = "SYNC leave_group " + grp_id + " " + user_id;
        sendToPeerTracker((char *)sync_msg.c_str());

        cout << user_id << " left group " << grp_id << endl;
        sendMsg(clientFd, "OK Left group");
    }
    else if (comm == "list_groups")
    {
        pthread_mutex_lock(&state_mutex);
        string ans;
        for (auto &g : groups)
        {
            ans += g.first + "\n";
        }
        pthread_mutex_unlock(&state_mutex);
        if (ans.empty())
            ans = "No groups available";
        sendMsg(clientFd, ans);
    }
    else if (comm == "list_requests")
    {
        if (tokens.size() != 2)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }
        string grp_id = tokens[1];
        string user_id = session_by_fd[clientFd];

        if (!groups.count(grp_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No such group");
            return;
        }
        if (groupAdmin[grp_id] != user_id)
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No admin");
            return;
        }

        string ans;
        for (auto &u : joinRequests[grp_id])
        {
            ans += u + "\n";
        }
        pthread_mutex_unlock(&state_mutex);

        if (ans.empty())
            ans = "No requests";
        sendMsg(clientFd, ans);
    }
    else if (comm == "accept_request")
    {
        if (tokens.size() != 3)
        {
            sendMsg(clientFd, "Err: Wrong Syntax");
            return;
        }
        pthread_mutex_lock(&state_mutex);
        if (!session_by_fd.count(clientFd))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : Not Logged In");
            return;
        }
        string grp_id = tokens[1];
        string final_user = tokens[2];
        string user_id = session_by_fd[clientFd];

        if (!groups.count(grp_id))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No such group");
            return;
        }
        if (groupAdmin[grp_id] != user_id)
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No Admin");
            return;
        }
        if (!joinRequests[grp_id].count(final_user))
        {
            pthread_mutex_unlock(&state_mutex);
            sendMsg(clientFd, "Err : No such Request");
            return;
        }

        joinRequests[grp_id].erase(final_user);
        groups[grp_id].insert(final_user);
        pthread_mutex_unlock(&state_mutex);

        string sync_msg = "SYNC accept_request " + grp_id + " " + final_user;
        sendToPeerTracker((char *)sync_msg.c_str());

        cout << final_user << " added to group " << grp_id << endl;
        sendMsg(clientFd, "OK User added to group");
    }
    else
    {
        if (clientFd != peer)
        {
            sendMsg(clientFd, "Err: Wrong Command");
        }
    }
}