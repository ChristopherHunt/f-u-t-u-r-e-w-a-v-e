#ifndef LINUX_MIDI_TEST_H
#define LINUX_MIDI_TEST_H
#include "midifile/include/MidiFile.h"
#include "midifile/include/Options.h"
//#include "portmidi/include/portmidi.h"
//#include "portmidi/include/porttime.h"
#include <porttime.h>
#include <portmidi.h>

#include "debug.h"
void printMidiPacketInfo(MidiEvent event);
#endif /* LINUX_MIDI_TEST_H */
