#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

//mutex attempt
void* threadfunc(void* thread_param)
{
struct thread_data* thread_func_args = (struct thread_data *) thread_param; // thread_funcs_args is a pointer to the thread_param structure pointer

//SLEEP
if(usleep(thread_func_args->wait_to_obtain_ms * MS_TO_US )!=0) // Sleep for wait_to_obtain_ms
{
thread_func_args->thread_complete_success = false; //usleep not succesfull
ERROR_LOG("WAIT to obtain mutex is not successful");
}
//LOCK
if(pthread_mutex_lock(thread_func_args->mutex) !=0) // Lock the mutex
{
thread_func_args->thread_complete_success = false; //lock cannot be obtained
ERROR_LOG("LOCK cannot be obtained");
return NULL;
}
//SLEEP
if(usleep(thread_func_args->wait_to_release_ms * MS_TO_US) !=0) // Sleep for wait_to_release_ms
{
thread_func_args->thread_complete_success = false; //usleep not succesfull
ERROR_LOG("WAIT to release mutex is not successful");
}
//UNLOCK
if(pthread_mutex_unlock(thread_func_args->mutex) !=0) // Unlock the mutex
{
thread_func_args->thread_complete_success = false; //lock cannot be released
ERROR_LOG("LOCK cannot be released");
return NULL;
}
//SUCCESS
if(thread_func_args->thread_complete_success == true) //Set to true if the thread completed with success
{
DEBUG_LOG("MUTEX successfully obtained by the thread");
}
//free(thread_func_args);
//return NULL;    //freeing
return thread_param; // Return the thread_data pointer
}


//function to pass arguments to the threadfunc
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
 struct thread_data *thread_info;// object to handle structure
thread_info = (struct thread_data*) malloc(sizeof(struct thread_data));  //allocate memory for thread_data
    if (thread_info == NULL) { //memory could not be allocated
      ERROR_LOG("MALLOC FAILED");
        printf("Memory allocation failed!\n");
        return false;
    }
      //assigning arguments to pass
    thread_info->mutex = mutex;// setup mutex 
    thread_info->wait_to_obtain_ms = wait_to_obtain_ms;//setup wait arguments
    thread_info->wait_to_release_ms = wait_to_release_ms;
    thread_info->thread_complete_success = true; //Set to true if the thread completed with success by default

 if (pthread_create(thread, NULL, threadfunc, thread_info) != 0) // pass thread_info to the created thread with threadfunc() as entry point
{
    ERROR_LOG("Thread cannot be created!");
    free(thread_info);
    return false;
}
return true;
}


