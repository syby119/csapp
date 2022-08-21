#!/bin/bash

#
# free-port.sh - returns an unused TCP port. 
#
PORT_START=4500
PORT_MAX=65000
port=${PORT_START}

gcc --version | grep -i ubuntu > /dev/null
is_ubuntu=$?

while [ TRUE ] 
do
  if [ "${is_ubuntu}" == "0" ]; then
    portsinuse=`netstat --numeric-ports --numeric-hosts -at | grep tcp`
  else
    portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip | grep tcp | \
      cut -c21- | cut -d':' -f2 | cut -d' ' -f1 | grep -E "[0-9]+" | uniq | tr "\n" " "`
  fi

  echo "${portsinuse}" | grep -wq "${port}"
  if [ "$?" == "0" ]; then
    if [ $port -eq ${PORT_MAX} ]
    then
      exit -1
    fi
    port=`expr ${port} + 1`
  else
    echo $port
    exit 0
  fi
done
