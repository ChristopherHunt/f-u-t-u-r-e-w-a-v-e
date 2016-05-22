#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include "network/network.hpp"

#define NUM_SYNC_TRIALS 3     // Number of times to sync with a client to
                              // established an avg. delay profile.

typedef struct ClientInfo {
   int fd;
   sockaddr_in addr;
   uint32_t seq_num;
   long avg_delay;
   long last_msg_send_time;
   std::vector<long> delay_times;
} ClientInfo;

namespace server {
   enum State { HANDSHAKE, WAIT_FOR_INPUT, PARSE_SONG, PLAY_SONG, SONG_FIN, DONE };
};

class Server {
   private:
      int server_sock;            // Server's socket fd.
      sockaddr_in local;          // Local socket config.
      uint32_t port;              // The server's port.
      fd_set normal_fds;          // Set of fds to for normal messages.
      fd_set priority_fds;        // Set of fds for priority messages.
      struct timeval tv;          // Timeval for select.

      int file_fd;                // File descriptor to the song file to read/play
      std::string filename;       // Name of the song file to read/play
      uint8_t temp[MAX_BUF_SIZE]; // Temporary buffer to hold a received packet.

      double error_percent;       // The percentage of packets the server drops

      int next_client_id;         // The id to be assigned to the next client.

      server::State state;        // Current state of the Server's state machine.
      std::deque<ClientInfo> priority_messages; // deque of ClientInfo with high priority

      // Mapping of client socket fd to the client's ClientInfo struct.
      std::unordered_map<int, ClientInfo> fd_to_client_info;

      uint32_t seq_num;       // Sequence number for packets.

      // computes delay profile times in the delay times vector
      void calc_delay(ClientInfo &client);

      // Configures the fd_set to contain the stdin.
      void config_fd_set_for_stdin();

      // Configures the fd_set to contain the server_sock.
      void config_fd_set_for_server_socket();

      // Configures the fd_set to contain all normal traffic.
      void config_fd_set_for_normal_traffic();

      // Configures the fd_set for priority traffic
      void config_fd_set_for_priority_traffic();

      // Checks to see if there are any available connections.
      int new_connection_ready();

      // Parses command line arguments
      bool parse_inputs(int num_args, char **arg_list);

      // Prints the usage message specifying the input arguments to the Server
      // constructor.
      void print_usage();

      // Sets the timeval struct tv to have timeout number of seconds.
      void set_timeval(uint32_t timeout);

      // Sets up the server's socket to receive connections on.
      void setup_udp_socket();

      // Returns the number of fds with pending packets to recv.
      int check_for_response(uint32_t timeout);

      // Initialize all variables in the Server object to default values.
      void init();

      // Cleanup after the file transfer.
      void handle_done();

      // Handles the handshake portion of the file transfer.
      void handle_handshake();

      void handle_wait_for_input();

      void handle_parse_song();

      void handle_play_song();

      void handle_song_fin();

      void handle_abort();

      void handle_client_packet(int fd);

      void handle_client_timing(ClientInfo& info);

      // Handle input from the user on stdin
      void handle_stdin();

      // Handle a new client that has connected to the server
      void handle_new_client();

      // Handle a normal message from the client
      void handle_normal_msg();

      // Handle a priority message from the client
      void handle_priority_msg();

      // Sends a sync packet to the client specified by the ClientInfo struct,
      // incrementing its seq_num count and setting the last_msg_send_time field
      // to the current time.
      void send_sync_packet(ClientInfo& info);

      // Returns true if the specified local file for writing was opened.
      int open_target_file(std::string& target_filename);

      // Parses a handshake packet and returns true if it is valid.
      bool parse_handshake();

      // Parses the midi song to determine if valid
      bool parse_midi_input();

      // Waits 10 seconds for a handshake packet to come in. If one does
      // arrive, the packet is parsed and the state of the file transfer
      // is advanced if the packet is good.
      void wait_for_handshake();

      // The main state machine loop for the server.
      void ready_go();

      // Prints the state of the server's priority_message deque and the
      // fd_to_client_info map.
      void print_state();

   public:
      // Base constructor, takes in a list of arguments and their count to be
      // parsed and used for the filetransfer.
      Server(int num_args, char **arg_list);

      // Base destructor.
      ~Server();
};

#endif
