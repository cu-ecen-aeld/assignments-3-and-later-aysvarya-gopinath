#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
int status=system(cmd); // makes C code to run shell commands
if(status!=0)//returns a non zero for error status
   return false;
else 
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...) {
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;

    // Build the command array
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    const char* full_path = command[0];  // Extract the full path
    if (full_path[0] != '/') {// Check if it's an absolute path
        return false;  
    }
else{ // if absolute path is given 
    int pid = fork();  // Create a new process
    if (pid == -1) {
        return false;  // Error in forking
    }
    if (pid == 0) {  // Child process
        execv(full_path, command);  // Attempt to execute the command
        perror("execv failed"); // \execv fails, return false
        exit(-1); 
    }
 // Parent process waits for the child to finish
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;  // Error waiting for child
    }
    if (WIFEXITED(status)) { // checking childs exit status
        return WEXITSTATUS(status)==0;  //  true if the child exited successfully
    }
    return false;  // false if the child process didn't exit normally
}

}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...) {
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;  // Null-terminate the command array

    const char *full_path = command[0]; // Extract the full path
    if (full_path[0] != '/') {
        return false;  // Only accept absolute paths
    }
    int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644); //opens the output file
    if (fd < 0) {                                 //if not available it creates
        perror("open");
        va_end(args);
        return false; // error opening file
    }
   int pid = fork(); // fork the child process
   switch (pid) { 
    case -1: {
        perror("child cannot be forked");  //child not created
        close(fd);
        va_end(args);
        return false;
    }

    case 0: {  // if pid is 0 then Child process
        if (dup2(fd,1) < 0) {  // Redirect stdout to the file (stdout fd is 1)
            perror("dup2");
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
        execv(full_path, command);  // Execute the command
        perror("execv");            // Only reaches here if execv fails
        exit(EXIT_FAILURE);
    }
default:{
    // Parent process
    close(fd);
    int status;
    if (waitpid(pid, &status, 0) == -1) { // check the chils exit status
        perror("waitpid");
        va_end(args);
        return false;
   } 
    va_end(args);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}}


