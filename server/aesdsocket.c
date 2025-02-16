//https://www.geeksforgeeks.org/signals-c-language/
//https://lloydrochester.com/post/c/unix-daemon-example/#manually-creating-a-daemon-in-c-using-a-double-fork
//https://www.w3resource.com/c-programming-exercises/file-handling/c-file-handling-exercise-3.php

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

#define PORT "9000"
#define BACKLOG 10
#define BUFFER_SIZE 1024

int sockfd;
const char *write_file = "/var/tmp/aesdsocketdata";

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void signal_handler(int signo) {
    syslog(LOG_ERR, "Caught signal %d, exiting", signo);
    remove(write_file);
    close(sockfd);
    closelog();
    exit(0);
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
     exit(EXIT_FAILURE);  //fork failed
     }
    if (pid > 0) 
     {exit(EXIT_SUCCESS);  // Parent exits
     }

    if (setsid() < 0) {
     exit(EXIT_FAILURE); //creates a new session and child is the leader
     }
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    umask(0); //file mode creation mask -default file permissions
    chdir("/"); //change to root directory
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t addr_size=sizeof (client_addr);
    char buffer[BUFFER_SIZE]= {0};
     char read_buffer[BUFFER_SIZE]={0};
     
    
    int status=0, new_fd=0;
    ssize_t bytes_received=0;
    char s[INET6_ADDRSTRLEN] = "";

    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    openlog(NULL, 0, LOG_USER);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(status));
        return -1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(servinfo);
        return -1;
    }
      
        
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Bind failed");
        close(sockfd);
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(sockfd);
        return -1;
    }

    else if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize(); //call daemon only if the lsiten is successfull
    }

    while (1) {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1) {
            syslog(LOG_ERR, "Accept failed");
            continue;  
        }

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

             FILE *fd = fopen(write_file, "a+");
    if (fd == NULL) {
         syslog(LOG_ERR, "File open failed");
          close(new_fd);
    }
    
        while ((bytes_received = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0) {
           buffer[bytes_received] = '\0';
           fwrite(buffer,1,bytes_received,fd);
           fflush(fd);
           
                
                 if (strchr(buffer, '\n'))  //if newline received 
           { 
                    fseek(fd, 0, SEEK_SET);  // Reset file pointer to read data back
               
                while ((fgets(read_buffer, BUFFER_SIZE, fd))>0) {
                size_t data_length = strlen(read_buffer);
                    send(new_fd,read_buffer, data_length, 0);
                }
            } 
        }
  
        syslog(LOG_INFO, "Closed connection to %s", s);
        close(new_fd);
    }

    close(sockfd);
    closelog();
    return 0;
}

