#!/bin/bash

# make the config message retained (-r) so it doesn't have to be
# re-broadcast every so often in the event of a Homeassistant restart
registerTopic () {
    mosquitto_pub \
        -h $MQTT_SERVER \
        -p $MQTT_PORT \
        -u "$MQTT_USERNAME" \
        -P "$MQTT_PASSWORD" \
        -i $MQTT_CLIENTID \
        -r \
        -t "$MQTT_TOPIC/sensor/"$MQTT_DEVICENAME"_$1/config" \
        -m "{
  \"name\": \""$MQTT_DEVICENAME"_$1\",
  \"unit_of_measurement\": \"$2\",
  \"state_topic\": \"$MQTT_TOPIC/sensor/"$MQTT_DEVICENAME"_$1\",
  \"unique_id\": \"${MQTT_CLIENTID}_$1\",
  \"icon\": \"mdi:$3\"
}"
} # registerTopic()

pushMQTTData () {
  mosquitto_pub \
        -h $MQTT_SERVER \
        -p $MQTT_PORT \
        -u $MQTT_USERNAME \
        -P $MQTT_PASSWORD \
        -i $MQTT_CLIENTID \
        -t $MQTT_TOPIC"/sensor/"$MQTT_DEVICENAME"_"$1 \
        -m $2
#  echo \
#        -t $MQTT_TOPIC"/sensor/"$MQTT_DEVICENAME"_"$1 \
#        -m $2 # >> /run/lock/debug
} # pushMQTTData()


### MAIN ###

if [ "$1" == "config" ]; then
  # run this script as a way to register (retained) the configuration
  # entries for the Home Assistant.  Need to run this script with parameter 'config'
  # only if the contents of TopicsInParseOrder are changed
  . InitVariables.sh

  for i in "${TopicsInOrder[@]}"
  do
    # Split our line up by double quoted strings
    eval "array=($i)"

    if [[ "${i:0:1}" == '"' && ( "${#array[@]}" = "3" || "${#array[@]}" = "4" ) ]]; then
      # we want to pass "Topic" "Units" "mdi_icon"
      registerTopic "${array[0]}" "${array[1]}" "${array[2]}"
    fi
  done

  exit
fi


### Normal code path: invoked every 30 sec to process the contents of /run/lock/process.txt
while mapfile -t -n 4 fourlines && ((${#fourlines[@]})); do
  # Line #1: QPIGS response
  eval "first=(${fourlines[0]})" # drops the quotes
  IFS=' ' read -ra SPLIT <<< "${first[0]}"

  if (( "${#SPLIT[@]}" < 17 )); then
    # Unit didn't respond to our pi or was malformed, assume offline
    # Send single entry with this state information, then loop
    pushMQTTData "Inverter_mode" "7"
  else
    for i in {0..15}; do
      eval "topics=(${TopicsInOrder[$i]})"
      pushMQTTData "${topics[0]}" "${SPLIT[$i]}"
      # Have variables we want to catch to use in later math:
      if [[ "${topics[0]}" = "SCC_voltage" ]]; then
        SCC_voltage="${SPLIT[$i]}"
      elif [[ "${topics[0]}" = "PV_in_current" ]]; then
        PV_in_current="${SPLIT[$i]}"
      elif [[ "${topics[0]}" = "Load_watt" ]]; then
        Load_watt="${SPLIT[$i]}"
      fi
    done
    # pick out 3 status we care about: Load, SCC charging and AC charging
    status="${SPLIT[16]}" # it is in the 16th 'word'
    eval "topics=(${TopicsInOrder[17]})"
    pushMQTTData "${topics[0]}" "${status:3:1}"

    eval "topics=(${TopicsInOrder[18]})"
    pushMQTTData "${topics[0]}" "${status:6:1}"

    eval "topics=(${TopicsInOrder[19]})"
    pushMQTTData "${topics[0]}" "${status:7:1}"

    # pv_input_watts = scc_voltage * pv_input_current;
    # pv_input_watthour = (float)pv_input_watts * runinterval / 3600.0;
    # load_watthour = (float)load_watt * runinterval / 3600.0;
    PV_in_watts=`echo $SCC_voltage \* $PV_in_current | bc -l`
    pushMQTTData "PV_in_watts" "${PV_in_watts}"
    PV_in_watthour=`echo $PV_in_watts \* 30.0 / 3600.0 | bc -l`
    pushMQTTData "PV_in_watthour" `printf "%0.2f" "${PV_in_watthour}"`
    Load_watthour=`echo $Load_watt \* 30.0 / 3600.0 | bc -l`
    pushMQTTData "Load_watthour" `printf "%0.2f" "${Load_watthour}"`

    # Line #2: QPIRI response
    # TopicsInOrder[26] is the first entry for this line
    # 25 entries in this line to process
    eval "second=(${fourlines[1]})" # drops the quotes
    IFS=' ' read -ra SPLIT <<< "${second[0]}"
    for i in {0..24}; do
      let line=${i}+26
      eval "topics=(${TopicsInOrder[$line]})"
      # Some items are marked as 'toss' in the TopicsInOrder list - don't use them
      if [[ "${topics[0]}" != "toss" ]]; then
        pushMQTTData "${topics[0]}" "${SPLIT[$i]}"
      fi
    done

    # Line 3: QMOD and QPIWS responses
    # TopicsInOrder[53] is the first entry for this line
    # 2 entries in this line to process
    IFS=' ' read -ra SPLIT <<< "${fourlines[2]}"
    for i in {0..1}; do
      let line=${i}+53
      eval "topics=(${TopicsInOrder[$line]})"
      pushMQTTData "${topics[0]}" "${SPLIT[$i]}"
    done

    # Line 4: Ethernet traffic and our times
    # TopicsInOrder[57] is the first entry for this line
    # 2 entries in this line to process
    IFS=' ' read -ra SPLIT <<< "${fourlines[3]}"
    for i in {0..1}; do
      let line=${i}+57
      eval "topics=(${TopicsInOrder[$line]})"
      pushMQTTData "${topics[0]}" "${SPLIT[$i]}"
    done

    # another Four lines in the TopicsInOrder will be for today's ever-increasing
    # and wholeday entries, collecting last scan Wh to tally
    todays_PV_Wh=`cat /run/lock/todays_PV_Wh.txt`
    [ -z "$todays_PV_Wh" ] && todays_PV_Wh=0 # ensure a zero for math
    todays_Load_Wh=`cat /run/lock/todays_Load_Wh.txt`
    [ -z "$todays_Load_Wh" ] && todays_Load_Wh=0 # ensure a zero for math

    # if it is a different day than what was last written, we
    # write those values to the wholeday entry and start over
    sd=`date +%Y%m%d`                               # System's Date
    fd=`date -r /run/lock/todays_PV_Wh.txt +%Y%m%d` # File's Date
    if [ "$fd" -ne "$sd" ]; then
      pushMQTTData "wholeday_PV_Wh" `printf "%0.2f" "${todays_PV_Wh}"`
      todays_PV_Wh=0
      pushMQTTData "wholeday_Load_Wh" `printf "%0.2f" "${todays_Load_Wh}"`
      todays_Load_Wh=0
    fi

    todays_PV_Wh=`bc <<< "$todays_PV_Wh"+"$PV_in_watthour"`
    pushMQTTData "todays_PV_Wh" `printf "%0.2f" "${todays_PV_Wh}"`
    echo "$todays_PV_Wh" > /run/lock/todays_PV_Wh.txt
    # by using the above system date, we ensure we can't miss a day rollover
    touch -d "$sd" /run/lock/todays_PV_Wh.txt

    todays_Load_Wh=`bc <<< "$todays_Load_Wh"+"$Load_watthour"`
    pushMQTTData "todays_Load_Wh" `printf "%0.2f" "${todays_Load_Wh}"`
    echo "$todays_Load_Wh" > /run/lock/todays_Load_Wh.txt

  fi
done < /run/lock/process.txt
