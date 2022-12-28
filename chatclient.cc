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

using namespace std;
string delimiter = ":";
char rec_buff[1024];
char send_buff[1024];
int MAX_LENGTH = 1024;
int sock;
struct sockaddr_in dest;
void intHandler(int dummy); // signal handling

int main(int argc, char ** argv){
    if(argc != 2){
        printf("Missing port number\n");
        return EXIT_FAILURE;
    }

    string arg = argv[1];
    
    string ip = arg.substr(0, arg.find(delimiter));
    int port = stoi(arg.substr(arg.find(delimiter)+1));

 
	/* Socket settings */
	sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0){
        fprintf(stderr, "Socket Initialization Failed\n");
        exit(1);
    }
    signal(SIGINT, intHandler);


    fd_set master_fds;
    fd_set readfds;
    struct timeval tv;
    int numfd, activity;
    int bytes_recieved;

    FD_ZERO(&master_fds);
    FD_ZERO(&readfds);

      // add our descriptors to the set (0 - stands for STDIN)
    FD_SET(sock,&master_fds);
    FD_SET(0,&master_fds);
    numfd = sock;


    bzero(&dest,sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip.c_str());
    dest.sin_port = htons(port);

    for(;;){
        readfds = master_fds;
        activity = select(numfd+1, &readfds, NULL, NULL, NULL);
        if ((activity < 0) && (errno!=EINTR)){
            perror("select error"); 
        }

        else{
            if (FD_ISSET(sock, &readfds)){
                FD_CLR(sock, &readfds);//clear the set
                bzero(rec_buff, 1024);
                bytes_recieved = recvfrom(sock,rec_buff,sizeof(rec_buff),0,(struct sockaddr *)&dest,(socklen_t*) sizeof(dest));
                rec_buff[bytes_recieved]= '\0';
                string rec = rec_buff;
                printf("%s\n",rec_buff);
            }
            else{
                bzero(send_buff, MAX_LENGTH);
                fgets (send_buff, MAX_LENGTH, stdin);
                for(int j=sock; j<=numfd; j++){
                    if (FD_ISSET(j, &master_fds)) {
                        if ((strlen(send_buff)>0) && (send_buff[strlen (send_buff) - 1] == '\n')){ 
                            send_buff[strlen (send_buff) - 1] = '\0';
                        }
                        
                        if ((strcmp(send_buff , "/quit") == 0)){ 
                            sendto(sock, send_buff, strlen(send_buff), 0, (struct sockaddr *)&dest, sizeof(struct sockaddr));
                            close(sock);
                            return EXIT_SUCCESS;
                        }

                        else{
                            sendto(sock, send_buff, strlen(send_buff), 0, (struct sockaddr *)&dest, sizeof(struct sockaddr));
                        }
                    }
                }
            }
        }
    }
    close(sock);
    return EXIT_SUCCESS;
}

void intHandler(int dummy) {
    bzero(send_buff, MAX_LENGTH);
    strcpy(send_buff, "/quit");
    sendto(sock, send_buff, strlen(send_buff), 0, (struct sockaddr *)&dest, sizeof(struct sockaddr));
    close(sock);
    exit(0);
}