#!/bin/bash

# Simple external vdcd device as bash script
#
# Created 2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
#
# This sample code is in the public domain

# connect file desciptor 3 to local port 8999
exec 3<>/dev/tcp/localhost/8999
# send vdcd external device API init message to create a light dimmer output
echo "{'message':'init','protocol':'simple','output':'light','name':'ext dimmer','uniqueid':'externalDeviceBashSample'}" >&3

# parse incoming stream for "Cx=y" channel change information
sed -n -E -e '/^C.*/s/C([0-9]+)=([0-9\.]+)/Channel \1 was changed to value \2/p' <&3