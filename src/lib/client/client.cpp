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

enum ParseArgs {MIDI_CHANNEL, DELAY, REMOTE_MACHINE, REMOTE_PORT};

Client::Client(int num_args, char **arg_list) {

   queued_acks.clear();
   queued_events.clear();
   queued_syncs.clear();

   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

   // Initialize all variables needed by the client.
   init();

   // Drop into the client state machine.
   ready_go();
}

Client::~Client() {
}

void Client::config_fd_set_for_stdin() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   // Add STDIN to the set of fds to check
   FD_SET(STDIN, &rdfds);
}

int Client::connection_ready(uint32_t timeout) {
    set_timeval(timeout);

    //int num_fds_available = selectMod(server_sock + 1, &rdfds, NULL, NULL, &tv);
    int num_fds_available = select(server_sock + 1, &rdfds, NULL, NULL, &tv);
    ASSERT(num_fds_available >= 0);

    return num_fds_available;
}

int Client::check_for_response(uint32_t timeout) {
    int num_fds_available;
    int handle_data = 1;

    // Select on stdin to see if the user wants to do something
    config_fd_set_for_stdin();
    num_fds_available = connection_ready(0);
    ASSERT(num_fds_available >= 0);
    if (num_fds_available) {
       print_debug("handling stdin input!\n");
       handle_stdin();
       handle_data = 0;
       num_fds_available = 0;
    }

    if (handle_data) {
      // Select on fdS to see if there is midi data
       config_fd_set();
       num_fds_available = connection_ready(timeout);
       ASSERT(num_fds_available >= 0);
    }

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

void Client::handle_done() {
   fprintf(stderr, "Done state, exiting!\n");
   exit(1);
}

void Client::handle_handshake() {
   // Setup main socket for the client to connect to the server on.
   setup_udp_socket();

   // Send handshake to the server.
   send_handshake();

   if (check_for_response(1) && client_alive) {
      // Obtain server's response
      recv_packet_into_buf(sizeof(Handshake_Packet));

      // Parse the received data.
      switch (parse_handshake_ack()) {
         case flag::HS_GOOD:
            // Start the midi timer
            Pm_Initialize();
            print_debug("Initialized!\n");
            Pt_Start(1, &process_midi, (void *)this);
            print_debug("timer started!\n");

            // Get the default midi device
            default_device_id = Pm_GetDefaultOutputDeviceID();
            print_debug("default_device_id: %d\n", default_device_id);

            // Setup the output stream for playing midi.
            Pm_OpenOutput(&stream, default_device_id, NULL, 1, NULL, NULL, 0);
            print_debug("Opened stream!\n");

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

void Client::handle_stdin() {
   std::string user_input;
   getline(std::cin, user_input);

   std::istringstream iss(user_input);

   std::string token;
   iss >> token;

   // for conversion string to int
   char *endptr;

   if (token.compare("start") == 0) {
     fprintf(stderr, "restarting the client\n");
     client_alive = 1;
   } else if (token.compare("stop") == 0) {
     fprintf(stderr, "killing the client\n");
     client_alive = 0;
     queued_events.clear();
     queued_acks.clear();
   } else {
     int temp_delay = strtol(token.c_str(), &endptr, 10);
     if (temp_delay > -1) {
       delay = temp_delay;
       fprintf(stderr, "changing latency to %d\n", delay);
     }
   }

}

void Client::init() {
  // Set client_alive to 1 to simulate an active client.
  client_alive = 1;

  // Set sequence number to 0 since we are just starting.
  seq_num = 0;

  // Initialize the timeout count to zero for connection attempts
  timeout_count = 0;

  // Put object into the HANDSHAKE state.
  state = client::HANDSHAKE;

  // Have the midi_header overlay the buf.
  midi_header = (Packet_Header *)buf;

  // Clear the midi_ack's unneeded fields to avoid printout confusion.
  midi_ack.num_midi_events = 0;

  // Clear buffer
  memset(buf, '\0', MAX_BUF_SIZE);
}

void Client::queue_midi_data() {
   int buf_offset = sizeof(Packet_Header);
   uint8_t num_midi_events = midi_header->num_midi_events;

   get_current_time(&current_time);

   // Loop through all midi events
   for (int i = 0; i < num_midi_events; ++i) {
      // Pull out each midi message from the buffer
      my_event = (MyPmEvent *)(buf + buf_offset);

      // Add the message to the queue along with its timestamp.
      queued_events.push_back(std::make_pair(current_time + delay, *my_event));

      // Move offset to next midi message
      buf_offset += SIZEOF_MIDI_EVENT;
   }
}

void Client::play_midi_data() {
   get_current_time(&current_time);

   // Play all events that are ready
   while (queued_events.size() > 0 && queued_events.front().first < current_time) {
      // Grab the event
      my_event = &(queued_events.front().second);

      // Make a message object to wrap this midi message
      message = Pm_Message(my_event->message[0], my_event->message[1],
            my_event->message[2]);

      // Wrap the message and its timestamp in a midi event
      event.message = message;
      event.timestamp = my_event->timestamp;

      // Send this midi event to output
      Pm_Write(stream, &event, 1);

      // Pop the event off the queue
      queued_events.pop_front();
   }
}

void Client::handle_play() {
   fprintf(stderr, "handle_play() not implemented!\n");
   ASSERT(FALSE);
}

void Client::queue_sync() {
   print_debug("Client::queue_sync!\n");
   get_current_time(&current_time);
   queued_syncs.push_back(std::make_pair(current_time + delay, seq_num));
}

void Client::send_sync_ack(uint32_t packet_seq_num) {
   int bytes_sent;

   // Build the handshake fin packet
   midi_header->seq_num = packet_seq_num;
   midi_header->flag = flag::SYNC_ACK;

   print_debug("responding with seq_num: %d\n", midi_header->seq_num);

   // Send the handshake fin packet to the server.
   uint16_t packet_size = sizeof(Packet_Header);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);

   // Pop this sync message from the deque.
   queued_syncs.pop_front();
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

   midi_channel = strtol(arg_list[MIDI_CHANNEL], &endptr, 10);
   if (endptr == arg_list[MIDI_CHANNEL] || midi_channel < 0) {
      printf("Invalid midi channel: '%s'\n", arg_list[MIDI_CHANNEL]);
      printf("Midi channel number must be 0 or greater.\n");
      return false;
   }

   delay = strtol(arg_list[DELAY], &endptr, 10);
   if (endptr == arg_list[DELAY] || delay < 0) {
      printf("Invalid delay: '%s'\n", arg_list[DELAY]);
      printf("Delay must be 0 or greater.\n");
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
   printf("Usage: client <midi-channel> <delay> <server-machine> <server-port>\n");
}

int Client::recv_packet_into_buf(uint32_t packet_size) {
   int bytes_recv;
   bytes_recv = recv_buf(server_sock, &server, buf, MAX_BUF_SIZE);
   seq_num = midi_header->seq_num + 1;
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

   print_debug("Client::send_handshake_fin!\n");
   // Parse the handshake ack to get the seq number.
   Handshake_Packet *hs = (Handshake_Packet *)buf;
   ASSERT(hs->header.seq_num == 1);

   // Build the handshake fin packet
   hs->header.seq_num = seq_num;
   ASSERT(hs->header.seq_num == 2);
   hs->header.flag = flag::HS_FIN;

   // Send the handshake fin packet to the server.
   uint16_t packet_size = sizeof(Handshake_Packet);
   bytes_sent = send_buf(server_sock, &server, buf, packet_size);
   ASSERT(bytes_sent == packet_size);
}

void Client::queue_midi_ack(uint32_t packet_seq_num) {
   get_current_time(&current_time);

   queued_acks.push_back(std::make_pair(current_time + delay, packet_seq_num));
}

void Client::send_midi_ack(uint32_t packet_seq_num) {
   int bytes_sent;

   print_debug("sending seq_num %d\n", seq_num);

   // Get the ack of the message to send.
   midi_ack.seq_num = packet_seq_num;
   midi_ack.flag = flag::MIDI_ACK;
   uint16_t packet_size = sizeof(Handshake_Packet);
   bytes_sent = send_buf(server_sock, &server, (uint8_t *)&midi_ack, packet_size);
   ASSERT(bytes_sent == packet_size);

   // Remove the bookkeeping from the queue
   queued_acks.pop_front();
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

void Client::twiddle() {
   // Wait for packet
   if (check_for_response(0) && client_alive) {
      // Recive the packet into the buffer
      recv_packet_into_buf(MAX_BUF_SIZE);

      // Parse the packet
      flag::Packet_Flag flag;
      flag = (flag::Packet_Flag)midi_header->flag;

      print_debug("the server sent seq_num: %d\n", midi_header->seq_num);

      // This packet has to either be a handshake_fin packet or a sync_ack packet.
      switch (flag) {
         case flag::SYNC:
            queue_sync();
            break;
         case flag::MIDI:
            queue_midi_data();
            break;
         default:
            fprintf(stderr, "Client::twiddle fell through!\n");
            fprintf(stderr, "packet flag: %d\n", flag);
            ASSERT(FALSE);
            break;
      }
   }

   /*
   fprintf(stderr, "current_time: %lu\n", current_time);
   if (queued_acks.size() > 0) {
      fprintf(stderr, "queued_acks.front().first: %lu\n", queued_acks.front().first);
   }
   if (queued_events.size() > 0) {
      fprintf(stderr, "queued_events.front().first: %lu\n", queued_events.front().first);
   }
   */

   get_current_time(&current_time);
   // Check to see if we need to ack any packets
   if (queued_acks.size() > 0 && queued_acks.front().first < current_time) {

      // Send the ack
      send_midi_ack(queued_acks.front().second);
   }

   get_current_time(&current_time);
   // Check to see if we need to play any midi events
   if (queued_events.size() > 0 &&
         queued_events.front().first < current_time) {

      // Play the midi data
      play_midi_data();
   }

   get_current_time(&current_time);
   // Check to see if we need to play any midi events
   if (queued_syncs.size() > 0 && queued_syncs.front().first < current_time) {

      // Play the midi data
      send_sync_ack(queued_syncs.front().second);
   }
}

void process_midi(PtTimestamp timestamp, void *userData) {
   // Lookup the time and populate the server's clock
   ((Client *)userData)->midi_timer = Pt_Time();
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
