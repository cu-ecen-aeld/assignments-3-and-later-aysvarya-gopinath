#!/bin/bash
#FILE :finder.sh
#This file requires the directory path and search string as arguments. It returns the number of files in the direcory and the number of lines matching with the serach text.
#AUTHOR:Aysvarya Gopinath
#REFERENCES:https://www.geeksforgeeks.org/grep-command-in-unixlinux/
#https://stackoverflow.com/questions/20895290/count-number-of-files-within-a-directory-in-linux

WRITEDIR=$1   #directory path
SEARCHSTR=$2  #text to search
if [ $# -lt 2 ]  #if total number of arguments is not atleast be 2
then
   echo "Please enter 2 arguments: first is a FILE  DIRECTORY and the second is a SEARCH STRING "
   exit 1       
elif [ -d $WRITEDIR ] #if directory exists
then
  X=$( ls -1 "$WRITEDIR" | wc -l )   # count the number of files in directory 
  Y=$( grep -r "$SEARCHSTR" "$WRITEDIR" | wc -l ) #count the number of lines matching with the search text in all files #Z=$( grep -rc "$SEARCHSTR" "$WRITEDIR" ) 
  echo "The number of files are ${X} and the number of matching lines are ${Y}"
else   #directory does not exits
  echo " Invalid, enter an existing directory"
  exit 1
fi



