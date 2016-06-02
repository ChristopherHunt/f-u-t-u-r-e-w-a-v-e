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

enum ParseArgs {SIMULATED_LATENCY, REMOTE_MACHINE, REMOTE_PORT};

Client::Client(int num_args, char **arg_list) {
   // Set sequence number to 0 since we are just starting.
   seq_num = 0;

   // Initialize the timeout count to zero for connection attempts
   timeout_count = 0;

   // Put object into the HANDSHAKE state.
   state = client::HANDSHAKE;

   // Have the midi_header overlay the buf.
   midi_header = (Packet_Header *)buf;

   // Clear buffer
   memset(buf, '\0', MAX_BUF_SIZE);

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

void Client::cleanup() {
}

void Client::config_fd_set() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   // Add server socket fd to the set of fds to check.
   FD_SET(server_sock, &rdfds);
}

void Client::handle_done() {
   fprintf(stderr, "No server found, exiting!\n");
   exit(1);
}

void Client::handle_handshake() {
   // Setup main socket for the client to connect to the server on.
   setup_udp_socket();

   // Send handshake to the server.
   send_handshake();

   if (check_for_response(1)) {
      // Obtain server's response
      recv_packet_into_buf(sizeof(Handshake_Packet));

      // Parse the received data.
      switch (parse_handshake_ack()) {
         case flag::HS_GOOD:
            // Start the midi timer
            Pm_Initialize();
            // fprintf(stderr, "Initialized!\n");
            Pt_Start(1, &process_midi, (void *)this);
            // fprintf(stderr, "timer started!\n");

            // Get the default midi device
            default_device_id = Pm_GetDefaultOutputDeviceID();
            // fprintf(stderr, "default_device_id: %d\n", default_device_id);

            // Setup the output stream for playing midi.
            Pm_OpenOutput(&stream, default_device_id, NULL, 1, NULL, NULL, 0);
            // fprintf(stderr, "Opened stream!\n");

            timeout_count = 0;
            send_handshake_fin();
            state = client::TWIDDLE;
            break;

         case flag::HS_FAIL:
            // fprintf(stderr, "Could not handshake with the server!\n");
            state = client::DONE;
            break;

         default:
            fprintf(stderr, "Fell through client handle_handshake!\n");
            exit(1);
            break;
      }

      timeout_count = 0;
   }
   else  {
      ++timeout_count;
      if (timeout_count == MAX_TIMEOUTS) {
         fprintf(stderr, "Could not connect to server, exiting!\n");
         exit(1);
      }
   }
}

void Client::handle_midi_data() {
  //  fprintf(stderr, "Client::handle_midi_data!\n");
  //  for (int i = 0; i < MAX_BUF_SIZE; ++i) {
  //     fprintf(stderr, "%02x ", buf[i]);
  //  }

   /*
   // Receive the remainder of the midi song data.
   int bytes_recv;
   int total_bytes_recv = sizeof(Packet_Header);
   uint8_t num_midi_events = midi_header->num_midi_events;
   int total_bytes_to_recv = num_midi_events * SIZEOF_MIDI_EVENT;
   fprintf(stderr, "client::handle_midi_data num_midi_events: %d\n",
         num_midi_events);
   fprintf(stderr, "client::handle_midi_data expecting: %d bytes\n",
         num_midi_events * SIZEOF_MIDI_EVENT);

   while (total_bytes_recv != total_bytes_to_recv) {
      fprintf(stderr, "here!\n");
      bytes_recv = recv_buf(server_sock, &server, buf + total_bytes_recv,
            total_bytes_to_recv - total_bytes_recv);
      fprintf(stderr, "bytes_recv: %d\n", bytes_recv);
      if (bytes_recv >= 0) {
         for (int i = 0; i < bytes_recv; ++i) {
            fprintf(stderr, "%02x ", *(buf + total_bytes_recv + i));
         }
         fprintf(stderr, "\n");
         total_bytes_recv += bytes_recv;
      }
      //fprintf(stderr, "client::handle_midi_data recevied: %d total bytes\n", total_bytes_recv);
   }

   ASSERT(total_bytes_recv == total_bytes_to_recv);

   // TODO -- REMOVE
   fprintf(stderr, "00 -- ");
   for (int i = 0; i < 6; ++i) {
      fprintf(stderr, "%02x ", buf[i]);
   }
   fprintf(stderr, "\n01 -- ");
   int j = 0;
   int k = 1;
   for(int i = 6; i < MAX_BUF_SIZE; ++i)
   {
      fprintf(stderr, "%02x ", buf[i]);
      ++j;
      if (j == 7) {
         fprintf(stderr, "\n%02x -- ", ++k);
         j = 0;
      }
   }
   fprintf(stderr,"\n");
   //
   */

   int buf_offset = sizeof(Packet_Header);
   uint8_t num_midi_events = midi_header->num_midi_events;

   // Loop through all midi events
   for (int i = 0; i < num_midi_events; ++i) {
      // Pull out each midi message from the buffer
      my_event = (MyPmEvent *)(buf + buf_offset);

      // Make a message object to wrap this midi message
      message = Pm_Message(my_event->message[0], my_event->message[1],
            my_event->message[2]);

      // Wrap the message and its timestamp in a midi event
      event.message = message;
      event.timestamp = my_event->timestamp;

      /*
      // TODO -- REMOVE
      uint8_t *ptr = (uint8_t *)my_event;
      for (int j = 0; j < SIZEOF_MIDI_EVENT; ++j) {
         fprintf(stderr, "%02x ", ptr[j]);
      }
      fprintf(stderr, "\n");
      //
      */

      // Send this midi event to output
      Pm_Write(stream, &event, 1);

      // Move offset to next midi message
      buf_offset += SIZEOF_MIDI_EVENT;
   }
}

void Client::handle_play() {
   fprintf(stderr, "handle_play() not implemented!\n");
   ASSERT(FALSE);
}

void Client::handle_sync() {
  //  fprintf(stderr, "Client::handle_sync!\n");
   int bytes_sent;
   // Parse the handshake ack to get the seq number.
   Packet_Header *ph = (Packet_Header *)buf;
   seq_num = ph->seq_num;

  //  fprintf(stderr, "the server sent seq_num: %d\n", seq_num);

   // Build the handshake fin packet
   ++seq_num;
   ph->seq_num = seq_num;
   ph->flag = flag::SYNC_ACK;

  //  fprintf(stderr, "responding with seq_num: %d\n", ph->seq_num);

   // Send the handshake fin packet to the server.
   uint16_t packet_size = sizeof(Packet_Header);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);
}

flag::Packet_Flag Client::parse_handshake_ack() {
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   return (flag::Packet_Flag)(hs->header.flag);
}

bool Client::parse_inputs(int num_args, char **arg_list) {
   if (num_args != INPUT_ARG_COUNT) {
      printf("Improper argument count.\n");
      return false;
   }

   char *endptr;
   server_machine = std::string(arg_list[REMOTE_MACHINE]);

   simulated_latency = strtol(arg_list[SIMULATED_LATENCY], &endptr, 10);
   if (endptr == arg_list[SIMULATED_LATENCY] || simulated_latency < 0) {
      printf("Invalid simulated latency: '%s'\n", arg_list[SIMULATED_LATENCY]);
      printf("Simulated latency must be greater than 0\n");
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
   printf("Usage: client <simulated-latency> <server-machine> <server-port>\n");
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

int Client::recv_packet_into_buf(uint32_t packet_size) {
   int bytes_recv = recv_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_recv == packet_size);
   return bytes_recv;
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

  //  fprintf(stderr, "Client::send_handshake_fin!\n");
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

void process_midi(PtTimestamp timestamp, void *userData) {
   // Lookup the time and populate the server's clock
   ((Client *)userData)->midi_timer = Pt_Time();
}

void Client::twiddle() {
   // Wait for packet
   if (check_for_response(0)) {
      // Recive the packet into the buffer
      int bytes_recv = recv_buf(server_sock, &server, buf, MAX_BUF_SIZE);
      //int bytes_recv = recv_buf(server_sock, &server, buf, sizeof(Packet_Header));
      //ASSERT(bytes_recv == sizeof(Packet_Header));

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
            handle_midi_data();
            break;
         default:
            fprintf(stderr, "Client::twiddle fell through!\n");
            // fprintf(stderr, "packet flag: %d\n", flag);
            ASSERT(FALSE);
            break;
      }
   }
}
