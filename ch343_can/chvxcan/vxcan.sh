#!/bin/sh
sudo modprobe can_raw
sudo modprobe vxcan

if ip link show can0 > /dev/null 2>&1; then
    sudo ip link delete dev can0 type vxcan
fi

sudo ip link add dev can0 type vxcan
sudo ip link set up can0
sudo ip link set dev vxcan0 up