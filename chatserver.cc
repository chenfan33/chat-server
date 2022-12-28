#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <algorithm>
#include "Multicast.h"

using namespace std;

typedef struct add_info {
    string f_ip;
    int f_port;
    string b_ip;
    int b_port;
} ad;
int idx, VFLAG; // index of current server
vector<add_info> address; // stores all server info
string ORDERING("unordered");
#define BUFF_SIZE 1024
vector<sockaddr_in> server_list;
map<string, Multicast::sock_info> client_list;
char rec_buff[BUFF_SIZE];
char send_buff[BUFF_SIZE];
int sock;
struct sockaddr_in server;//current server
Multicast multicast("unordered", sock);

int sameAddr(struct sockaddr_in client1, struct sockaddr_in client2);
void broadcast(string sender_add, char* buff, int room);// sending messages
void sendClient(string msg, struct sockaddr_in a);// send one message to a
void joinRoom(struct sockaddr_in client, int room);
void parseCommand(string input, struct sockaddr_in c);
int isServer(struct sockaddr_in address);// is current server
int read_argument(int argc, char *argv[]);
int read_config(string filename);
string current_time();

int main(int argc, char ** argv){
    if(argc < 3){
        fprintf (stderr, "Chen Fan, cfan3\n");
        return EXIT_FAILURE;
    }
    // if read_argument 
    if (read_argument(argc, argv) == 1){
        return EXIT_FAILURE;
    }

    //reading ip and port from address list
    string b_ip = address.at(idx-1).b_ip;
    int port = address.at(idx-1).b_port;
    if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) <= 0){
        fprintf(stderr, "Socket Initialization Failed\n");
        exit(1);
    }

    struct sockaddr_in client;
    socklen_t add_len = sizeof(client);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(b_ip.c_str());
    server.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) != 0){
        fprintf(stderr,"Socket bind Failed\n");
        exit(1);
    }

    if (VFLAG){
        printf("Listener on port %d \n", port);  
    }
    // setting parameters for multicast 
    multicast.ordering = ORDERING;// set ordering
    multicast.sock_fd = sock;//set sock_fd
    multicast.idx = idx;//set current server index
    multicast.server_list = server_list;// set server list
    multicast.vflag = VFLAG;

    while (true){
        bzero(send_buff, BUFF_SIZE);
        int rlen = recvfrom(sock,rec_buff,sizeof(rec_buff),0,(struct sockaddr *)&client,&add_len);
        rec_buff[rlen] = '\0';

        if (VFLAG){
            printf("%s Client %d sends %s\n", current_time().c_str(),ntohs(client.sin_port), rec_buff);
        }
        // if from client, parseCommand, notify server and broadcast
        if (!isServer(client)){
            parseCommand(rec_buff, client);
        }
        // if from server then only broadcast
        else{
            string rec = rec_buff;
            multicast.deliver(rec);
        }
    }

    return EXIT_SUCCESS;
}

/**
 * changeRoom: use to handle /join, /part command
 * int join : if > 0, meaning joining a room
 *               == -1, meaning leaving a room
*/
void changeRoom(struct sockaddr_in client, int join){
    string add = inet_ntoa(client.sin_addr) + ":"s + to_string(ntohs(client.sin_port));
    map<string, Multicast::sock_info>::iterator it = client_list.find(add);
    string msg;
    // if client already in client_list, i.e in a room
    if (it != client_list.end()){
        string old_room = to_string(it->second.room);
        // if leave room
        if (join == -1){
            if (it->second.room > 0){
                it->second.room = join;
                msg = "+OK You have left chat room #" + old_room;
            }
            else{
                msg = "-ERR You did not join any room";
            }
        }
        // if join room, and c is not assigned room
        else if(join > 0){
            if(it->second.room < 0){
                it->second.room = join;
                msg = "+OK You are now in chat room #" + to_string(join);
            }
            else{
                msg = "-ERR You are already in room #"+ old_room;
            }                    
        }
    }
    else{
        // if new client join room
        if (join > 0){
            Multicast::sock_info *newClient = new Multicast::sock_info{};
            newClient->addr = client;
            newClient->room = join;
            newClient->nick = add;
            client_list.insert(pair<string, Multicast::sock_info>(add, *newClient));
            msg = "+OK You are now in chat room #" + to_string(join);
            multicast.client_list = client_list;
        }
        else{
            msg = "-ERR You did not join any room";
        }
    }
    sendClient(msg, client);
}

/**
 * joinRoom : connects client to join
 * int join: room number
*/
void joinRoom(struct sockaddr_in client, int join){
    changeRoom(client, join);
    if (VFLAG){
        printf("%s Client %d connected to room %d\n", current_time().c_str(),ntohs(client.sin_port), join);
    }
} 
/**
 * leaveRoom : leave client from its room
*/
void leaveRoom(struct sockaddr_in client){
    changeRoom(client, -1);
    if (VFLAG){
        printf("%s Client %d leave room\n", current_time().c_str(), ntohs(client.sin_port));
    }
} 

//send message to one client
void sendClient(string msg, struct sockaddr_in a){
    char *buff = &msg[0];
    sendto(sock, buff, strlen(buff), 0, (struct sockaddr *) &a, sizeof(struct sockaddr));
}

/**
 * parseCommand : parsing any message sending from client
*/
void parseCommand(string input, struct sockaddr_in client){
    // used to identify client
    string add = inet_ntoa(client.sin_addr) + ":"s + to_string(ntohs(client.sin_port));
    map<string, Multicast::sock_info>::iterator it = client_list.find(add);

    string command = input.substr(0, 5);
    // if command start with /
    if (input[0] == '/'){
        if (command == "/join"){
            if (input.length() > 6){
                int join = stoi(input.substr(6, command.length()-6));
                joinRoom(client, join);
            }
        }
        
        else if (command == "/part"){
            leaveRoom(client);
        }

        else if(command == "/nick"){
            if (input.length() > 6){
                string nick = input.substr(6, command.length()-6);
                if (it == client_list.end()){
                    string msg = "-ERR You did not join any room";
                    sendClient(msg, client);
                }
                else{
                    if (VFLAG){
                        printf("%s Client %d set nickname to %s\n", current_time().c_str(), ntohs(client.sin_port), nick.c_str());
                    }
                    it->second.nick = nick;
                }
            }
        }
        else if(command == "/quit"){
            client_list.erase(add);
            if (VFLAG){
                printf("%s Client %d disconnects\n", current_time().c_str(), ntohs(client.sin_port));
            }
            multicast.client_list = client_list;
        }
        else{
            string msg = " -ERR invalid command";
            sendClient(msg, client);
        }
    }
    
    // if message
    else{
        // if it joined a room, then send the message
        if (it->second.room > 0){
            string msg = "<"s + it->second.nick + "> " + input;
            char *buff = &msg[0];
            multicast.send(add, it->second.room, buff);
        }
        else{
            string msg = " -ERR please join a room first";
            sendClient(msg, client);
        }
    }
}

/**
 * return 1 if the same address, else 0
*/
int sameAddr(struct sockaddr_in c1, struct sockaddr_in c2){
    if(strncmp ((char *) &c1.sin_addr.s_addr, (char *) &c2.sin_addr.s_addr,
        sizeof(unsigned long)) == 0) {
        if(strncmp((char *) &c1.sin_port, (char *) &c2.sin_port, sizeof(unsigned short))
           == 0) {
            if(strncmp ((char *) &c1.sin_family, (char *) &c2.sin_family, sizeof(unsigned short)) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * return 1 if is current server, else 0
*/
int isServer(struct sockaddr_in address){
    for (auto &a : server_list){
        if(sameAddr(a, address)) {
            return 1;
        }
    }
    return 0;
}

/**
 * read the configuration file
*/
int read_config(string filename){
    ifstream input(filename);
    string line;

    while( getline(input, line )) {
        // e.g 127.0.0.1:8000,127.0.0.1:5000
        int pos = line.find(",");
        string bind;
        string forward;
        // if not , find, then this address is both bind and forward
        if (pos == string::npos){
            bind = forward = line;
        }

        else{
            forward = line.substr(0, pos);
            bind = line.substr(pos+1, line.length() - pos -1);
        }

        int f_pos = forward.find(":");
        int b_pos = bind.find(":");
        string f_ip = forward.substr(0, f_pos);
        int f_port = stoi(forward.substr(f_pos+1));
        string b_ip = bind.substr(0, b_pos);
        int b_port = stoi(bind.substr(b_pos+1));

        add_info* info = new add_info{};
        info->f_ip = f_ip;
        info->f_port = f_port;
        info->b_ip = b_ip;
        info->b_port = b_port;

        struct sockaddr_in dest;
        bzero(&dest,sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = inet_addr(f_ip.c_str());
        dest.sin_port = htons(f_port);

        server_list.push_back(dest);
        address.push_back(*info);       
    }
    input.seekg(0);
    input.close();
    return EXIT_SUCCESS;
}

/**
 * read_argument: parse the command line input
 * return 0 if success
 *        1 if failure 
*/
int read_argument(int argc, char *argv[]){
    if (argc == 1){
        fprintf (stderr, "Please provide directory path\n");
        return EXIT_FAILURE;
    }
    int c;
    int index;
    opterr = 0;

    while ((c = getopt (argc, argv, "vo:")) != -1){
        switch (c){
        case 'o':
            if (strcmp(optarg, "fifo") == 0 && strcmp(optarg, "total") == 0 && strcmp(optarg, "unordered") == 0){
                fprintf (stderr, "Unknown ordering: %s (supported: unordered, fifo, total)\n", optarg);
                return EXIT_FAILURE;
            }
            ORDERING = optarg;
            
            break;

        case 'v':
            VFLAG = 1;
            break;

        case '?':
            if (optopt == 'o')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt)){
                fprintf (stderr, "Unknown option `-%c'!\n", optopt);
            }
            else{
                fprintf (stderr, "Unknown option character `\\x%x',please use -a/-p/-v!!\n", optopt);
            }
            return EXIT_FAILURE;
        default:
            abort ();
        }
    }

    if (read_config(argv[optind]) != 0){
        return EXIT_FAILURE;
    }
    optind++;
    idx = atoi(argv[optind]);
    optind++;
    if (address.size() < idx){
        return EXIT_FAILURE;
    }

    for(; optind < argc; optind++){     
        printf("Non option arguments: %s\n", argv[optind]); 
        return EXIT_FAILURE;
    }
  return EXIT_SUCCESS;
}
/**
 * return a string with current time stamp
*/
string current_time(){
    time_t now= time(0);
    tm* now_tm= gmtime(&now);
    char t[42];
    strftime(t, 42, "%Y:%m:%d %X", now_tm);
    return t;
}