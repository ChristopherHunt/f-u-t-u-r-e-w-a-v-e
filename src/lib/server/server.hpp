#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>
#include "network/network.hpp"
#include "midifile/include/MidiFile.h"
//#include "midifile/include/Options.h"
#include "portmidi/include/portmidi.h"
#include "portmidi/include/porttime.h"

#define NUM_SYNC_TRIALS 3     // Number of times to sync with a client to
                              // established an avg. delay profile.

#define NUM_DELAY_SAMPLES 3  // Number of delay times each client keeps track
                              // of when computing average delay.

typedef struct ClientInfo {
   // Client's socket fd
   int fd;

   // Client's socket address information
   sockaddr_in addr;

   // Current sequence number to send next for this client
   uint32_t seq_num;

   // The expected next sequence number from the client
   uint32_t expected_seq_num;

   // The average delay of this client (used for syncing with other clients)
   long avg_delay;

   // The time the last sync message was sent to the client
   long last_msg_send_time;

   // A temp variable to hold syncing values (which are averaged before putting
   // them into the delay_times deque).
   long session_delay;

   // A counter to determine how many times to send sync packets per sync
   // session.
   int session_delay_counter;

   // Container of sync times which can be averaged.
   std::deque<long> delay_times;

   // Tracks this client is responsible for playing (the server uses this to
   // figure out who to send tracks to).
   std::vector<int> tracks;

   //std::unordered_map<uint32_t, long> packet_to_send_time;
} ClientInfo;

namespace server {
   enum State { HANDSHAKE, WAIT_FOR_INPUT, PARSE_SONG, PLAY_SONG, SONG_FIN, DONE };
};

void process_midi(PtTimestamp timestamp, void *userData);

class Server {
   private:
      uint32_t port;              // The server's port.

      int server_sock;            // Server's socket fd.
      int max_sock;               // Maximum server socket number.
      sockaddr_in local;          // Local socket config.
      fd_set normal_fds;          // Set of fds to for normal messages.
      fd_set priority_fds;        // Set of fds for priority messages.
      struct timeval tv;          // Timeval for select.

      int file_fd;                // File descriptor to the song file to read/play
      std::string filename;       // Name of the song file to read/play
      uint8_t buf[MAX_BUF_SIZE];  // Temporary buffer to hold a received packet.
      uint64_t buf_offset;        // Offset to index into the buffer with.

      Handshake_Packet *hs;       // Overlay on top of the buffer.
      Packet_Header *midi_header; // Overlay on top of the buffer.
      int next_client_id;         // The id to be assigned to the next client.
      bool song_is_playing;       // Tells the state machine we are playing a song

      server::State state;        // Current state of the Server's state machine.
      MidiFile midifile;          // Midifile object to parse midi data
      PtError time_error;         // Time error
      long max_client_delay;      // The current max delay from any client

      ClientInfo *sync_client;    // Client that is currently being synced.

      // Iterator which decides which client to try and sync with at any given
      // time.
      std::unordered_map<int, ClientInfo>::iterator sync_it;

      // Mapping of client socket fd to the client's ClientInfo struct.
      std::unordered_map<int, ClientInfo> fd_to_client_info;

      // Mapping of tracks to their queue of events to be played.
      std::unordered_map<int, std::deque<MyPmEvent> > track_queues;

      // Appends the event to the buffer, incrementing the number of midi
      // messages in the buffer's midi_header.
      void append_to_buf(MyPmEvent *event);

      // computehandle_plays delay profile times in the delay times vector
      void calc_delay(ClientInfo &client);

      // Configures the fd_set to contain all normal traffic.
      void config_fd_set_for_normal_traffic();

      // Configures the fd_set for priority traffic
      void config_fd_set_for_priority_traffic();

      // Configures the fd_set to contain the server_sock.
      void config_fd_set_for_server_socket();

      // Configures the fd_set to contain the stdin.
      void config_fd_set_for_stdin();

      // Checks to see if there are any available connections.
      int connection_ready(fd_set fds);

      // Handles aborting the server.
      void handle_abort();

      // Handles any message sent from the client to the server.
      void handle_client_packet(int fd);

      // Determines what the delay of the client is.
      void handle_client_timing(ClientInfo& info);

      // Cleanup after the file transfer.
      void handle_done();

      // Handles the handshake portion of the file transfer.
      void handle_handshake();

      // Handle a new client that has connected to the server.
      void handle_new_client();

      // Handle a normal message from the client.
      void handle_normal_msg();

      // Parses the midi song, breaking it down into subsequent tracks and
      // assigning those tracks to clients for playing.
      void handle_parse_song();

      // Checks to see if midi event(s) are ready to be played and sends them
      // out to the client(s).
      void handle_play_song();

      // Handle a priority message from the client.
      void handle_priority_msg();

      // Handles the end of a song (if we want to do this still).
      void handle_song_fin();

      // Handle input from the user on stdin.
      void handle_stdin();

      // Handles the state where the client is waiting for an event to occur
      // (either a midi event is ready to be sent or a client has responded
      // / connected).
      void handle_wait_for_input();

      // Initialize all variables in the Server object to default values.
      void init();

      // Returns true if the specified local file for writing was opened.
      int open_target_file(std::string& target_filename);

      // Parses a handshake packet and returns true if it is valid.
      bool parse_handshake();

      // Parses command line arguments
      bool parse_inputs(int num_args, char **arg_list);

      // Parses the midi song to determine if valid
      bool parse_midi_input();

      // Prints the state of the server's priority_message deque and the
      // fd_to_client_info map.
      void print_state();

      // Prints the usage message specifying the input arguments to the Server
      // constructor.
      void print_usage();

      // The main state machine loop for the server.
      void ready_go();

      // Sends the content in the buffer to the client at the specified socket.
      int send_midi_msg(ClientInfo *info);

      // Sends a sync packet to the client specified by the ClientInfo struct,
      // incrementing its seq_num count and setting the last_msg_send_time field
      // to the current time.
      void send_sync_packet(ClientInfo& info);

      // Sets up the buffer as a midi message to the specified client.
      void setup_midi_msg(ClientInfo *);

      // Sets up the server's socket to receive connections on.
      void setup_udp_socket();

      // Sets the timeval struct tv to have timeout number of seconds.
      void set_timeval(uint32_t timeout);

      // Waits 10 seconds for a handshake packet to come in. If one does
      // arrive, the packet is parsed and the state of the file transfer
      // is advanced if the packet is good.
      void wait_for_handshake();

      // TODO -- For testing only
      // Call this to setup the PortMidi timer and device.
      void setup_music_locally();

      // Call this to play the music in buf at a given offset through the midi
      // device.
      void play_music_locally(uint8_t *buf, int offset);

      PortMidiStream *stream;       // Pointer to the port midi output stream.
      //

   public:
      PtTimestamp midi_timer;     // Midi timer (int32_t)

      // Base constructor, takes in a list of arguments and their count to be
      // parsed and used for the filetransfer.
      Server(int num_args, char **arg_list);

      // Base destructor.
      ~Server();
};

#endif
