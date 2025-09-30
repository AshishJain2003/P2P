#include "state.h"

using namespace std;

map<string, string> users;
map<string, set<string>> groups;
map<int, string> session_by_fd;
map<string, int> fd_by_username;
map<string, string> groupAdmin;
map<string, set<string>> joinRequests;
map<string, map<string, FileInfo>> group_files;
map<string, map<int, set<string>>> piece_info;
map<string, string> peer_addresses;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;