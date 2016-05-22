/*
 * MIDI File Stats Printer
 *
 * Prints the statistics for a MIDI file, including
 * -- Number of each MIDI command used
 * -- Min time between MIDI commands
 * -- Max time between MIDI commands
 * -- Average time between MIDI commands
 */

#include "midi_file_stats.h"

#include <iostream>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#define WRITE_STR(outstream, str)\
    ((outstream) << (str) << endl)
#define PRINT_STR(str) WRITE_STR(cout, str)
#define WRITE_DESC(outstream, description, value)\
    ((outstream) << (description) << (value) << endl)
#define WRITELN(outstream) outstream << endl
#define PRINTLN() WRITELN(cout)

using namespace std;

/*
 * Options for this program
 */
static struct option opts_long[] = {
    {"file", required_argument, 0, 'f'},
    {NULL, 0, 0, 0}
};
static const char opts_short[] = "f:";

typedef struct Args {
    char *filename;
} Args;

int parse_args(Args *args, int argc, char **argv) {
    int arg;
    int opts_ndx;
    while (-1 != (arg = getopt_long(argc, argv, opts_short, opts_long, &opts_ndx))) {
        switch (arg) {
            case 'f':
                args->filename = optarg;
        }
    }
    return EXIT_SUCCESS;
}

int write_midi_file_stats(ostream& out, MidiFile midifile) {
    WRITELN(out);
    WRITE_DESC(out, "MIDI Filename         : ", midifile.getFilename());
    WRITE_DESC(out, "Ticks Per Quarter Note: ", midifile.getTicksPerQuarterNote());
    WRITE_DESC(out, "Is Absolute Time?     : ", midifile.isAbsoluteTicks());
    WRITE_DESC(out, "Is Delta Time?        : ", midifile.isDeltaTicks());
    WRITE_DESC(out, "Number of Tracks      : ", midifile.getTrackCount());
    WRITE_DESC(out, "Has Joined Tracks?    : ", midifile.hasJoinedTracks());
    WRITE_DESC(out, "Has Split Tracks?     : ", midifile.hasSplitTracks());
    WRITELN(out);
    WRITE_STR(out, "Performing Time Analysis...");
    midifile.doTimeAnalysis();
    WRITE_STR(out, "Done with Time Analysis.");
    return EXIT_SUCCESS;
}

int write_midi_event_stats(ostream& out, MidiFile midifile) {
    return EXIT_SUCCESS;
}

int print_file_stats(MidiFile midifile) {
    return write_midi_file_stats(cout, midifile);
}

int print_event_stats(MidiFile midifile) {
    return write_midi_event_stats(cout, midifile);
}

int main(int argc, char **argv) {
    int rtn = 0;
    Args args = {NULL};

    /*
     * Parse Arguments
     */
    rtn = parse_args(&args, argc, argv);
    ASSERT_MSG(!rtn, "Arguments failed to parse");
    /*
     * Open File
     */
    MidiFile midifile;
    PRINTLN();
    printf("Opening file '%s'...\n", args.filename);
    midifile.read(args.filename);
    ASSERT_MSG(!errno, "Error reading MIDI File");
    /*
     * Print MIDI File Stats
     */
    PRINT_STR("Printing File Stats...");
    print_file_stats(midifile);

    /*
     * Print MIDI Event Stats
     */
    PRINT_STR("Printing Event Stats...");
    print_event_stats(midifile);
    return 0;
}
