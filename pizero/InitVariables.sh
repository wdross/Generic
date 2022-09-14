#!/bin/bash

MQTT_SERVER="192.168.1.2"
MQTT_PORT="1883"
MQTT_TOPIC="homeassistant"
MQTT_DEVICENAME="mppsolar"
MQTT_USERNAME="xxxx"
MQTT_PASSWORD="xxxx"
MQTT_CLIENTID="mppsolar_92812108100014"

# Read contents of our Topics file:
mapfile -t TopicsInOrder < TopicsInParseOrder

