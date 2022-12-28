#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Multicast.h"
#include <iostream>
#include <set>
#include <sys/time.h>

using namespace std;

/*read and parse one request*/
Multicast::Multicast(string ordering, int sock_fd) {
  this->ordering = ordering;
  this->sock_fd = sock_fd;
}


/***
 * sending msg to groupID(room)
*/
void Multicast::notify_clients(string msg, int groupID){
   int pos = msg.find('/');
   msg = msg.substr(pos+1);
   pos = msg.find('/');
   msg = msg.substr(pos+1);
   pos = msg.find('/');
   msg = msg.substr(pos+1);
   char *buff = &msg[0];
   for (auto &c : client_list){
      if (groupID == c.second.room){
         if (vflag){
            printf("%s Sending %s to client %d\n", current_time().c_str(),buff , ntohs(c.second.addr.sin_port));
         }
         sendto(sock_fd, buff, strlen(buff), 0, (struct sockaddr *) &c.second.addr, sizeof(struct sockaddr));
      }
   }
}

/**
 * sending msg to other servers in server_list
*/
void Multicast::notify_servers(string msg){
   char *buff = &msg[0];
   int index = 0;
   for (auto &a : server_list){
      index++;
      // if current server
      if(index == idx) {
         continue;
      }
      if (vflag){
         printf("%s Sending %s to server %d\n", current_time().c_str(),buff , ntohs(a.sin_port));
      }
      bind(sock_fd, (struct sockaddr*)&a, sizeof(a));
      sendto(sock_fd, buff, strlen(buff), 0, (struct sockaddr *) &a, sizeof(struct sockaddr));
   }
}

/**
 * FIFO  send
 * process the message m, parse it into message with information needed for fifo 
 * return message in the form of : clientID/msgID/groupID/content
 * where clientID = ip+port
 *       msgID = int ...
*/
string Multicast::send_fifo(string clientID, int groupID, string m){
   std::map<string, int>::iterator it =  S.find(clientID);
   int msgID = 1;
   if (it != S.end()){
      msgID = S.find(clientID)->second + 1;
      S.find(clientID)->second = msgID;
   }
   else{
      S.insert(pair<string, int>(clientID, msgID));
   }
   m = clientID + "/" + to_string(msgID) + "/" + to_string(groupID) + "/" + m;
   return m;
}

/**
 * Total send
 * propose : send message to other servers for proposals
 * message in the form of : groupID/idx/msgID/content
 * idx : (current server index)
 * msgID : ip+port+timestamp
 * 
*/
void Multicast::propose(string m, int groupID, string clientID){
   struct timeval tv;
   gettimeofday(&tv, NULL);
   string time = to_string(tv.tv_usec); 
   string msgID = clientID + time;
   m = to_string(groupID) + "/" + to_string(idx) + "/" + msgID + "/" + m;

   // get the max sequence number to propose
   P_g[groupID] = max(P_g[groupID], A_g[groupID].first)+1;
   // update A_g, highest sequence num it has seen
   if (P_g[groupID] >  A_g[groupID].first){
      A_g[groupID] = make_pair(P_g[groupID], idx);
   }
   notify_servers(m);
   // counter used to count number of proposal it receives include from itself
   counter.insert(pair<string, int>(msgID, 1));
}

/**
 * send_total : first send to its client, then notify other servers
*/
void Multicast::send_total(string msg, int groupID, string clientID){
   notify_clients(msg, groupID);
   propose(msg, groupID, clientID);
}

/**
 * send : do corresponding send according to ordering
*/
int Multicast::send(string clientID, int groupID, string m){
   if (ordering.compare("total") == 0){
      send_total(m, groupID, clientID);
   }
   else if (ordering.compare("fifo") == 0){
      string ms = send_fifo(clientID, groupID, m);
      notify_clients(ms, groupID);
      notify_servers(ms);
   }
   else{
      notify_clients(m, groupID);
      m = to_string(groupID) + "/" + m;
      notify_servers(m);
   }
   return 1;
}

/**
 * Total deliver : deliver message in 3 cases
 * message starts with / : a propose message
 * in the form of : groupID/idx/msgID/content
 * message starts with a : agreed sequence number 
 * in the form of : amsgID/sequence_num/nodeID/groupID
 * others : message that request proposal
 * in the form of : groupID/nodeID/msgID/content
*/
void Multicast::t_deliver(string m){
   // receiving a propose 
   if (m.at(0) == '/'){
      
      m = m.substr(1);
      int pos = m.find("/");
      string msgID = m.substr(0, pos);
      m = m.substr(pos+1);
      pos = m.find("/");
      int p_num = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find("/");
      int nodeID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find("/");
      int groupID = stoi(m.substr(0, pos));

      // increasing the number of proposals it receives
      counter.find(msgID)->second = counter.find(msgID)->second + 1;
      int count = counter.find(msgID)->second;
      // if higher, then update its A_g
      if (p_num > A_g[groupID].first){
         A_g[groupID] = make_pair(p_num, nodeID);
      }
      // if same, then compare nodeID
      else if (p_num == A_g[groupID].first){
         if (nodeID > A_g[groupID].second){
            A_g[groupID] = make_pair(p_num, nodeID);
         }
      }
      if (vflag){
            printf("%s Receiving sequence number from %d total\n", current_time().c_str(),nodeID);
      }
      // if receive from all servers, sending agreed message
      if (count == server_list.size()){
         string sending = "a" + msgID  + "/" + to_string(A_g[groupID].first) + "/" + to_string(A_g[groupID].second) + "/" + to_string(groupID);
         if (vflag){
            printf("%s Sending agreed sequence number for %s\n", current_time().c_str(),msgID.c_str());
         }
         notify_servers(sending);
      }

   }
   // receiving agree
   else if (m.at(0) == 'a'){
      m = m.substr(1);
      int pos = m.find("/");
      string msgID = m.substr(0, pos);
      m = m.substr(pos+1);
      pos = m.find("/");
      int seqID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find("/");
      int nodeID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find("/");
      int groupID = stoi(m.substr(0, pos));
      // if receiving agreed sequence number, make it deliverable
      holdback_t.find(msgID)->second.deliverable = true;
      // check the queue to see if there's any to deliver
      sort(holdback_t);
      if (vflag){
         printf("%s Receiving agreed sequence number for %s\n", current_time().c_str(),msgID.c_str());
      }
   }
   // receiving message that request proposal
   else{
      int pos = m.find('/');
      int groupID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find('/');
      int nodeID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      pos = m.find('/');
      string msgID = m.substr(0, pos);
      m = m.substr(pos+1);
      int p_new = max(P_g[groupID], A_g[groupID].first)+1;
      P_g[groupID] = p_new;
      msg_info *msg = new msg_info{};
      msg->deliverable = false;
      msg->groupID = groupID;
      msg->msg = m;
      msg->nodeID = idx;
      msg->seqID = p_new;
      // add message to holdback queue
      holdback_t.insert(pair<string, msg_info>(msgID, *msg));

      // send back with proposal /msgID/propose_num/nodeID/groupID
      m = "/" + msgID  + "/" + to_string(p_new) + "/" + to_string(idx) + "/" + to_string(groupID);
      char *buff = &m[0];
      if (vflag){
         printf("%s Propose sequence number for %s\n", current_time().c_str(),msgID.c_str());
      }
      sockaddr_in a =  server_list.at(nodeID-1);
      bind(sock_fd, (struct sockaddr*)&a, sizeof(a));
      sendto(sock_fd, buff, strlen(buff), 0, (struct sockaddr *) &a, sizeof(struct sockaddr));
   }
}

/**
 * Fifo : deliver
 * parse m, m in the form of : clientID/msgID/groupID/content
*/
void Multicast::f_deliver(string m){
   if (vflag){
      printf("%s Receiving %s from another server\n", current_time().c_str(),m.c_str());
   }
   int pos = m.find('/');
   string clientID = m.substr(0, pos);
   m = m.substr(pos+1);
   pos = m.find('/');
   int msgID = stoi(m.substr(0, pos));
   m = m.substr(pos+1);
   pos = m.find('/');
   int groupID = stoi(m.substr(0, pos));
   m = m.substr(pos+1);

   map<string, map<int, string>>::iterator it = holdback_f.find(clientID);
   //if clientID does not exists
   if (it == holdback_f.end()){
      map<int, string> temp;
      temp.insert(pair<int, string>(msgID, m));
      //add one with msgID to queue
      holdback_f.insert(pair<string, map<int, string>>(clientID, temp));
   }
   else{
      it->second.insert(pair<int, string>(msgID, m));
   }

   int nextID;
   // find by client
   map<string, map<int, int>>::iterator it_2 =  R.find(clientID);
   if (it_2 == R.end()){
      nextID = 1;
      map<int, int> temp;
      temp.insert(pair<int, int>(groupID,nextID));
      // insert client into R
      R.insert(pair<string, map<int, int>>(clientID, temp));
   }
   else{
      // find by group
      map<int, int>::iterator it_3 = it_2->second.find(groupID);
      if (it_3 == it_2->second.end()){
         nextID = 1;
         // insert group into client map
         it_2->second.insert(pair<int, int>(groupID,nextID)); 
      }
      else{
         nextID = it_3->second;
      }
   }
   while(holdback_f.find(clientID)->second.find(nextID) != holdback_f.find(clientID)->second.end()){
      string prev_m = holdback_f.find(clientID)->second.find(nextID)->second;
      holdback_f.find(clientID)->second.erase(nextID);
      notify_clients(prev_m, groupID);
      nextID++;
      R.find(clientID)->second.find(groupID)->second = nextID;

   }
}

/**
 * deliver message according to ordering
*/
int Multicast::deliver(string m){
   if (ordering.compare("total") == 0){
      t_deliver(m);
   }

   else if (ordering.compare("fifo") == 0){
      f_deliver(m);
   }
   else{
      int pos = m.find("/");
      int groupID = stoi(m.substr(0, pos));
      m = m.substr(pos+1);
      notify_clients(m, groupID);
   }
   return 1;

}

/**
 * traverse the map in order, 
 * and deliver if the lowest is deliverable, else break and wait
*/
void Multicast::sort(map<string, msg_info>& M){
    set<pair<string, msg_info>, comp> S(M.begin(), M.end());
    for (auto& it : S) {
      if (it.second.deliverable){
         notify_clients(it.second.msg, it.second.groupID);
         M.erase(it.first);
      }
      // if has hanging message with lower sequence number, then break
      else{
         break;
      }
    }
}

string Multicast::current_time(){
    time_t now= time(0);
    tm* now_tm= gmtime(&now);
    char t[42];
    strftime(t, 42, "%Y:%m:%d %X", now_tm);
    return t;
}