#!/bin/bash

#clear existing tunnel
ip link delete tun4

openvpn --mktun --dev tun4
ip link set tun4 up
ip addr add 192.168.0.2/24 dev tun4


echo "created interface tun4 with ip 192.168.0.2"
