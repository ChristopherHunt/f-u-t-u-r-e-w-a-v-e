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

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

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

int main(int argc, char **argv) {
    int rtn = 0;
    Args args = {NULL};
    rtn = parse_args(&args, argc, argv);
    ASSERT_MSG(!rtn, "Arguments failed to parse");
    printf("Opening file: %s\n", args.filename);
    return 0;
}
