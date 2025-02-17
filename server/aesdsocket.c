/*AESDSOCKET.C
*INFO-This file contains socket implemenation and communication from the server side.
*AUTHOR-Aysvarya Gopinath
*REFERENCES-https://www.geeksforgeeks.org/signals-c-language/
*https://www.w3resource.com/c-programming-exercises/file-handling/c-file-handling-exercise-3.php
*how to read data line by line using fgets-Chat gpt prompt
*https://lloydrochester.com/post/c/unix-daemon-example/#manually-creating-a-daemon-in-c-using-a-double-fork
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <stdio.h>
#include <signal.h> 
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT "9000"  //port to redirect 
#define BACKLOG 10   //max number of pending connections accepted
#define BUFFER_SIZE 1024

int sockfd;
const char *write_file = "/var/tmp/aesdsocketdata";  //file path to write 

//Function to get the socket address(IPV4 or IPV6)
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Signal handler function
void signal_handler(int signo) {
    syslog(LOG_ERR, "Caught signal %d, exiting", signo);
    remove(write_file); //removes the created file
    close(sockfd);   //closes the socket
    closelog();
    exit(0);
}

//Fucntion to run the application as daemon
void daemonize() {
    pid_t pid = fork();  //create a child process
    if (pid < 0) {
     exit(EXIT_FAILURE);  //fork failed
     }
    if (pid > 0) 
     {exit(EXIT_SUCCESS);  // Parent exits
     }

    if (setsid() < 0) {
     exit(EXIT_FAILURE); //creates a new session and child is the leader
     }
    freopen("/dev/null", "r", stdin);   //redirects the standard input/output/error to null directory to discard
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    umask(0); //file mode creation mask -default file permissions
    chdir("/"); //change to root directory
}

//main function
int main(int argc, char *argv[]) {
    struct addrinfo hints, *servinfo; //holds hints nad server info
    struct sockaddr_storage client_addr;     //address of the client
    socklen_t addr_size=sizeof (client_addr);
    char buffer[BUFFER_SIZE]= {0};  //buffer to receive data
     char read_buffer[BUFFER_SIZE]={0};   //buffer to send the read data
    int status=0, new_fd=0;
    ssize_t bytes_received=0;
    char s[INET6_ADDRSTRLEN] = ""; 
    memset(&hints, 0, sizeof hints);
    //socket parameters
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM; 

    openlog(NULL, 0, LOG_USER);

    signal(SIGINT, signal_handler);  //set up the signal handler
    signal(SIGTERM, signal_handler);  

//getaddrinfo provides the socket address
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));
        return -1;
    }
//create a socket for communication
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(servinfo);
        return -1;
    }
//bind the socket to an address and port number   
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Bind failed");
        close(sockfd);
        freeaddrinfo(servinfo);
        return -1;
    }
    freeaddrinfo(servinfo);
//server waits for client to make connection
    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(sockfd);
        return -1;
    }
//go to daemon mode after binding to the port
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {  //checking if -d arg is passed
        daemonize(); //call daemon function
    }
    //loop until connection is made and data is transfered without any interrupts
    while (1) {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size); //accepts connection with a requested node
        if (new_fd == -1) {  //client node
            syslog(LOG_ERR, "Accept failed");
            continue;  
        }
      //extract the IP address of the client
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

     FILE *fd = fopen(write_file, "a+"); //open the file at the specied path/ create if it doesnt exist
    if (fd == NULL) {
         syslog(LOG_ERR, "File open failed");
          close(new_fd);
    }
    //receive data from the client node
        while ((bytes_received = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0) {
           buffer[bytes_received] = '\0'; //null terminate the packets
           fwrite(buffer,1,bytes_received,fd); //block transfer of the received bytes to the file 
           fflush(fd); //flush immediately
           fseek(fd, 0, SEEK_SET);  // Reset file pointer to the begining 
           if (strstr(buffer, "\n"))  //if newline is found in the data stream
           { while ((fgets(read_buffer, BUFFER_SIZE, fd))>0) { //read the data line by line from the file
                size_t data_length = strlen(read_buffer);
                    send(new_fd,read_buffer, data_length, 0); //send data back to the client 
                }
            } 
        }
  
        syslog(LOG_INFO, "Closed connection to %s", s);
        close(new_fd);  //close the connection
    }

    close(sockfd);  //close the socket
    closelog();
    shutdown(sockfd,2); //cutoff further sends and receives
    return 0;
}

