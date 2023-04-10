#!/bin/sh
# param 1 = directory aka filesdir
# param 2 = text to searsh aka searchstr
filesdir=$1
searchstr=$2

if [ $# -ne 2 ]
then
  echo "Need to specify 2 arguments: filesdir and searchstr"
  exit 1
fi

if ! [ -d $filesdir ]
then
  echo "$filesdir is not a directory"
  exit 1
fi

count_total=$(find $filesdir/* 2>/dev/null | wc -l)
count_found=$(find $filesdir/* "*$searchstr*" 2>/dev/null | wc -l)
echo "The number of files are $count_total and the number of matching lines are $count_found"
