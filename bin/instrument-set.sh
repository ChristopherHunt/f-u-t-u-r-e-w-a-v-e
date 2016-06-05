#! /bin/bash

if [ "$#" != 1 ]
then
        echo "usage: ${0} <num>" 1>&2
        echo
        echo "Please select from the following instruments: "
        aconnect -o
        exit 22
fi

NUM_MIDI_THRU=$(aconnect -i | egrep "client [0-9]*: 'Midi Through'*" | awk -F' ' '{print $2}' | awk -F':' '{print $1}')
NUM_INSTRUMENT=${1}

aconnect -x
aconnect ${NUM_MIDI_THRU} ${NUM_INSTRUMENT} || echo "Did not set instrument" 1>&2
echo "Successfully set instrument to " $(aconnect -o | grep "client ${NUM_INSTRUMENT}")
