#!/bin/bash -e
# Collect CPU usage and refresh rate data from instrumentation.log to csv

time=0
ucpu=0
kcpu=0
ffps=0
cfps=0

echo "time,user,kernel,playback,controller"
while read line; do
	set -- $line
	[[ "$line" == "" && "$ffps" != 0 && "$cfps" != 0 ]] && echo "$time,$ucpu,$kcpu,$ffps,$cfps" && continue
	[[ "$1" == @ ]] && time=$2 && continue
	[[ "$2" == User ]] && ucpu=$1 && continue
	[[ "$2" == Kernel ]] && kcpu=$1 && continue
	[[ "$2" == Data && "$7" == rate ]] && ffps=$1 && continue
	[[ "$2" == TCPLinky && "$4" == rate ]] && cfps=$1 && continue
	[[ "$2" == FileIO ]] && ffps=$1 && continue
	[[ "$2" == Controller ]] && cfps=$1 && continue
done
exit 0
