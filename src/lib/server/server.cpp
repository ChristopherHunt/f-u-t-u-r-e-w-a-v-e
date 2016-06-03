#include <arpa/inet.h>        // htons
#include <fcntl.h>            // O_CREAT, O_TRUNC, O_WRONLY
#include <netinet/in.h>       // sockaddr_in
#include <stdio.h>            // printf
#include <stdlib.h>           // strtol, strtod, exit
#include <string.h>           // memcpy
#include <sys/socket.h>       // socket, accept, listen
#include <sys/types.h>
#include <sys/wait.h>         // wait
#include <unistd.h>           // gethostname
#include <iostream>
#include <string>
#include <sstream>
#include "network/network.hpp"
#include "server/server.hpp"

Server::Server(int num_args, char **arg_list) {
   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

   // Overlay the midi header onto the buf for easy dereferencing later.
   midi_header = (Packet_Header *)buf;
   buf_offset = 0;

   // No song is playing at startup.
   song_is_playing = false;

   // Setup main socket for the server to listen for clients on.
   setup_udp_socket();

   // Overlay a Handshake_Packet over the front of the buffer for future use.
   hs = (Handshake_Packet *)buf;

   // TODO: Remove -- this is just to test playing music locally to troubleshoot
   // packets!
   //setup_music_locally();
   //

   // Initialize all variables needed by the server.
   init();

   // Begin servicing clients.
   state = server::WAIT_FOR_INPUT;
   ready_go();
}

Server::~Server() {
}

void Server::append_to_buf(MyPmEvent *event) {
   ASSERT(event != NULL);
   ASSERT(midi_header->flag == flag::MIDI);
   event->serialize(buf, buf_offset);
   buf_offset += SIZEOF_MIDI_EVENT;
   ++midi_header->num_midi_events;
}

void Server::calc_delay(ClientInfo& client){
   std::vector<long>::iterator it;
   for (it = client.delay_times.begin(); it != client.delay_times.end(); it++) {
      client.avg_delay += *it;
   }
   client.avg_delay /= client.delay_times.size();
}

void Server::config_fd_set_for_normal_traffic() {
   // Clear initial fd_set.
   FD_ZERO(&normal_fds);

   // Add client socket fd to the FD list to listen for
   std::unordered_map<int, ClientInfo>::iterator it;
   for (it = fd_to_client_info.begin(); it != fd_to_client_info.end(); ++it) {
      FD_SET(it->second.fd, &normal_fds);
   }
}

void Server::config_fd_set_for_priority_traffic() {
   // Clear initial fd_set.
   FD_ZERO(&priority_fds);

   // Add client socket fd to the FD list to listen for
   if (priority_messages.size() > 0) {
      FD_SET(priority_messages.front().fd, &priority_fds);
   }
}

void Server::config_fd_set_for_stdin() {
   // Clear initial fd_set.
   FD_ZERO(&normal_fds);

   FD_SET(STDIN, &normal_fds);
}

void Server::config_fd_set_for_server_socket() {
   // Clear initial fd_set.
   FD_ZERO(&normal_fds);

   FD_SET(server_sock, &normal_fds);
}

int Server::connection_ready(fd_set fds) {
   set_timeval(0);

   // Just select on the world for now
   int num_fds_available = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
   ASSERT(num_fds_available >= 0);

   return num_fds_available;
}

void Server::handle_abort() {
   fprintf(stderr, "Server::handle_abort unimplemented!\n");
   exit(1);
}

void Server::handle_client_packet(int fd) {
   ASSERT(fd >= 0);
   int bytes_recv;
   long current_time;
   long rtt;
   uint32_t actual_seq_num;

   // Get the current time
   get_current_time(&current_time);

   // Get a reference to this client's info
   ClientInfo *info = &fd_to_client_info[fd];

   // Receive the message into the buffer
   bytes_recv = recv_buf(fd, &info->addr, buf, MAX_BUF_SIZE);

   // Extract the packet's seq_num
   actual_seq_num = midi_header->seq_num;
   fprintf(stderr, "Client responded with seq_num %d\n", midi_header->seq_num);
   fprintf(stderr, "expected_seq_num %d\n", info->expected_seq_num);
   
   // If the actual sequence number in the received packet is greater than the
   // expected_seq_num then we had packet loss (we will not concern
   // ourselves with reordering for now).
   if (actual_seq_num != info->expected_seq_num) {
      // Drop all of the sequence number tracking for packets that came before
      // the current packet we just received (because we assume those packets
      // are lost).
      while (info->expected_seq_num < actual_seq_num) {
         info->packet_to_send_time.erase(info->expected_seq_num);
         info->expected_seq_num += 2;
      }
   }

   // Determine the rtt for the packet.
   rtt = current_time - info->packet_to_send_time[actual_seq_num];

   // Remove this packet timing entry from the map of packets to dispatch time
   info->packet_to_send_time.erase(actual_seq_num);

   // Update the client's delay
   // FIXME: Need to make this better, right now this is just a placeholder!
   // Ideally we will try multiple versions of delay computing so we can see
   // which works best.
   info->avg_delay = rtt / 2;

   // TODO: Remove!!!!
   print_state();
   //
}

// This function handles setting up the client's timing.
void Server::handle_client_timing(ClientInfo& info) {
   fprintf(stderr, "Server::handle_client_timing()!\n");
   long current_time;
   long rtt;

   // Get the current time from the server's clock
   get_current_time(&current_time);

   // Get the difference between the current time and the previous time to
   // determine the rtt.
   rtt = current_time - info.last_msg_send_time;

   // Divide the rtt to get the one sided delay (assuming the delays are equal
   // on the way to the client and the way back).
   info.delay_times.push_back(rtt / 2);

   // If we have recv'd enough client sync messages to establish an avg. delay
   // for this client.
   if (info.delay_times.size() >= NUM_SYNC_TRIALS) {
      fprintf(stderr, "Done syncing with client %d\n", info.fd);
      // Compute the average delay for this client
      calc_delay(info);

      // Remove the client from the priority_message_queue.
      priority_messages.pop_front();

      // Clear the delay_times vector for the client
      info.delay_times.clear();

      // Add the clinet to the fd_to_client_info mapping
      fd_to_client_info[info.fd] = info;
   }
   // If we still need to do more sync trials to compute an avg. delay.
   else {
      send_sync_packet(info);
   }
}

void Server::handle_done() {
   fprintf(stderr, "Server::handle_done unimplemented!\n");
   exit(1);
}

void Server::handle_handshake() {
   fprintf(stderr, "Server::handle_handshake unimplemented!\n");
   exit(1);
}

void Server::handle_new_client() {
   fprintf(stderr, "Server::handle_new_client!\n");

   int result;
   ClientInfo info;

   // Recv message from client
   result = recv_buf(server_sock, &info.addr, buf, sizeof(Handshake_Packet));
   ASSERT(result == sizeof(Handshake_Packet));

   // Parse the handshake packet
   flag::Packet_Flag flag;
   flag = (flag::Packet_Flag)hs->header.flag;

   // Make sure the packet flag is a handshake
   ASSERT(flag == flag::HS);

   // FIXME: This doesn't work because each client will be assigned a unique
   // port to communicate with the server on. So you would need to have the
   // client send some unique id in order to know who is who.
   std::deque<ClientInfo>::iterator it;
   for (it = priority_messages.begin(); it != priority_messages.end(); ++it) {
      // If the client already has a stale entry in the priority message queue,
      // drop that stale entry.
      if (it->addr.sin_family == info.addr.sin_family &&
            it->addr.sin_port == info.addr.sin_port) {
         // Close the fd associated with this stale connection
         close(it->fd);

         // Remove the stale connection from the priority message queue
         priority_messages.erase(it);
      }
   }

   // Create a new socket to service this new client
   info.fd = socket(AF_INET, SOCK_DGRAM, 0);

   // Update the client's sequence number
   info.seq_num = ++(hs->header.seq_num);

   // Update the client's expected_seq_num
   info.expected_seq_num = info.seq_num + 1;

   // Set the new client info's timing info to zero.
   info.avg_delay = 0;
   info.last_msg_send_time = 0;

   // Add the new client info to the priority deque
   priority_messages.push_back(info);

   // Build response packet to client
   memset(buf, '\0', MAX_BUF_SIZE);
   Packet_Header *ph = (Packet_Header *)buf;
   ph->seq_num = info.seq_num;
   ph->flag = flag::HS_GOOD;

   // Send hs ack to client
   result = send_buf(info.fd, &info.addr, buf, sizeof(Packet_Header));
   ASSERT(result == sizeof(Packet_Header));

   print_state();
}

void Server::handle_normal_msg() {
   //fprintf(stderr, "Server::handle_normal_msg!\n");
   for (int i = STDERR + 1; i <= FD_SETSIZE; ++i) {
      if (FD_ISSET(i, &normal_fds)) {
         handle_client_packet(i);
      }
   }
}

void Server::handle_parse_song() {
   bool song_good;

   // Open the file to play
   file_fd = open_target_file(filename);
   if (file_fd < 0) {
      fprintf(stderr, "Midi song no good!\n");
      state = server::WAIT_FOR_INPUT;
   }
   // If the file was opened successfully.
   else {
      // Parse the midi file
      song_good = parse_midi_input();

      // If the song is good, move to the play_song state
      if (song_good) {
         state = server::PLAY_SONG;

         // Set the flag so we know a song is playing in the WAIT_FOR_INPUT
         // state.
         song_is_playing = true;

         // Start the timer with 1 millisecond resolution and creates a thread to call
         // the process_midi function every 1 millisecond.
         time_error = Pt_Start(1, &process_midi, (void *)this);
        //  fprintf(stderr, "value time_error: %d\n", time_error);
      }
      else {
         fprintf(stderr, "Midi song no good!\n");
         state = server::WAIT_FOR_INPUT;
      }
   }
}

void Server::handle_play_song() {
   MyPmEvent event;
   MyPmMessage message;

   ClientInfo *client;
   std::deque<MyPmEvent> *track_deque;

   song_is_playing = false;

   std::unordered_map<int, ClientInfo>::iterator client_it;
   std::vector<int>::iterator track_it;

   // Loop through all of the clients
   for (client_it = fd_to_client_info.begin(); client_it != fd_to_client_info.end();
         ++client_it) {

      // Loop through all of the tracks that this client is assigned
      for (track_it = client_it->second.tracks.begin();
            track_it != client_it->second.tracks.end(); ++track_it) {

         // Get the deque that corresponds to the track
         track_deque = &(track_queues[*track_it]);

         if (track_deque->size()) {
            song_is_playing = true;

            // Get the next event
            event = track_deque->front();

            // Setup the buffer to send a midi message if its time to send.
            //std::cout << "\tMidi Timer: " << midi_timer << std::endl;
            // fprintf(stderr, "value midi_timer: %d\n", midi_timer);
            if (event.timestamp <= midi_timer) {

               // Get the current client
               client = &(client_it->second);
               setup_midi_msg(client);

               // If any of the queues have events that need to be sent
               while (track_deque->size() && event.timestamp <= midi_timer) {
                  // Pull the midi message out of the PmEvent
                  memcpy(message, event.message, 3 * sizeof(uint8_t));

                  // Add this event to the buffered midi message
                  append_to_buf(&event);

                  // TODO -- this is for testing the notes locally
                  //play_music_locally(buf, (buf_offset - SIZEOF_MIDI_EVENT));
                  //

                  // Remove the first event from the queue
                  track_deque->pop_front();

                  // Get a reference to the new front event of the queue.
                  event = track_deque->front();
               }

               // Send the midi message to the client
               send_midi_msg(client);
            }
         }
      }
   }

   // Go back to waiting for input from the clients, and expect that the
   // process_midi function will be called by the PortMidi thread every 1
   // millisecond to do the sending of the packets to the clients.
   state = server::WAIT_FOR_INPUT;
}

void Server::handle_priority_msg() {
   int result;
   ClientInfo *info;
   info = &(priority_messages.front());

   result = recv_buf(info->fd, &info->addr, buf, sizeof(Packet_Header));
   ASSERT(result == sizeof(Packet_Header));

   // Update the client's info structure with the proper seq_num
   info->seq_num = ++midi_header->seq_num;

   // Update the client's expected_seq_num
   info->expected_seq_num = info->seq_num + 1;

   // Parse the packet
   flag::Packet_Flag flag;
   flag = (flag::Packet_Flag)midi_header->flag;

   // This packet has to either be a handshake_fin packet or a sync_ack packet.
   switch (flag) {
      case flag::HS_FIN:
         fprintf(stderr, "Recv'd handshake_fin!\n");
         send_sync_packet(*info);
         break;
      case flag::SYNC_ACK:
         fprintf(stderr, "Recv'd sync_ack!\n");
         handle_client_timing(*info);
         break;
      default:
         fprintf(stderr, "handle_priority_message fell through!\n");
         handle_abort();
         break;
   }
   print_state();
}

void Server::handle_song_fin() {
   fprintf(stderr, "Server::handle_song_fin unimplemented!\n");
   exit(1);
}

void Server::handle_stdin() {
   std::string user_input;
   getline(std::cin, user_input);

   std::istringstream iss(user_input);

   std::string token;
   iss >> token;

   // For now, just assign the token to the filename
   filename.assign(token);
   std::cout << "filename: " << token << std::endl;

   state = server::PARSE_SONG;
}

void Server::handle_wait_for_input() {
   int num_connections_available;

   // Select on stdin to see if the user wants to do something
   config_fd_set_for_stdin();
   num_connections_available = connection_ready(normal_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "stdin!\n");
      handle_stdin();
   }

   // Select on the server socket to see if any other clients are trying to chat
   // with us:
   config_fd_set_for_server_socket();
   num_connections_available = connection_ready(normal_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "new client!\n");
      handle_new_client();
   }

   // Select on the priority client sockets to see if they are saying anything
   config_fd_set_for_priority_traffic();
   num_connections_available = connection_ready(priority_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "priority msg!\n");
      handle_priority_msg();
   }

   // Select on the normal client sockets to see if they are saying anything
   config_fd_set_for_normal_traffic();
   num_connections_available = connection_ready(normal_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "normal msg!\n");
      handle_normal_msg();
   }

   if (song_is_playing) {
      state = server::PLAY_SONG;
   }
}

void Server::init() {
   file_fd = 0;
   next_client_id = 0;
   midi_timer = 0;
   memset(buf, '\0', MAX_BUF_SIZE);
}

int Server::open_target_file(std::string& target_filename) {
   // Sync the filesystem to get accurate state.
   sync();

   // Check if the file exists.
   return open(target_filename.c_str(), O_RDONLY, 400);
}

bool Server::parse_handshake() {
   fprintf(stderr, "Server::parse_handshake unimplemented!\n");
   exit(1);
   return true;
}

bool Server::parse_inputs(int num_args, char **arg_list) {
   if (num_args > 1) {
      printf("Improper argument count.\n");
      return false;
   }

   char *endptr;
   port = 0;

   if (num_args == 1) {
      port = (uint32_t)strtol(arg_list[0], &endptr, 10);
      if (endptr == arg_list[0]) {
         printf("Invalid port: '%s'\n", arg_list[0]);
         return false;
      }
   }

   return true;
}

bool Server::parse_midi_input(){
   // Read the midifile from disk
   midifile.read(filename.c_str());

   // If the midifile is no good, return to previous state.
   if (!midifile.status()) {
      fprintf(stderr, "Error reading midifile %s!\n", filename.c_str());
      return false;
   }

   // If nobody is around to play the song, print error message and get out.
   if (fd_to_client_info.size() == 0) {
      fprintf(stderr, "No clients connected, connect clients to the server "
            "before trying to play a song.\n");
      return false;
   }

   int num_tracks = midifile.getTrackCount();

   MidiEvent midi_event;
   MyPmEvent pmEvent;

   std::unordered_map<int, ClientInfo>::iterator client_it;
   client_it = fd_to_client_info.begin();

   // Break down the midi file by track and queue up its events.
   for (int track = 0; track < num_tracks; ++track) {
      std::deque<MyPmEvent> track_deque;

      int track_size = midifile[track].size();
      // Looping through the track, adding events to its queue
      for (int event = 0; event < track_size; ++event) {
         // Make a midi_event so we can extract the midi bytes
         midi_event = (MidiEvent)midifile[track][event];

         // Making a port midi message based off of the bytes from the
         // midi_event
         pmEvent.message[0] = midi_event[0];
         pmEvent.message[1] = midi_event[1];
         pmEvent.message[2] = midi_event[2];

         // Making a port midi event which can be sent to port midi to be played
         // This is a wrapper around the event that just carries the timestamp
         // to play the event at.
         pmEvent.timestamp = midifile.getTimeInSeconds(midi_event.tick) * 1000.0;

         // Push the pmEvent onto the deque
         track_deque.push_back(pmEvent);
      }

      // Give this track a handle to reference it later by
      track_queues[track] = track_deque;

      // Reset to front of collection if you hit the end
      if (client_it == fd_to_client_info.end()) {
         client_it = fd_to_client_info.begin();
      }

      // Assign this track's handle to the next client in round robin fashion
      client_it->second.tracks.push_back(track);

      // Increment the client iterator to the next client in the collection
      ++client_it;
   }

   return true;
}

void Server::print_state() {
   ClientInfo info;
   fprintf(stderr, "Server state:\n");
   fprintf(stderr, "\tpriority_messages:\n");
   for (int i = 0; i < priority_messages.size(); ++i) {
      info = priority_messages[i];
      fprintf(stderr, "\t\tfd:        %d\n", info.fd);
      fprintf(stderr, "\t\tseq_num to send next:   %d\n", info.seq_num);
      fprintf(stderr, "\t\texpected_seq_num to recv next:   %d\n", info.expected_seq_num);
      fprintf(stderr, "\t\tavg_delay: %lu\n", info.avg_delay);
      fprintf(stderr, "\t\tlast_send: %lu\n", info.last_msg_send_time);
      fprintf(stderr, "\t\tdelay_times.size(): %lu\n", info.delay_times.size());
      fprintf(stderr, "\n");
   }
   fprintf(stderr, "\tfd_to_client_info:\n");
   std::unordered_map<int, ClientInfo>::iterator it;
   for (it = fd_to_client_info.begin(); it != fd_to_client_info.end(); ++it) {
      info = it->second;
      fprintf(stderr, "\t\tfd:        %d\n", info.fd);
      fprintf(stderr, "\t\tseq_num to send next:   %d\n", info.seq_num);
      fprintf(stderr, "\t\texpected_seq_num to recv next:   %d\n", info.expected_seq_num);
      fprintf(stderr, "\t\tavg_delay: %lu\n", info.avg_delay);
      fprintf(stderr, "\t\tlast_send: %lu\n", info.last_msg_send_time);
      fprintf(stderr, "\n");
   }
}

void Server::print_usage() {
   printf("Usage: server [remote-port]\n");
}

int Server::send_midi_msg(ClientInfo *info) {
   ASSERT(info != NULL);
   ASSERT(midi_header->flag == flag::MIDI);

   // Get the current server wall-clock time.
   long current_time;
   get_current_time(&current_time);

   // Store the seq_num to send time mapping for this message. Note that we are
   // storing the seq_num + 1 (which is the expected seq_num for the ack to this
   // message) in the map for easy lookup later.
   info->packet_to_send_time[info->seq_num + 1] = current_time;

   // Fire off the packet
   int bytes_sent = send_buf(info->fd, &info->addr, buf, buf_offset);

   // Reset the offset into the buffer for the next message to build on.
   buf_offset = 0;

   // Increment the seq_num so we know what the next packet should go out with
   info->seq_num += 2;
   fprintf(stderr, "setting client %d seq_num to %d\n", info->fd, info->seq_num);

   return bytes_sent;
}

void Server::send_sync_packet(ClientInfo& info) {
   int result;

   // Rebuild the packet to the client
   memset(buf, '\0', MAX_BUF_SIZE);
   midi_header->seq_num = info.seq_num;
   midi_header->flag = flag::SYNC;

   // Send sync packet to client
   result = send_buf(info.fd, &info.addr, buf, sizeof(Packet_Header));
   ASSERT(result == sizeof(Packet_Header));

   // Set the send time in the ClientInfo struct
   get_current_time(&(info.last_msg_send_time));
}

void Server::setup_midi_msg(ClientInfo *info) {
   ASSERT(info != NULL);
   midi_header->seq_num = info->seq_num;
   midi_header->flag = flag::MIDI;
   midi_header->num_midi_events = 0;
   buf_offset = sizeof(Packet_Header);
}

void Server::setup_udp_socket() {
   // Create the main socket the server will listen for clients on.
   server_sock = socket(AF_INET, SOCK_DGRAM, 0);
   ASSERT(server_sock >= 0);

   local.sin_family = AF_INET;                  // IPv4
   local.sin_addr.s_addr = htonl(INADDR_ANY);   // Match any IP
   local.sin_port = htons(port);                // Set server's port

   int result = 0;
   result = ::bind(server_sock, (struct sockaddr *)&local, sizeof(sockaddr_in));

   // Check if port was already taken by another process on this box.
   if (result < 0) {
      printf("Port: %d already taken, exiting.", port);
      exit(1);
   }

   socklen_t sockaddr_in_size = sizeof(sockaddr_in);

   // Obtain port number for server.
   result = getsockname(server_sock, (struct sockaddr *)&local,
         &sockaddr_in_size);
   ASSERT(result >= 0);

   port = ntohs(local.sin_port);

   printf("Server is using port %d\n", port);
}

void Server::set_timeval(uint32_t timeout) {
   tv.tv_sec = timeout;
   tv.tv_usec = 0;
}

void process_midi(PtTimestamp timestamp, void *userData) {
   // Lookup the time and populate the server's clock
   ((Server *)userData)->midi_timer = Pt_Time();

  //  fprintf(stderr, "value pt_time: %d\n", ((Server *)userData)->midi_timer);
}

void Server::wait_for_handshake() {
   fprintf(stderr, "Server::wait_for_handshake unimplemented!\n");
   exit(1);
}

void Server::setup_music_locally() {
   Pm_Initialize();
   int default_device_id = Pm_GetDefaultOutputDeviceID();

   // Setup the output stream for playing midi.
   Pm_OpenOutput(&stream, default_device_id, NULL, 1, NULL, NULL, 0);
}

// Assumes that it is receiving a single serialized midi event
void Server::play_music_locally(uint8_t *buf, int offset) {
   PmMessage message;
   PmEvent event;

   MyPmEvent *my_event = (MyPmEvent *)(buf + offset);

   // Make a message object to wrap this midi message
   message = Pm_Message(my_event->message[0], my_event->message[1],
         my_event->message[2]);

   // Wrap the message and its timestamp in a midi event
   event.message = message;
   event.timestamp = my_event->timestamp;

   // TODO -- REMOVE
   uint8_t *ptr = (uint8_t *)my_event;
   fprintf(stderr, "Server::play_music_locally\n");
   for (int j = 0; j < SIZEOF_MIDI_EVENT; ++j) {
      fprintf(stderr, "%02x ", ptr[j]);
   }
   fprintf(stderr, "\n");
   //

   // Send this midi event to output
   Pm_Write(stream, &event, 1);
}

void Server::ready_go() {
   while (true) {
      switch (state) {
         case server::HANDSHAKE:
            handle_handshake();
            break;
         case server::WAIT_FOR_INPUT:
            handle_wait_for_input();
            break;
         case server::PARSE_SONG:
            handle_parse_song();
            break;
         case server::PLAY_SONG:
            handle_play_song();
            break;
         case server::SONG_FIN:
            handle_song_fin();
            break;
         case server::DONE:
            handle_done();
            break;
         default:
           handle_abort();
            break;
      }
   }
}
