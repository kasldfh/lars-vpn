#!/bin/bash

sudo ip link delete tun3
openvpn --mktun --dev tun3
ip link set tun3 up
ip addr add 192.168.0.1/24 dev tun3

echo "created interface tun3 with ip 192.168.0.1"
