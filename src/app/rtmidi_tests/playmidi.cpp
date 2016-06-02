/*
 * playmidi.cpp
 */

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <string>
#include "RtMidi.h"
#include "midinotes.h"
#include "midimessages.h"

using namespace std;

// Platform-dependent sleep routines.
#if defined(__WINDOWS_MM__)
  #include <windows.h>
  #define SLEEP( milliseconds ) Sleep( (DWORD) milliseconds ) 
#else // Unix variants
  #include <unistd.h>
  #define SLEEP( milliseconds ) usleep( (unsigned long) (milliseconds * 1000.0) )
#endif
/*
 * opts_long
 * Allowable arguments
 */
static struct option opts_long[] = {
    {"port", required_argument, 0, 'p'},
    {NULL, 0, 0, 0}
};
static const char opts_short[] = "p:";

/*
 * struct Args
 * Arguments structure
 */
typedef struct Args {
    unsigned int port;
} Args;

/*
 * parse_args
 * Arguments parser
 */
int parse_args(Args *args, int argc, char **argv);

/*
 * open_port
 * Embed this in a try / catch block.
 */
bool open_port(RtMidi *rtmidi, unsigned int port);

static vector<unsigned char> message;
int play_note(RtMidiOut *midiout, unsigned char note, int mill_duration) {
    message[0] =NOTE_ON;
    message[1] =note;
    message[2] =127;
    midiout->sendMessage(&message);
    SLEEP(mill_duration);
    message[0] = NOTE_OFF;
    message[1] = note;
    message[2] = 64;
    midiout->sendMessage(&message);
    return EXIT_SUCCESS;
}

/* main
 */
int main(int argc, char **argv) {
    int rtn;
    Args args = {(unsigned int)-1};
    RtMidiOut *midiout = NULL;
    vector<unsigned char> notes;
    message.push_back(NOTE_OFF);
    message.push_back(C4);
    message.push_back(64);
    notes.push_back(C5);
    notes.push_back(Cs5);
    notes.push_back(D5);
    notes.push_back(Ds5);
    notes.push_back(E5);
    notes.push_back(F5);
    notes.push_back(Fs5);
    notes.push_back(G5);
    notes.push_back(Gs5);
    notes.push_back(A5);
    notes.push_back(As5);
    notes.push_back(B5);

    // Parse arguments
    rtn = parse_args(&args, argc, argv);
    if (rtn) {
        cerr << "u fucked up the args" << endl;
        return EXIT_FAILURE;
    }

    // Construct RtMidiOut object
    try {
        midiout = new RtMidiOut();
    } catch (RtMidiError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    // Call function to select port
    try {
        if (false == open_port(midiout, args.port)) {
            goto cleanup;
        }
    } catch (RtMidiError &error) {
        error.printMessage();
        goto cleanup;
    }
    // Send some MIDI shit
    cout << "Playing some sweet sweet music..." << endl;
    for (unsigned int j = 0; j < 5; j++) {
        for (unsigned int i = 0; i < notes.size(); i++) {
            play_note(midiout, notes[i], 25);
        }
        for (unsigned int i = notes.size() - 1; i > 0; i--) {
            play_note(midiout, notes[i], 25);
        }
    }
    cout << "Note on..." << endl;
    cout << "Done!..." << endl;
cleanup:
    delete midiout;
    return 0;
}
/* open_port
 */
bool open_port(RtMidi *rtmidi, unsigned int port) {
    cout << "Attempting to open port " << port << endl;

    unsigned int nPorts = rtmidi->getPortCount();
    if (nPorts - 1 < port) {
        cerr << "Only " << nPorts << " ports available. ";
        cerr << "Port index " << port << " is too high." << endl;
        return false;
    }
    rtmidi->openPort(port);
    return true;
}

int parse_args(Args *args, int argc, char **argv) {
    int arg;
    int opts_ndx;
    while (-1 != (arg = getopt_long(argc, argv, opts_short, opts_long, &opts_ndx))) {
        switch (arg) {
            case 'p':
                args->port = std::stoi(optarg, NULL, 10);
        }
    }
    return EXIT_SUCCESS;
}
