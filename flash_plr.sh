#!/bin/bash
adb wait-for-device
sleep 1

echo "copying.. plrprepare_520"
adb push plrprepare /system/xbin/plrprepare

echo "copying.. plrtest_520"
adb push plrtest /system/xbin/plrtest

echo "copying.. tr"
adb push tr /system/xbin/tr

echo "changing.. permission plr* n tr"
adb shell chmod 777 /system/xbin/plr*
adb shell chmod 777 /system/xbin/tr
adb shell sync

sleep 1
reset
