#ifndef __CLIENT__HPP__
#define __CLIENT__HPP__

#include <stdint.h>
#include <string>
#include "network/network.hpp"

#define MAX_TIMEOUTS 5
#define INPUT_ARG_COUNT 3

enum Client_State { INIT, HANDSHAKE, TWIDDLE, PLAY, EXIT };

class Client {
   private:
      // States of the file transfer state machine.
      Client_State state;           // State of the instrument node.

      sockaddr_in remote;           // Server connection information.
      int remote_sock;              // Socket for connecting to the server.
      uint32_t remote_port;         // Port of server.
      std::string remote_machine;   // Server's IP.

      double error_percent;         // Percentage of packets to lose or corrupt.
      fd_set rdfds;                 // Set of fds to select on.
      struct timeval tv;            // Timeval for select.
      uint8_t timeout_count;        // # times select has timed out in a row.

      uint8_t buf[MAX_BUF_SIZE];    // Buffer used for message handling.
      uint32_t buffer_size;         // Buffer size for packet data region.

      int seq_num;                  // The current packet sequence number.
      uint32_t max_packet_num;      // Max packet sequence number expected.

      // Returns true if there's a response ready for receiving from the server.
      int check_for_response(uint32_t timeout);

      // Checks if the current select_timeout is equal to the max allowable
      // timeouts, and if it is is prints an error message and exits.
      void check_timeout();

      // Frees all resources held by the client for a given song.
      void cleanup();

      // Configures a fresh FD_SET that contains the remote_sock.
      void config_fd_set();

      // Handles the exit message from the server, closing the client. 
      void handle_exit();

      // Handles the setup of the client with the server.
      void handle_handshake();

      // Handles the initialization of the client object.
      void handle_init();

      // Handles the playing of the song's midi events from the server.
      void handle_play();

      // Handles the waiting state of the client when it is sitting around for
      // instructions from the server.
      void handle_twiddle();

      // Parses a handshake ack, returning its flag.
      Packet_Flag parse_handshake_ack(); 

      // Parses a list of arguments which correspond to the required
      // configuration options for the client class. If any errors are
      // detected, a message will be printed and the program will exit.
      bool parse_inputs(int num_args, char **arg_list);

      // Prints the usage message specifying the input arguments to the client
      // constructor.
      void print_usage();

      // Drops the client into the state machine to connect with the server and
      // play a song.
      void ready_go();

      // Checks for outstanding messages to the client object and processes
      // all of them, updating internal bookkeeping.
      void recv_and_parse_midi_data();

      // Assemble and send the handshake packet to the server.
      void send_handshake();

      // Recv's the contents of a UDP message into the client's message buffer
      // for futher processing by other functions, requiring that packet_size
      // number of bytes are recv'd.
      int recv_packet_into_buf(uint32_t packet_size);

      // Sets tv to have timeout seconds.
      void set_timeval(uint32_t timeout);

      // Sets up the client's socket to connect to the server on.
      void setup_udp_socket();

   public:
      // Base constructor, takes in a list of arguments and their count to be
      // parsed and used for the filetransfer.
      Client(int num_args, char **arg_list);

      // Base destructor.
      ~Client();

};

#endif
