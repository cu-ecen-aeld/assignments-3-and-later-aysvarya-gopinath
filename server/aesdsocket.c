#define _POSIX_C_SOURCE 200112L

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
#include <sys/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include<errno.h>

#define PORT "9000"
#define BACKLOG 10
#define BUFFER_SIZE 1024

int sockfd = 0;
volatile sig_atomic_t handler_exit = 0;

struct file_lock {
    const char *write_file;
    pthread_mutex_t lock;
};

struct file_lock file_param = {
    .write_file = "/var/tmp/aesdsocketdata",
    //.lock = PTHREAD_MUTEX_INITIALIZER
};

/*struct file_lock file_param = {
    .write_file = "/var/tmp/aesdsocketdata",
    .lock = PTHREAD_MUTEX_INITIALIZER
};
*/
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER; ////
typedef struct client_node {
    pthread_t tid;
    bool thread_complete;
    int clientfd;
} client_data;

typedef struct slist_data_s {
    client_data value;
    SLIST_ENTRY(slist_data_s) entries;
} slist_data_t;

SLIST_HEAD(slisthead, slist_data_s) head = SLIST_HEAD_INITIALIZER(head);

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void signal_handler(int signo) {
    syslog(LOG_ERR, "Caught signal %d, exiting", signo);
    handler_exit = 1;
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    umask(0);
    chdir("/");
}

//timer handler triggered every 10seconds
void* timestamp_file(void *arg) {
while (!handler_exit) {
sleep(10);
    time_t t ; //variable to hold the current time
    struct tm *tmp ;  //structure pointer  to hold the local time
    char time_buffer[150]; //buffer to hold the formatted time string
    time( &t );//get the current time in UTC
    tmp = localtime( &t ); //convert to local time(system time zone)
        strftime(time_buffer, sizeof(time_buffer), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tmp);
         //  pthread_mutex_lock(&file_param.lock); //lock the file before writing /reading 
          pthread_mutex_lock(&file_lock);  
        // Open the file in append mode and write the timestamp
      FILE *fd = fd = fopen(file_param.write_file, "a+");
        if (fd != NULL) {
          //  fprintf(fd, "Timestamp:%s\n", time_buffer);  //timestamp
          syslog(LOG_INFO,"Timestamping to the file");
            fputs(time_buffer, fd);
            fflush(fd);
        fclose(fd);
        } else {
            perror("Failed to open file");
            syslog(LOG_ERR,"Cannnot open file to timestamp");
        }
        pthread_mutex_unlock(&file_lock);
       // pthread_mutex_unlock(&file_param.lock); //unlock the file
       }
        return NULL;
    }


void* fileIO(void *arg) {
    client_data *client_info = (client_data *)arg;
    int new_fd = client_info->clientfd;
    char buffer[BUFFER_SIZE];
    char read_buffer[BUFFER_SIZE];
    ssize_t bytes_received, data_length;

    //pthread_mutex_lock(&file_param.lock);
    
    while ((bytes_received = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0) {
    pthread_mutex_lock(&file_lock);//
    FILE *fd = fopen(file_param.write_file, "a+");
    if (!fd) {
        syslog(LOG_ERR, "File open failed");
        pthread_mutex_unlock(&file_lock);//
        break;//return NULL;//
    }
   
        fwrite(buffer, 1, bytes_received, fd);
        fflush(fd);  // Ensure data is written immediately
  //  }
    fclose(fd);
 
    fd = fopen(file_param.write_file, "r");
    if (!fd) {
        syslog(LOG_ERR, "File open failed");
        pthread_mutex_unlock(&file_lock);//
         break;//return NULL;
    }

    while ((data_length = fread(read_buffer, 1, BUFFER_SIZE, fd)) > 0) {
        if (send(new_fd, read_buffer, data_length, 0) == -1) {
            syslog(LOG_ERR, "Failed to send packets to client");
            break;
        }
    }
    fclose(fd);
    pthread_mutex_unlock(&file_lock);//
    client_info->thread_complete = true;
    }//
    close(new_fd);
    return NULL;
}

void cleanup() {
    slist_data_t *node;
    SLIST_FOREACH(node, &head, entries) {
        pthread_join(node->value.tid, NULL);
        SLIST_REMOVE(&head, node, slist_data_s, entries);
        free(node);
    }
    close(sockfd);
    remove(file_param.write_file);
    closelog();
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    int new_fd;
    char s[INET6_ADDRSTRLEN] = "";
    pthread_t timestamp_tid;

    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    openlog(NULL, 0, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }


     
   //  slist_data_t *thread_info = NULL;


    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) {
        syslog(LOG_ERR, "Getaddrinfo error");
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

    // Create the timestamp thread
    if (pthread_create(&timestamp_tid, NULL, timestamp_file, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timestamp thread");
        close(sockfd);
        return -1;
    }


//go to daemon mode after binding to the port
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {  //checking if -d arg is passed
        daemonize(); //call daemon function
    }
    
    // Start the timer
   // setitimer(ITIMER_REAL, &delay, NULL);


    while (!handler_exit) {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1) {
            if (errno == EINTR) continue;  // Interrupted by signal, retry
            syslog(LOG_ERR, "Accept failed");
            continue;
        }

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);

        slist_data_t *node = malloc(sizeof(slist_data_t));
        if (!node) {
            syslog(LOG_ERR, "Memory allocation failed");
            close(new_fd);
            continue;
        }

        node->value.clientfd = new_fd;
        node->value.thread_complete = false;

        if (pthread_create(&node->value.tid, NULL, fileIO, &node->value) != 0) {
            syslog(LOG_ERR, "Thread creation failed");
            free(node);
            close(new_fd);
            continue;
        }

        SLIST_INSERT_HEAD(&head, node, entries);
    }

    // Cleanup
    cleanup();
    pthread_join(timestamp_tid, NULL);  // Wait for the timestamp thread to exit

    return 0;
}
