#include "midifile/include/MidiFile.h"
#include "midifile/include/Options.h"
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
    Options options;
    options.process(argc, argv);

    if(options.getArgCount() != 1)
    {
        cerr << "At least one midi file is required! \n";
        exit(1);
    }

    MidiFile midifile;
    midifile.read(options.getArg(1));

    if(!midifile.status())
    {
        cerr << "Error reading midi file " << options.getArg(1) << endl;
        exit(1);
    }

    cout << "Midi file name: " << midifile.getFilename() << endl;
    cout << "Ticks per quarter note: " << midifile.getTicksPerQuarterNote() << endl;
    cout << "Is absolute time? : " << midifile.isAbsoluteTicks() << endl;
    cout << "Is delta time? : " << midifile.isDeltaTicks() << endl;
    int track = 0;
    
    for(int i=0; i < midifile[track].size(); i++)
    {
        MidiEvent event = (MidiEvent)midifile[track][i];
        cout << "Midi event at tick " << event.tick;
         
    }

}
