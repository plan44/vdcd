#!/bin/bash

# Simple external vdcd device as bash script
# makes RaspberryPi CPU temperature available as sensor value
#
# Created 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
#
# This sample code is in the public domain


# connect to external device class container
exec 3<>/dev/tcp/localhost/8999
# announce single temperature sensor device
echo "{'message':'init','group':3,'protocol':'simple','name':'RPi CPU temp.','uniqueid':'externalDeviceRPiCPUTemp','sensors':[{'sensortype':1,'usage':1,'hardwarename':'RPiCPU','min':0,'max':100,'resolution':1,'updateinterval':20}]}" >&3

# every 20 seconds, send the raspberry pi's CPU temperature
while :; do
  sleep 20
  TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
  echo "S0=$(($TEMP/1000))" >&3
done

