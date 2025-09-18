#ifndef TRACKER_STATE_H
#define TRACKER_STATE_H

#include <map>
#include <set>
#include <string>
#include <pthread.h>
using namespace std;

//User Manage
extern map<string, string> users; 
extern map<int, string> session_by_fd;
extern map<string, int> fd_by_username;
extern pthread_mutex_t state_mutex;

//Grp Manage
extern map<string, set<string>> groups;
extern map<string, string> groupAdmin;      
extern map<string, set<string>> joinRequests;

#endif