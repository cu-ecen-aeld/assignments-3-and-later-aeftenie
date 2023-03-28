#!/bin/bash
# param 1 = full path to file aka writefile
# param 2 = text to write to file aka writestr
writefile=$1
writestr=$2

if [ $# -ne 2 ]
then
  echo "Need to specify 2 arguments: writefile and writestr"
  exit 1
fi

mkdir -p "$(dirname $writefile)" && touch "$writefile"

if ! [ -f $writefile ]
then
  echo "Could not create file"
  exit 1
fi

echo $writestr > $writefile
