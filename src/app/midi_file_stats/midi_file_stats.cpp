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
#include <float.h>
#include <getopt.h>
#include <limits.h>
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

int write_file_stats(ostream& out, MidiFile midifile) {
    WRITELN(out);
    WRITE_DESC(out, "MIDI Filename         : ", midifile.getFilename());
    WRITE_DESC(out, "Ticks Per Quarter Note: ", midifile.getTicksPerQuarterNote());
    WRITE_DESC(out, "Is Absolute Time?     : ", midifile.isAbsoluteTicks());
    WRITE_DESC(out, "Is Delta Time?        : ", midifile.isDeltaTicks());
    WRITE_DESC(out, "Has Joined Tracks?    : ", midifile.hasJoinedTracks());
    WRITE_DESC(out, "Has Split Tracks?     : ", midifile.hasSplitTracks());
    WRITELN(out);
    WRITE_DESC(out, "Number of Tracks      : ", midifile.getTrackCount());
    WRITE_DESC(out, "Size (Num Tracks?)    : ", midifile.size());
    WRITELN(out);
    WRITE_STR(out, "Performing Time Analysis...");
    midifile.doTimeAnalysis();
    WRITE_STR(out, "Done with Time Analysis.");
    return EXIT_SUCCESS;
}

int write_track_stats(ostream& out, MidiFile midifile) {
    for (int i = 0; i < midifile.getTrackCount(); i++) {
        cout << "Track " << i << " Size: " << midifile[i].getSize() << endl;
    }
    return EXIT_SUCCESS;
}

int write_event_information(ostream& out, MidiEvent event) {
    return EXIT_SUCCESS;
}

int write_event_stats(ostream& out, MidiFile midifile) {
    MidiEvent event;
    for (int i = 0; i < midifile.getTrackCount(); i++) {
        cout << "Track " << i << " Size: " << midifile[i].getSize() << endl;
        int event_total_size = 0;
        int event_total_duration = 0;
        int event_num = 0;
        int size_max = INT_MIN;
        int size_min = INT_MAX;
        MidiEvent event_size_min;
        MidiEvent event_size_max;
        double duration_max = DBL_MIN;
        double duration_min = DBL_MAX;
        MidiEvent event_duration_min;
        MidiEvent event_duration_max;
        for (int j = 0; j < midifile[i].getSize(); j++) {
            midifile[i].linkNotePairs();
            int this_size = midifile[i][j].getSize();
            int this_duration = midifile[i][j].getDurationInSeconds();
            //char this_command = midifile[i][j].getCommand();
            if  (this_size > size_max) {
                size_max = this_size;
                event_size_max = midifile[i][j];
            }
            if  (this_size < size_min) {
                size_min = this_size;
                event_size_min = midifile[i][j];
            }
            if  (this_duration > duration_max) {
                duration_max = this_duration;
                event_duration_max = midifile[i][j];
            }
            if  (this_duration < duration_min) {
                duration_min = this_duration;
                event_duration_min = midifile[i][j];
            }
            event_total_size += this_size;
            event_total_duration += this_duration;
            event_num++;
        }
        cout << "\tMinimum Event Size: " << size_min << endl;
        cout << "\tMaximum Event Size: " << size_max << endl;
        cout << "\tAverage Event Size: " << (double)event_total_size / event_num << endl;
        cout << "\tMinimum Event Duration: " << duration_min << endl;
        cout << "\tMaximum Event Duration: " << duration_max << endl;
        cout << "\tAverage Event Duration: " << (double)event_total_duration / event_num << endl;
    }
    return EXIT_SUCCESS;
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
    PRINT_STR("Printing FILE Stats...");
    write_file_stats(cout, midifile);

    /*
     * Print MIDI Track Stats
     */
    PRINT_STR("Printing TRACK Stats...");
    write_track_stats(cout, midifile);

    /*
     * Print MIDI Event Stats
     */
    PRINT_STR("Printing EVENT Stats...");
    write_event_stats(cout, midifile);
    return 0;
}
