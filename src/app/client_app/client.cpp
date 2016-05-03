#include <arpa/inet.h>        // htons
#include <netdb.h>            // hostent
#include <stdio.h>            // printf
#include <fcntl.h>            // open, O_RDONLY
#include <iostream>
#include <stdlib.h>           // strtol, strtod, exit, calloc, free
#include <string.h>           // memset, memcpy
#include <sys/socket.h>       // socket
#include <sys/select.h>       // select
#include <sys/stat.h>         // stat
#include <sys/types.h>
#include <errno.h>            // errno
#include <unistd.h>           // access
#include <utility>            // std::pair, std::get
#include "client/client.hpp"

enum ParseArgs {ERROR_PERCENT, REMOTE_MACHINE, REMOTE_PORT};

Client::Client(int num_args, char **arg_list) {
   // Set sequence number to 0 since we are just starting.
   seq_num = 0;

   // Put object into the INIT state.
   state = INIT;

   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

   // Initialize the error functions.
   sendErr_init(error_percent, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

   // Drop into the client state machine.
   ready_go();
}

Client::~Client() {
}

void Client::cleanup() {
}

int Client::check_for_response(uint32_t timeout) {
   config_fd_set();
   set_timeval(timeout);

   int num_fds_available = selectMod(remote_sock + 1, &rdfds, NULL, NULL, &tv);
   ASSERT(num_fds_available >= 0);

   return num_fds_available;
}

void Client::check_timeout() {
   if (timeout_count == MAX_TIMEOUTS) {
      printf("Server unreachable, exiting.\n");
      exit(1);
   }
}

void Client::config_fd_set() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   // Add remote socket fd to the set of fds to check.
   FD_SET(remote_sock, &rdfds);
}

// TODO
/*
void Client::handle_play() {
   // If there is a response from the server.
   while (check_for_response(0)) {
      // Receive the next packet in the queue.
      recv_flag_packet();

      // Parse the packet as a flag packet.
      std::pair<Packet_Flag, uint32_t> tuple = parse_flag_packet();

      // Pull out the flag and the reference sequence number.
      Packet_Flag packet_flag = tuple.first;
      uint32_t rr_number = tuple.second;

      switch (packet_flag) {
         case RR:
            // Check for extraneous packets.
            if (rr_number > bottom_edge) {
               // Update the window based on the packet's RR number.
               update_window_bookkeeping(rr_number);
               state = WINDOW_OPEN;
            }
            break;

         case SREJ:
            send_buffered_packet(rr_number);
            break;

         case CORRUPT:
            break;

         default:
            break;
      }
   }

   if (lower_edge == upper_edge) {
      timeout_count = 0;
      state = WINDOW_CLOSED;
   }
   else {
      state = WINDOW_OPEN;
   }
}
*/

void Client::handle_handshake() {
   // Setup main socket for the client to connect to the server on.
   setup_udp_socket();

   // Send handshake to the server.
   send_handshake();

   // Select for 1 second waiting for the server to respond.
   if (check_for_response(1) == 0) {
      ++timeout_count;
      check_timeout();
   }
   // Server responded
   else {
      // Obtain server's response
      recv_packet_into_buf(sizeof(Handshake_Packet));

      // Parse the received data.
      switch (parse_handshake_ack()) {
         case HS_GOOD:
            timeout_count = 0;
            state = TWIDDLE;
            break;

         case HS_FAIL:
            state = EXIT;
            break;

         default:
            fprintf(stderr, "Fell through client handle_handshake!\n");
            exit(1);
            break;
      }
   }
}

void Client::handle_init() {
   timeout_count = 0;

   // Move onto HANDSHAKE state.
   state = HANDSHAKE;
}

Packet_Flag Client::parse_handshake_ack() {
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   return (Packet_Flag)(hs->header.flag);
}

int Client::recv_packet_into_buf(uint32_t packet_size) {
   int bytes_recv = recv_buf(remote_sock, &remote, buf, packet_size);
   ASSERT(bytes_recv == packet_size);
   return bytes_recv;
}

bool Client::parse_inputs(int num_args, char **arg_list) {
   if (num_args != INPUT_ARG_COUNT) {
      printf("Improper argument count.\n");
      return false;
   }

   char *endptr;
   remote_machine = std::string(arg_list[REMOTE_MACHINE]);

   error_percent = strtod(arg_list[ERROR_PERCENT], &endptr);
   if (endptr == arg_list[ERROR_PERCENT] || error_percent < 0 || error_percent > 1) {
      printf("Invalid error percent: '%s'\n", arg_list[ERROR_PERCENT]);
      printf("Error percent must be between 0 and 1 inclusive.\n");
      return false; 
   }

   remote_port = (uint32_t)strtol(arg_list[REMOTE_PORT], &endptr, 10);
   if (endptr == arg_list[REMOTE_PORT]) {
      printf("Invalid remote port: '%s'\n", arg_list[REMOTE_PORT]);
      return false; 
   }

   return true;
}

void Client::print_usage() {
   printf("Usage: client <error-rate> <remote-machine> <remote-port>\n");
}

void Client::send_handshake() {
   // Build the handshake packet
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   hs->header.seq_num = 0;
   hs->header.flag = HS;

   // Send the handshake packet to the server.
   uint16_t packet_size = sizeof(Handshake_Packet);
   send_buf(remote_sock, &remote, buf, packet_size);
}

void Client::set_timeval(uint32_t timeout) {
   tv.tv_sec = timeout;
   tv.tv_usec = 0;
}

void Client::setup_udp_socket() {
   // Create a socket to connect to the server on.
   remote_sock = socket(AF_INET, SOCK_DGRAM, 0);
   ASSERT(remote_sock >= 0);

   // Configure the remote sockaddr prior to sending data.
   hostent *hp;
   hp = gethostbyname(remote_machine.c_str());

   if (hp == NULL) {
      printf("Could not resolve server ip %s, exiting.\n",
            remote_machine.c_str());
      exit(1);
   }

   memcpy(&remote.sin_addr, hp->h_addr, hp->h_length);
   remote.sin_family = AF_INET;           // IPv4
   remote.sin_port = htons(remote_port);  // Use specified port                
}

void Client::ready_go() {
   while (true) {
      switch (state) {
         case INIT:
            handle_init();
            break;

         case HANDSHAKE:
            handle_handshake();
            break;

         case TWIDDLE:
            handle_twiddle();
            break;

         case PLAY:
            handle_play();
            break;

         case EXIT:
            handle_exit();
            break;

         default:
            fprintf(stderr, "Fell through client state machine!\n");
            exit(1);
            break;
      }
   }
}

/*
int main(int argc, char **argv) {
   Client(argc - 1, argv + 1);
   return 0;
}
*/
