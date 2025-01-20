#!/bin/bash
#FILE : writer.sh
#This file  requires the file path and write string as arguments.It creates the file path if not existing and enters the write string in the file 
#AUTHOR:Aysvarya Gopinath
#REFERENCES:https://unix.stackexchange.com/questions/30127/whats-the-quickest-way-to-add-text-to-a-file-from-the-command-line
#https://stackoverflow.com/questions/6121091/how-to-extract-directory-path-from-file-path

WRITEFILE=$1   #file path
WRITESTR=$2  #text to write
DIRECTORY="$(dirname "${WRITEFILE}")"	 # return directory portion of a pathname
FILE="$(basename "${WRITEFILE}")" # returns non-directory portion of a pathname
	
if [ $# -lt 2 ]  #if total number of arguments is not atleast 2
then
  echo "Please enter 2 arguments: first is a FILE   PATH and the second is a WRITE STRING "
  exit 1       
fi

if [ ! -d $WRITEFILE ]	#if directory doesnot exists	
then 
  mkdir -p $DIRECTORY	#create the directory
fi
if [ ! -f "$FILE" ] #file doesn't exits
then
   cd $DIRECTORY    # change to that directory
   touch "$FILE"    #create the file 
   echo "$WRITESTR" >"$FILE"   # write to the new file
   
elif [ -f "$FILE" ] #if file exists
then
 echo "$WRITESTR"  >> "$FILE"  #overwrite the file
 
else # file cannot be created at the given file path
  echo "File cannot be created" 
  exit 1
fi


