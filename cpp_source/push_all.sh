#!/bin/bash

DIR=obj/local/armeabi-v7a

adb root \
&& adb remount \
&& adb push $DIR/libpowertutor.so /system/lib/
