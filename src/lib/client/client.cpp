#include <arpa/inet.h>        // htons
#include <netdb.h>            // hostent
#include <stdio.h>            // printf
#include <fcntl.h>            // open, O_RDONLY
#include <iostream>
#include <string>
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

   // Put object into the HANDSHAKE state.
   state = client::HANDSHAKE;

   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

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

   //int num_fds_available = selectMod(server_sock + 1, &rdfds, NULL, NULL, &tv);
   int num_fds_available = select(server_sock + 1, &rdfds, NULL, NULL, &tv);
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

   // Add server socket fd to the set of fds to check.
   FD_SET(server_sock, &rdfds);
}

void Client::handle_handshake() {
   // Setup main socket for the client to connect to the server on.
   setup_udp_socket();

   // Send handshake to the server.
   send_handshake();

   if (check_for_response(5)) {
      // Obtain server's response
      recv_packet_into_buf(sizeof(Handshake_Packet));

      // Parse the received data.
      switch (parse_handshake_ack()) {
         case flag::HS_GOOD:
            timeout_count = 0;
            send_handshake_fin();
            state = client::TWIDDLE;
            break;

         case flag::HS_FAIL:
            fprintf(stderr, "Could not handshake with the server!\n");
            state = client::DONE;
            break;

         default:
            fprintf(stderr, "Fell through client handle_handshake!\n");
            exit(1);
            break;
      }
   }
}

void Client::handle_done() {
   fprintf(stderr, "No server found, exiting!\n");
   exit(1);
}

void Client::handle_play() {
   fprintf(stderr, "handle_play() not implemented!\n");
   ASSERT(FALSE);
}

void Client::twiddle() {
   // Wait for packet
   if (check_for_response(0)) {
      // Recive the packet into the buffer
      int bytes_recv = recv_buf(server_sock, &server, buf, sizeof(Packet_Header));
      ASSERT(bytes_recv == sizeof(Packet_Header));

      // Treat this message as a normal message
      Packet_Header *ph = (Packet_Header*)buf;

      // Parse the packet
      flag::Packet_Flag flag;
      flag = (flag::Packet_Flag)ph->flag;

      // This packet has to either be a handshake_fin packet or a sync_ack packet.
      switch (flag) {
         case flag::SYNC:
            handle_sync();
            break;
         case flag::MIDI:
            fprintf(stderr, "Client::midi flag not implemented yet!\n");
            exit(1);
            break;
         default:
            fprintf(stderr, "Client::twiddle fell through!\n");
            ASSERT(FALSE);
            break;
      }
   }
}

flag::Packet_Flag Client::parse_handshake_ack() {
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   return (flag::Packet_Flag)(hs->header.flag);
}

void Client::handle_sync() {
   int bytes_sent;
   fprintf(stderr, "Client::handle_sync!\n");
   // Parse the handshake ack to get the seq number.
   Packet_Header *ph = (Packet_Header *)buf;
   seq_num = ph->seq_num;

   fprintf(stderr, "the server sent seq_num: %d\n", seq_num);

   // Build the handshake fin packet
   ++seq_num;
   ph->seq_num = seq_num;
   ph->flag = flag::SYNC_ACK;

   fprintf(stderr, "responding with seq_num: %d\n", ph->seq_num);

   std::string meh;
   std::cin >> meh;
   std::cout << meh << std::endl;

   // Send the handshake fin packet to the server.
   uint16_t packet_size = sizeof(Packet_Header);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);
}

int Client::recv_packet_into_buf(uint32_t packet_size) {
   int bytes_recv = recv_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_recv == packet_size);
   return bytes_recv;
}

bool Client::parse_inputs(int num_args, char **arg_list) {
   if (num_args != INPUT_ARG_COUNT) {
      printf("Improper argument count.\n");
      return false;
   }

   char *endptr;
   server_machine = std::string(arg_list[REMOTE_MACHINE]);

   error_percent = strtod(arg_list[ERROR_PERCENT], &endptr);
   if (endptr == arg_list[ERROR_PERCENT] || error_percent < 0 || error_percent > 1) {
      printf("Invalid error percent: '%s'\n", arg_list[ERROR_PERCENT]);
      printf("Error percent must be between 0 and 1 inclusive.\n");
      return false; 
   }

   server_port = (uint32_t)strtol(arg_list[REMOTE_PORT], &endptr, 10);
   if (endptr == arg_list[REMOTE_PORT]) {
      printf("Invalid server port: '%s'\n", arg_list[REMOTE_PORT]);
      return false; 
   }

   return true;
}

void Client::print_usage() {
   printf("Usage: client <error-rate> <server-machine> <server-port>\n");
}

void Client::send_handshake() {
   int bytes_sent;

   // Build the handshake packet
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   hs->header.seq_num = 0;
   hs->header.flag = flag::HS;

   // Send the handshake packet to the server.
   uint16_t packet_size = sizeof(Handshake_Packet);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);
}

void Client::send_handshake_fin() {
   int bytes_sent;

   fprintf(stderr, "Client::send_handshake_fin!\n");
   // Parse the handshake ack to get the seq number.
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   seq_num = hs->header.seq_num;
   ASSERT(seq_num == 1);

   // Build the handshake fin packet
   ++seq_num;
   hs->header.seq_num = seq_num;
   ASSERT(hs->header.seq_num == 2);
   hs->header.flag = flag::HS_FIN;

   // Send the handshake fin packet to the server.
   uint16_t packet_size = sizeof(Handshake_Packet);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);
}

void Client::set_timeval(uint32_t timeout) {
   tv.tv_sec = timeout;
   tv.tv_usec = 0;
}

void Client::setup_udp_socket() {
   // Create a socket to connect to the server on.
   server_sock = socket(AF_INET, SOCK_DGRAM, 0);
   ASSERT(server_sock >= 0);

   // Configure the server sockaddr prior to sending data.
   hostent *hp;
   hp = gethostbyname(server_machine.c_str());

   if (hp == NULL) {
      printf("Could not resolve server ip %s, exiting.\n",
            server_machine.c_str());
      exit(1);
   }

   memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
   server.sin_family = AF_INET;           // IPv4
   server.sin_port = htons(server_port);  // Use specified port                
}

void Client::ready_go() {
   while (true) {
      switch (state) {
         case client::HANDSHAKE:
            handle_handshake();
            break;

         case client::TWIDDLE:
            twiddle();
            break;

         case client::PLAY:
            handle_play();
            break;

         case client::DONE:
            handle_done();
            break;

         default:
            fprintf(stderr, "Fell through client state machine!\n");
            exit(1);
            break;
      }
   }
}
