#ifndef MULTICAST_H
#define MULTICAST_H

#include <map>
#include <string>
#include <sys/socket.h>
#include <vector>
#include <iostream>
#include <set>
using namespace std;

class Multicast {
public:
// vars related to server, etc..
  int vflag;
  int sock_fd;
  string ordering;
  int idx;
  struct sock_info {
    sockaddr_in addr;
    int room; 
    string nick;
  } si;
  vector<sockaddr_in> server_list;
  map<string, sock_info> client_list;

  // fifo ordering variables
  map<string, int> S;
  map<string, map<int, int>> R;
  map<string, map<int, string>> holdback_f;

  // total ordering variables
  struct msg_info {
    string msg;
    bool deliverable;
    int groupID;
    int nodeID;
    int seqID;
  } ms;
  map<string, msg_info> holdback_t;
  map<string, int> counter;
  int P_g[10] = {0}; //Highest sequence number it has proposed to group g so far
  pair<int, int> A_g[10] = {make_pair(0,0)};//Highest 'agreed' sequence number it has seen for group g
  // Comparator function for map holdback_t
  struct comp {
    template <typename T>
    bool operator()(const T& l, const T& r) const {
      if (l.second.seqID != r.second.seqID) {
        return l.second.seqID < r.second.seqID;
      }
      return l.second.nodeID < r.second.nodeID;
    }
  }t;

public:
  Multicast(string ordering, int sock_fd);
  void notify_clients(string msg, int groupID);
  void notify_servers(string msg);

  string send_fifo(string clientID, int groupID, string m);
  void propose(string m, int groupID, string clientID);
  void send_total(string msg, int groupID, string clientID);
  int send(string clientID, int groupID, string m);

  void t_deliver(string m);
  void f_deliver(string m);
  int deliver(string m);

  void sort(map<string, msg_info>& M);
  string current_time();
};

#endif
