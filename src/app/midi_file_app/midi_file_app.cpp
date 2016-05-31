#include "midi_file_app.h"
#include <unistd.h>
#include <queue>
#include <iostream>

#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)

using namespace std;
PortMidiStream **stream;
queue<PmEvent *> *eventQueue;

void process_midi(PtTimestamp timestamp, void *userData)
{
    //Peek at the top of the queue
    PmEvent *event = eventQueue->front();
    cout << "Timestamp at front of queue: " << event->timestamp << endl;
    cout << "Pt time: " << Pt_Time() << endl;
    cout << "Message: " << event->message << endl;
    // While we have events to be played in the queue, play them
    while (event->timestamp <= Pt_Time())
    {
        Pm_Write(*stream, event, 1);
        // Remove played event
        eventQueue->pop();
        // Peek at the top of the queue;
        event = eventQueue->front();   
    }
}

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
        cout << "Midi event at tick " << event.tick << endl;
        printMidiPacketInfo(event);         
    }

    // Test out port midi
    Pm_Initialize();
    cout << "Number of midi devices available: " << Pm_CountDevices() << endl;

    int defaultDeviceID = Pm_GetDefaultOutputDeviceID();
    cout << "The default output device set by the configuration is : " << defaultDeviceID << endl;
    cout << "opening device for writing..." << endl;

    stream = new PortMidiStream*;
    PtError timeError = Pt_Start(1, &process_midi, NULL); 
    if (timeError != 0)
    {
        cout << "Error starting timer: " << timeError;
        exit(1);
    }    
    cout << "Timer started" << endl;

    PmError midiError = Pm_OpenOutput(stream, defaultDeviceID, NULL, 1, NULL, NULL, 0); 

    if (midiError != 0)
    {
        cout << Pm_GetErrorText(midiError);
        exit(1);
    }

    cerr << "Opened midi device successfully!" << endl;

    // Openening event queue
    eventQueue = new queue<PmEvent *>(); 

    //for(int i=0; i < midifile[1].size(); i++)
    for(int i=0; i < midifile[track].size(); i++)
    {
        MidiEvent event = (MidiEvent)midifile[track][i];

        //Convert a MidiEvent into a Portmidi message
        //Copy midi data into a PmEvent buffer
        PmMessage message = Pm_Message(event[0], event[1], event[2]);
        cout << dec << event.tick;
        cout << '\t' << hex;
        cout << (int)event[0] << ' ' << (int)event[1] << ' ' << (int)event[2] << endl;

        // Events wrap messages and timestamps
        // This timestamp tells portmidi when to send stuff from the buffer, 0 means immediately
        // Different than any timing stuff in the packet itself
        PmEvent *pmEvent = new PmEvent;
        pmEvent->timestamp = midifile.getTimeInSeconds(event.tick) * 1000.0;
        pmEvent->message = message;
        
        // Throw the event into the queue
        eventQueue->push(pmEvent);    
    }
    
    do
    {
        cout << '\n' << "Press any key to exit...";
    } while (cin.get() != '\n');
    
    Pt_Stop(); 
    Pm_Close(stream);
    Pm_Terminate();

   
    return 0;
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
