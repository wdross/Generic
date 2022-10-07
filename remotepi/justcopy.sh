#!/bin/bash

# We'll create time-relative file at reboot/start of script
# This may or may not be the same time the router was last rebooted,
# but will let the script not fail for lack of existence
touch /run/justcopy_AskedForReboot
touch /run/justcopy_ScriptStarted

while true; do
  if [ -f /run/response.txt ]; then
    touch /run/sent
    if mountpoint -q /home/pi/backhome; then

      # folder is mounted OK, append our file over there
      sudo mv -f /run/response.txt /run/moving.txt
      sudo cat /run/moving.txt >> /home/pi/backhome/responses.txt
      sudo rm /run/moving.txt
    else
      online=`ping -c 3 192.168.1.7 | grep "3 received"`
      if [[ "$online" == "" ]]; then
        # not hearing from pizero thru our VPN, check the internet

        online=`ping -c 3 waynes.boxathome.net | grep "3 received"`
        if [[ "$online" == "" ]]; then

          # sent 3 pings, didn't get back exactly 3, so probably offline

          # compute how many seconds its been since last reboot of AT&T
          secs=`echo $(( $(date +%s) - $(date -r /run/justcopy_AskedForReboot +%s) ))`
          if (( $secs > 1800 )); then
            # its been at least a half hour, reboot AT&T home connect
            touch /run/justcopy_AskedForReboot
            . ./Reboot.sh
          #else less than 1/2 hr, we do not do a thing, because we do not want to reboot too often
          fi
        else
          # Internet seems connected, we do not do a thing, because we do not want to reboot too often
          touch /run/justcopy_sees_only_internet
        fi
      else
        # we got all pings back, seems to be online, just ask to be mounted again
        sudo mount -av /home/pi/backhome
        sleep 5 # additional time to allow mount to process in background
      fi
    fi
  elif [ -f  /run/sent ]; then
    # we know when we sent out last packet, see if it's been way too long
    secs=`echo $(( $(date +%s) - $(date -r /run/sent +%s) ))`
    if (( secs > 1000 )); then
      touch /run/sent
      touch /run/justcopy_restarted_docker
      pushd /opt/ha-inverter-mqtt-agent/
      docker-compose restart
      popd
    fi
  else
    # we don't have this file, so must have been a recent restart, make it
    touch /run/sent
  fi
  sleep 3
done
