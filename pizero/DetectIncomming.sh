#!/bin/bash

# when launched at boot, ensure we are in the correct folder!
cd /home/pi/Generic/pizero

. InitVariables.sh

while true; do
  if [ -f /run/lock/responses.txt ]; then
    mv /run/lock/responses.txt /run/lock/process.txt

    # Put the meat of the work into a script that can be changed after boot,
    # while this DetectIncomming.sh script continues to cycle
    . ParseIncomming.sh

    touch /run/lock/sent
    rm -q /run/lock/NOCOMM
    cat /run/lock/process.txt >> /run/lock/accumulate.txt
    rm /run/lock/process.txt
    # end of processing, if a responses.txt file was seen
  elif [ -f /run/lock/sent ]; then
    # How long has it been since we've processed a message
    let secs=`date +%s`-`date +%s -r /run/lock/sent`
    if (( $secs > 180 )); then
      # Send entry with the NO COMM state
      ./ParseIncomming.sh pub "Inverter_mode" "8" -r
      touch /run/lock/sent # don't keep hammering this
      touch /run/lock/NOCOMM
    fi
  else # create file for a reference point at boot
    touch /run/lock/sent
  fi # there was a file seen


  if [ -f /run/lock/accumulate.txt ]; then
    # Always check when we have data.  When the date rolls over, we'll
    # be pretty quick about sending it off -- won't have any actual
    # info from today in it.
    fn="/home/pi/responses"`date +%Y%m%d`".txt"
    if [ ! -f "$fn" ]; then
      mv /run/lock/accumulate.txt $fn
    fi
  fi

  sleep 2
done
