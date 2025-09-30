#ifndef TRACKER_STATE_H
#define TRACKER_STATE_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <pthread.h>
using namespace std;

struct FileInfo
{
    long long file_size;
    vector<string> piece_hashes;
};

extern map<string, string> users;
extern map<int, string> session_by_fd;
extern map<string, int> fd_by_username;
extern pthread_mutex_t state_mutex;
extern map<string, set<string>> groups;
extern map<string, string> groupAdmin;
extern map<string, set<string>> joinRequests;
extern map<string, map<string, FileInfo>> group_files;
extern map<string, map<int, set<string>>> piece_info;
extern map<string, string> peer_addresses;

#endif