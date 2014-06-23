#!/bin/sh

if [ -z "$1" ]; then
  /bin/echo "$0 <command line>"
  exit 1
fi

if [ -f /bin/awk ]; then
  AWK=/bin/awk
else
  AWK=/usr/bin/awk
fi

if [ -f /bin/grep ]; then
  GREP=/bin/grep
else
  GREP=/usr/bin/grep
fi

if [ -f /bin/expr ]; then
  EXPR=/bin/expr
else
  EXPR=/usr/bin/expr
fi

if [ -f /bin/sed ]; then
  SED=/bin/sed
else
  SED=/usr/bin/sed
fi

program=`/bin/echo $1 | $AWK -F '/' '{print $NF;}'`
grep_cmd="$GREP -w $program"

list='2 3 4 5 6 7 8 9'
for i in $list; do
  eval p='$'$i
  if [ -z "$p" ]; then
    break
  fi
  #first_ch=`$EXPR substr "$p" 1 1`
  first_ch=`/bin/echo "$p" | $SED -e 's/\(.\).*/\1/'`
  if [ "$first_ch" = "-" ]; then
      p="'\\$p'"
  fi
  grep_cmd="$grep_cmd | $GREP -w $p"
done

cmd="/bin/ps auxww | $grep_cmd | $GREP -v grep | $GREP -v $0 | $AWK '{print \$2;}'"
pids=`/bin/sh -c "$cmd"`
if [ ! -z "$pids" ]; then
  i=0
  count=0
  /bin/echo "stopping $program ..."
  while [ 1 -eq 1 ]; do
    new_pids=''
    for pid in $pids; do
        if [ $i -eq 0 ]; then
           /bin/kill $pid
        else
           /bin/kill $pid >/dev/null 2>&1
        fi

    	if [ $? -eq 0 ]; then
           new_pids="$new_pids $pid"
    	fi
        count=`$EXPR $count + 1`
    done

    if [ -z "$new_pids" ]; then
      break
    fi

    pids="$new_pids"
    /usr/bin/printf .
    /bin/sleep 1
    i=`$EXPR $i + 1`
  done
fi

/bin/echo ""
cmd="/bin/ps auxww | $grep_cmd | $GREP -v grep | $GREP -v $0 | /usr/bin/wc -l"
count=`/bin/sh -c "$cmd"`
if [ $count -eq 0 ]; then
   exit 0
else
  cmd="/bin/ps auxww | $grep_cmd | $GREP -v grep | $GREP -v $0"
  /bin/sh -c "$cmd"
  /bin/echo "already running $program count: $count, stop fail!"
  exit 16
fi

