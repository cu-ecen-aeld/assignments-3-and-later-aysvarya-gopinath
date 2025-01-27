//WRITER.C
/*INFO:This file takes in the file path and  write string as arguments and writes the string in the corresponding file
AUTHOR:Aysvarya Gopinath
REFERENCES:https://www.systutorials.com/docs/linux/man/3-dirname/

*/

#include<syslog.h>
#include<stdio.h>
#include<stdlib.h>                //exit
#include <unistd.h>               //write function
#include <sys/types.h>            //modes
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>              // dir/base names
#include <string.h>               

int main(int argc, char *argv[]) {

int fd,nr; // variables to get the return values of open() and write()
const char *write_file, *write_str; //command line arguments
char *dirc, *basec, *directory, *file_name; // store the parsed arugments
openlog(NULL,0,LOG_USER);   // open syslog

if(argc!=3)        // if number of arguments are not three
{
    printf("\n\rPlease enter 2 arguments: first is a FILE PATH and the second is a WRITE STRING \n ");
    syslog(LOG_ERR,"Invalid number of arguments:%d",argc);// syslog error  message 
    exit (1);  // error exit code
}
//assigning arguments
write_file =argv[1];  // file path as first argument 
write_str=argv[2];  // string to write as second argument

//extracting file and directory name
dirc = strdup(write_file);  // POSIX version of dirname and basename may modify the content of the argument
basec = strdup(write_file);  // duplicate the string
directory = dirname(dirc);  // extract the directory name 
file_name = basename(basec); //extract the file name
printf("directory=%s, filename=%s\n", directory,file_name);

// assuming that either the directory exists or is created by the caller
fd = open(write_file, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU); // read write format/if file doesnt exists its created/erases content re-existing content,all permissions for user
if (fd == -1) //returns file descriptor of the given file  
{             // file cannot be accessed 
    printf("\n\rFile not found,please enter a valid file name\n" );
    syslog(LOG_ERR,"file %s couldnot be created or found ",file_name);// syslog error  message 
    exit (1);  // error exit code
}
else{   // file is opened
nr = write (fd, write_str, strlen (write_str));  // write the contents to the file
if (nr == -1)
{
    printf("\n\r writes cannot be done");
    syslog(LOG_ERR,"Provided text %s couldnot be written ",write_str);// syslog error  message 
}
printf("\n\rwrites successful\n");
syslog(LOG_DEBUG,"Writing %s to %s",write_str,write_file);// syslog debug  message 
}

closelog();   // close syslog
}
