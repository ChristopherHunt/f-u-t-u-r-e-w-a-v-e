#include "linux_midi_test.h"
#include <unistd.h>
#include <iostream>
#include <queue>

#include <errno.h>

#define NOTE_OFF 0x80
#define NOTE_ON  0x90

#define PMALSA

#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)

using namespace std;

PortMidiStream **stream;
queue<PmEvent *> *eventQueue;

void process_midi(PtTimestamp timestamp, void *userData) {
    int rtn = 0;
    //Peek at the top of the queue
    PmEvent *event = eventQueue->front();
    cout << "Timestamp at front of queue: " << event->timestamp << endl;
    cout << "Pt time: " << Pt_Time() << endl;
    cout << "Message: x" << std::uppercase << std::hex << event->message << endl;
    cout << "Pointer: " << std::uppercase << std::hex << *stream << endl;
    // While we have events to be played in the queue, play them
    while (event->timestamp <= Pt_Time())
    {
        DEBUG_MSG("FUCK FIRST");
        ASSERT_MSG(Pt_Started(), "portmidi not started");
        DEBUG_MSG("WRITING the thing");
        rtn = Pm_Write(*stream, event, 1);
        printf("SOME BULLSHIT\n");
        fflush(stdout);
        fflush(stderr);
        DEBUG_MSG("WROTE   the thing ?!");
        ASSERT_MSG(rtn == 0, "Pm_Write Failed");
        // Remove played event
        DEBUG_MSG("FUCK");
        eventQueue->pop();
        DEBUG_MSG("FUCK");
        // Peek at the top of the queue;
        DEBUG_MSG("FUCK");
        event = eventQueue->front();
        cerr << "timestamp: " << event->timestamp << endl;
        ASSERT_MSG(Pt_Started(), "portmidi not started");
        DEBUG_MSG("FUCK SLEEP");
        sleep(1);
        DEBUG_MSG("FUCK LAST");
    }
}

int main(int argc, char **argv) {
    ASSERT_MSG(errno == 0, "Error Doing what?!");

    Pm_Initialize();
    //ASSERT_MSG(errno == 0, "Error Initializing Pm?!");
    cout << "initialied..." << endl;
    cout << "Number of midi devices available: " << Pm_CountDevices() << endl;

    Pt_Start(1, &process_midi, NULL);
    //ASSERT_MSG(errno == 0, "Error Starting Timer");
    cout << "started timer..." << endl;

    int defaultDeviceID = Pm_GetDefaultOutputDeviceID();
    //ASSERT_MSG(errno == 0, "Error Getting Default OutputDeviceID");
    cout << "defaultDeviceID: " << defaultDeviceID << endl;

    stream = new PortMidiStream*;
    Pt_Start(1, &process_midi, NULL);
    //ASSERT_MSG(errno == 0, "Error Starting Timer");
    cout << "Timer started" << endl;
    PmError midiError = Pm_OpenOutput(stream, defaultDeviceID, NULL, 1, NULL, NULL, 0);
    ASSERT_MSG(midiError == 0, Pm_GetErrorText(midiError));
    cerr << "Opened midi device successfully! 0x" << std::hex << stream << endl;

    eventQueue = new queue<PmEvent *>();
    double timestamp = 0.0;
    unsigned char midi_channel = 1;
    unsigned char note_number = 69;
    unsigned char velocity = 127;
    MidiEvent event_list[5];
    for (int i = 0; i < 5; i++) {
        event_list[i] = MidiEvent();
        char command = 0x0;
        if (i % 2 == 0) {
            command |= NOTE_ON;
        } else {
            command |= NOTE_OFF;
        }
        command |= midi_channel & 0x0F;
        event_list[i].setCommandByte(command);
        event_list[i].setParameters(note_number, velocity);
    }
    for (int i = 0; i < 5; i++) {
        unsigned char command = 0x01;
        if (i % 2 == 0) {
            command |= NOTE_ON;
        } else {
            command |= NOTE_OFF;
        }
        //MidiEvent event = event_list[i];
        unsigned char event[3] = {command, note_number, velocity};
        DEBUG_MSG("Pm_message");
        PmMessage message = Pm_Message(event[0], event[1], event[2]);
        cout << "event[0]: 0x" << std::hex << (uint8_t)event[0] << endl;
        cout << "event[1]: 0x" << std::hex << (uint8_t)event[1] << endl;
        cout << "event[2]: 0x" << std::hex << (uint8_t)event[2] << endl;

        DEBUG_MSG("new PmEvent");
        PmEvent *pmEvent = new PmEvent;

        DEBUG_MSG("Setting timestamp");
        pmEvent->timestamp = timestamp++ * 1000.0;

        DEBUG_MSG("Setting message");
        pmEvent->message = message;
        eventQueue->push(pmEvent);
    }

    Pt_Stop();
    Pm_Close(stream);
    Pm_Terminate();
    return 0;
}
