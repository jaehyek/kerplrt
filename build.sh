#!/bin/bash

make clean
make -s BOARD=5410
cp ./plrtest ../rootfs/rootbase/sbin/
