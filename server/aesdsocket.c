/*AESDSOCKET.C
*INFO-This file contains socket implemenation and communication from the server side.
*AUTHOR-Aysvarya Gopinath
*REFERENCES-https://www.geeksforgeeks.org/signals-c-language/
*https://www.w3resource.com/c-programming-exercises/file-handling/c-file-handling-exercise-3.php
*how to read data line by line using fgets-Chat gpt prompt
*https://lloydrochester.com/post/c/unix-daemon-example/#manually-creating-a-daemon-in-c-using-a-double-fork
*https://github.com/stockrt/queue.h/blob/master/sample.c
*https://www.geeksforgeeks.org/strftime-function-in-c/
*https://en.cppreference.com/w/c/chrono/strftime
CHAT-GPT--how to initialize HEAD without using init_slist
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
#include <sys/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define PORT "9000"
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define USE_AESD_CHAR_DEVICE (0)

#if USE_AESD_CHAR_DEVICE
	#define WRITE_FILE ( "/dev/aesdchar" ) //driver already performs locking
#else 
	#define WRITE_FILE ("/var/tmp/aesdsocketdata")  //locks can be used
#endif
int sockfd = 0;
volatile sig_atomic_t handler_exit = 0;

// structure to lock the file
struct file_lock
{
    const char *write_file; // file to read and write packets
    #if (!USE_AESD_CHAR_DEVICE )
    pthread_mutex_t lock;   // Mutex to lock/unlock the file
    #endif
};

// initailise parameters
struct file_lock file_param = {
    .write_file = WRITE_FILE, // file path to write
};

#if (!USE_AESD_CHAR_DEVICE )
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER; // initiaise lock
#endif

// structure to hold the client threads parameter
typedef struct client_node
{
    pthread_t tid;        // thread identifier
    bool thread_complete; // flag to track if the thread accomplished its work
    int clientfd;         // socket id of the client(multiple clients)
} client_data;

// slist structure
typedef struct slist_data_s
{
    client_data value; // a structure in each node
    SLIST_ENTRY(slist_data_s)
    entries;
} slist_data_t;

// initialise the head
SLIST_HEAD(slisthead, slist_data_s)
head = SLIST_HEAD_INITIALIZER(head);

// Function to get the socket address(IPV4 or IPV6)
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

// Signal handler function
void signal_handler(int signo)
{
    syslog(LOG_ERR, "Caught signal %d, exiting", signo);
    if (shutdown(sockfd, SHUT_RDWR) == -1) // try to shutdown the socket
    {
        perror("ERROR: Failed on shutdown");
        syslog(LOG_ERR, "Failed to shoutdown in signal handler");
    }
    handler_exit = 1; // set the flag to exit and remove the file
}

// Function to run the application as daemon
void daemonize()
{
    pid_t pid = fork(); // create a child process
    if (pid < 0)
    {
        exit(EXIT_FAILURE); // fork failed
    }
    if (pid > 0)
    {
        exit(EXIT_SUCCESS); // Parent exits
    }
    if (setsid() < 0)
    {
        exit(EXIT_FAILURE); // creates a new session
    }
    freopen("/dev/null", "r", stdin); // redirects the standard input/output/error to null directory to discard
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    umask(0);   // file mode creation mask -default file permissions
    chdir("/"); // change to root directory
}


// function to perform the file operations
void *fileIO(void *arg)
{
    client_data *client_info = (client_data *)arg;
    int new_fd = client_info->clientfd;
    char buffer[BUFFER_SIZE];      // Buffer to store data received from the client
    char read_buffer[BUFFER_SIZE]; // Buffer to store data read from the file
    ssize_t bytes_received, data_length;
    while ((bytes_received = recv(new_fd, buffer, BUFFER_SIZE, 0)) > 0)
    {
        #if (!USE_AESD_CHAR_DEVICE )
        pthread_mutex_lock(&file_lock);                // Lock the file before writing/reading
        #endif
        FILE *fd = fopen(file_param.write_file, "a+"); // Open the file in append mode
        if (!fd)
        {
            syslog(LOG_ERR, "File open failed");
            #if (!USE_AESD_CHAR_DEVICE )
            pthread_mutex_unlock(&file_lock);
            #endif
            break; // Exit if the file cannot be opened
        }
        buffer[bytes_received] = '\0';         // Null terminate the packet ///
        fwrite(buffer, 1, bytes_received, fd); // Write received bytes to the file
        fflush(fd);                            // Ensure data is written immediately
        if (strstr(buffer, "\n"))
        {                                           // If newline is found in the data stream
            fclose(fd);                             // Close the file after writing
            fd = fopen(file_param.write_file, "r"); // Reopen file for reading
            if (!fd)
            {
                syslog(LOG_ERR, "File open failed");
                #if (!USE_AESD_CHAR_DEVICE )
                pthread_mutex_unlock(&file_lock); // Unlock if open fails
                #endif
                break;
            }
            while (fgets(read_buffer, BUFFER_SIZE, fd) != NULL)
            {
                data_length = strlen(read_buffer);
                if (send(new_fd, read_buffer, data_length, 0) == -1)
                {
                    syslog(LOG_ERR, "Failed to send packets to client");
                    break;
                }
            }
        }
        fclose(fd);
        #if (!USE_AESD_CHAR_DEVICE )
        pthread_mutex_unlock(&file_lock);    // Unlock the file after the operations are done
        #endif
        client_info->thread_complete = true; // Indicate that the thread has completed
    }
    close(new_fd);
    return NULL; // Return from the thread function
}

// main function
int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo;    // holds hints nad server info
    struct sockaddr_storage client_addr; // address of the client
    socklen_t addr_size = sizeof(client_addr);
    int new_fd, yes = 1;
    char s[INET6_ADDRSTRLEN] = "";
    memset(&hints, 0, sizeof hints);
    slist_data_t *thread_info;
    // socket parameters
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    openlog(NULL, 0, LOG_USER); // open syslog
    // set up the signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // getaddrinfo provides the socket address
    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0)
    {
        syslog(LOG_ERR, "Getaddrinfo error");
        return -1;
    }
    // create a socket for communication
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Socket creation failed");
        freeaddrinfo(servinfo);
        return -1;
    }
    // allow port reusal
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        syslog(LOG_ERR, "Port reusal failed");
        close(sockfd);
        freeaddrinfo(servinfo);
        return -1;
    }
    // bind the socket to an address and port number
    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        syslog(LOG_ERR, "Bind failed");
        close(sockfd);
        freeaddrinfo(servinfo);
        return -1;
    }
    freeaddrinfo(servinfo);
    // server waits for client to make connection
    if (listen(sockfd, BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Listen failed");
        close(sockfd);
        return -1;
    }
    // go to daemon mode after binding to the port
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {                // checking if -d arg is passed
        daemonize(); // call daemon function
    }
   
    // loop until connection is made and data is transfered without any interrupts(signals)
    while (!handler_exit)
    {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size); // accepts connection with a requested node
        if (new_fd == -1)
        {
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "Accept failed");
            continue;
        }
        // extract the IP address of the client
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        syslog(LOG_INFO, "Accepted connection from %s", s);
        thread_info = malloc(sizeof(slist_data_t)); // Allocate memory for new node
        if (!thread_info)
        {
            syslog(LOG_ERR, "Memory allocation failed");
            close(new_fd);
            continue;
        }
        thread_info->value.clientfd = new_fd; // id of the client node
        thread_info->value.thread_complete = false;
        // Creating a new thread and pass the structure with client data
        if (pthread_create(&thread_info->value.tid, NULL, fileIO, &thread_info->value) != 0)
        {
            syslog(LOG_ERR, "Thread creation failed");
            free(thread_info);
            close(new_fd);
            continue;
        }
        SLIST_INSERT_HEAD(&head, thread_info, entries);
    }
    // cleanup
    SLIST_FOREACH(thread_info, &head, entries) // Traverse Linked list
    {
        if (thread_info->value.thread_complete == true)
        {                                               // only if  thread tasks are completed
            close(thread_info->value.clientfd);         // close the client socket
            pthread_join(thread_info->value.tid, NULL); // free the resources by joining the thread
            syslog(LOG_INFO, "Clearing all threads");
        }
    }
    while (!SLIST_EMPTY(&head))
    {
        thread_info = SLIST_FIRST(&head); // points to the head
        SLIST_REMOVE_HEAD(&head, entries);
        free(thread_info); // free pointer
        thread_info = NULL;
    } // referred to Prof Chris Choi repository for this clean up
    close(sockfd);
    #if (!USE_AESD_CHAR_DEVICE )
         remove(WRITE_FILE);
     #endif
    closelog();
    return 0;
}
