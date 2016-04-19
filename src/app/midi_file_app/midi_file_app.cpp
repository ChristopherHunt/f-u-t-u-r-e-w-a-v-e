#include "midi_file_app.h"
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
    int track = 1;
    
    for(int i=0; i < midifile[track].size(); i++)
    {
        MidiEvent event = (MidiEvent)midifile[track][i];
        cout << "Midi event at tick " << event.tick << endl;
        printMidiPacketInfo(event);         
    }

    // Test out port midi
    Pm_Initialize();
    cout << "Number of midi devices available: " << Pm_CountDevices() << endl;

    int defaultDeviceID = Pm_GetDefaultOutputDeviceID();
    cout << "The default output device set by the configuration is : " << defaultDeviceID << endl;
    const PmDeviceInfo *deviceInfo = Pm_GetDeviceInfo(defaultDeviceID);
    cout << "opening device for writing..." << endl;
    int32_t bufferSize = 256;
    PortMidiStream** stream;
    PtError timeError = Pt_Start(1, NULL, NULL); 
    if (timeError != 0)
    {
        cout << "Error starting timer: " << timeError;
        exit(1);
    }    
    cout << "Timer started" << endl;

    PmError midiError = Pm_OpenOutput(stream, defaultDeviceID, NULL, bufferSize, NULL, NULL, 0); 
    if (midiError != 0)
    {
        cout << Pm_GetErrorText(midiError);
        exit(1);
    }

    cout << "Opened midi device successfully!" << endl;

    Pm_Terminate();
}

void printMidiPacketInfo(MidiEvent event)
{

    if(event.isMeta())
    {
        cout << "\t Is meta -  ";
        if(event.isTempo())
        {
            cout << "tempo: " << event.getTempoBPM() << "bpm" << endl;
        } 
    }

    if(event.isController())
    {
        cout << "\t Is controller" << endl;
    }

    if(event.isTimbre())
    {
        cout << "\t Is patch change" << endl;
    }

    if(event.isPressure())
    {
        cout << "\t Is Pressure message: " <<  event[1] << endl;
        
    }

    if(event.isNoteOn())
    {
        cout << "\t Is note on: " << event.getKeyNumber() << endl; 
    }

    if(event.isNoteOff())
    {
        cout << "\t Is note off: " << event.getKeyNumber() << endl;
    }
}
