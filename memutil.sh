#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)

MODE=NONE
DATALEN=0
ADDR=-1

function Usage () {
   echo READ mode
   echo "- Usage : $0 -r read_length -a top_address" 1>&2
   echo WRITE mode
   echo "- Usage : $0 -w write_length -a top_address value" 1>&2
   echo MONITOR mode
   echo "not available now"
   echo ----
   echo read_length list: 1~
   echo write_length list: 1,2,4,8
}


if [ `whoami` != 'root' ]; then
   echo 'ERROR: root please.'
   Usage;
   exit 1;
fi

while getopts hw:r:a: OPT
do
   case $OPT in
      "w" )  MODE=WRITE;DATALEN=$OPTARG ;;
      "r" )  MODE=READ; DATALEN=$OPTARG ;;
      "m" )  exit ;;
      "h" )  Usage ; exit 1 ;;
      "a" )  ADDR=$OPTARG ;;
   esac
done
shift $((OPTIND - 1))

# args check
if [ ${ADDR} == -1 ]; then
   echo "ERROR: Address is not set!!"
   echo ----
   Usage; exit 1;
fi

# process 
if [ "${MODE}" == "READ" ]; then
   # echo READ mode
   echo "read ${DATALEN} ${ADDR} $1" > /dev/memd
   cat /dev/memd
fi

if [ "${MODE}" == "WRITE" ]; then
   # echo WRITE mode
   if [ "$1" == "" ]; then
      echo "ERROR: write data is not set!!"
      echo ------------------------------
      Usage; exit 1;
   fi
   echo "# before writing"
   echo "read ${DATALEN} ${ADDR} ${DATALEN}" > /dev/memd
   cat /dev/memd
   echo -------------------
   echo "# after writing"
   echo "write ${DATALEN} ${ADDR} $1" > /dev/memd
   echo "read ${DATALEN} ${ADDR} ${DATALEN}" > /dev/memd
   cat /dev/memd
fi

