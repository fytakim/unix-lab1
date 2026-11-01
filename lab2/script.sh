#!/bin/sh

DIR=/data
ID=$(cat /proc/sys/kernel/random/uuid)
N=0

exec 9>$DIR/lock

while true; do
    N=$((N + 1))

    flock 9
    I=1
    while [ -e $DIR/$(printf '%03d' $I) ]; do
        I=$((I + 1))
    done
    FILE=$DIR/$(printf '%03d' $I)
    echo "$ID $N" > $FILE
    flock -u 9

    sleep 1
    rm $FILE
    sleep 1
done
