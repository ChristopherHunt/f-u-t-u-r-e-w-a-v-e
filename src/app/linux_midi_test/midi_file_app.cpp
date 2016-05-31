#include "midi_file_app.h"
#include <iostream>

#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)

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
    DEBUG_MSG("READING FILE");
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
    int track = 5;
    DEBUG_MSG("DISPLAYING MIDI FILE");
    fprintf(stderr, "track: %d\n", track);
    fprintf(stderr, "midifile.size(): %d\n", midifile.size());
    for(int i=0; i < midifile[track].size(); i++)
    {
        MidiEvent event = (MidiEvent)midifile[track][i];
        cout << "Midi event at tick " << event.tick << endl;
        printMidiPacketInfo(event);
    }

    // Test out port midi
    int pmErrInit = Pm_Initialize();
    cout << "Number of midi devices available: " << Pm_CountDevices() << endl;

    int defaultDeviceID = Pm_GetDefaultOutputDeviceID();
    cout << "The default output device set by the configuration is : " << defaultDeviceID << endl;
    const PmDeviceInfo *deviceInfo = Pm_GetDeviceInfo(defaultDeviceID);
    cout << "opening device for writing..." << endl;
    int32_t bufferSize = 1024;

    DEBUG_MSG("NEW PortMidiStream");
    PortMidiStream** stream = new PortMidiStream*;
    PtError timeError = Pt_Start(1, NULL, NULL); 
    if (timeError != 0)
    {
        cout << "Error starting timer: " << timeError;
        exit(1);
    }
    cout << "Timer started" << endl;

    DEBUG_MSG("Pm_OpenOutput");
    PmError midiError = Pm_OpenOutput(stream, defaultDeviceID, NULL, 256,
                          (PmTimestamp (*)(void *))&Pt_Time, NULL, 100);

    if (midiError != 0)
    {
        cout << Pm_GetErrorText(midiError);
        exit(1);
    }

    cerr << "Opened midi device successfully!" << endl;

    int time = 0;
    for(int i=0; i < midifile[1].size(); i++)
    {
        MidiEvent event = (MidiEvent)midifile[track][i];
        //Convert a MidiEvent into a Portmidi message
        if (event.isNoteOn() || event.isNoteOff())
        {
            DEBUG_MSG("NOTE ON/OFF EVENT");
            //Copy midi data into a PmEvent buffer
            PmMessage message = Pm_Message(event[0], event[1], event[2]);
            cout << dec << event.tick;
            cout << '\t' << hex;
            cout << (int)event[0] << ' ' << (int)event[1] << ' ' << (int)event[2] << endl;
            // Events wrap messages and timestamps
            // This timestamp tells portmidi when to send stuff from the buffer, 0 means immediately
            // Different than any timing stuff in the packet itself
            PmEvent *pmEvent = new PmEvent;
            cout << "Ticks per quarter note: " << dec << midifile.getTicksPerQuarterNote() << endl;
            cout << "Playing at time: " << midifile.getTimeInSeconds(event.tick) * 1000.0 << endl;
            pmEvent->timestamp = 1000.0 * midifile.getTimeInSeconds(event.tick);
            pmEvent->message = message;

            PmError sendError = Pm_Write(*stream, pmEvent, 24);
        }
        else
        {
            DEBUG_MSG("OTHER EVENT");
            PmEvent *pmEvent = new PmEvent;
            pmEvent->message = Pm_Message(event[0], event[1], event[2]);
            DEBUG_MSG("OTHER EVENT:WRITING");
            Pm_Write(*stream, pmEvent, 1);
        }
    }

    DEBUG_MSG("CLOSING STREAM");
    Pm_Close(stream);
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
