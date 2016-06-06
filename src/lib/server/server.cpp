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
#include <algorithm>
#include "network/network.hpp"
#include "server/server.hpp"

Server::Server(int num_args, char **arg_list) {
   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

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
   if (client.delay_times.size() > NUM_DELAY_SAMPLES) {
      client.delay_times.pop_front();
   }

   std::deque<long>::iterator it;
   for (it = client.delay_times.begin(); it != client.delay_times.end(); it++) {
      client.avg_delay += *it;
   }
   client.avg_delay /= client.delay_times.size();
   print_debug("client %d's delay: %lu\n", client.fd, client.avg_delay);
}

void Server::config_fd_set_for_normal_traffic() {
   // Clear initial fd_set.
   FD_ZERO(&normal_fds);

   // Add client socket fd to the FD list to listen for
   std::unordered_map<int, ClientInfo>::iterator it;
   for (it = fd_to_client_info.begin(); it != fd_to_client_info.end(); ++it) {
      FD_SET(it->second.fd, &normal_fds);
   }

   // Remove the priority client from the fd_set if a priority client exists
   if (fd_to_client_info.size() > 0) {
      FD_CLR(sync_client->fd, &normal_fds);
   }
}

void Server::config_fd_set_for_priority_traffic() {
   // Clear initial fd_set.
   FD_ZERO(&priority_fds);

   // Add the current priority client to the fd_set
   if (fd_to_client_info.size() > 0) {
      FD_SET(sync_client->fd, &priority_fds);
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
   int num_fds_available = select(max_sock + 1, &fds, NULL, NULL, &tv);
   ASSERT(num_fds_available >= 0);

   return num_fds_available;
}

void Server::handle_abort() {
   fprintf(stderr, "Server::handle_abort unimplemented!\n");
   exit(1);
}

void Server::handle_client_packet(int fd) {
   /*
      ASSERT(fd >= 0);
      int bytes_recv;
      long current_time = 0;
      long rtt;
      uint32_t actual_seq_num;

   // Get the current time
   get_current_time(&current_time);

   // Get a reference to this client's info
   ClientInfo *info = &fd_to_client_info[fd];

   // Extract the packet's seq_num
   actual_seq_num = midi_header->seq_num;
   print_debug("Client responded with seq_num %d\n", midi_header->seq_num);
   print_debug("expected_seq_num %d\n", info->expected_seq_num);

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

   print_debug("info->packet_to_send_time[%d]: %lu\n", actual_seq_num,
   info->packet_to_send_time[actual_seq_num]);
   print_debug("current_time: %lu\n", current_time);

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
   */
}

// This function handles setting up the client's timing.
void Server::handle_client_timing(ClientInfo& info) {
   print_debug("Server::handle_client_timing()!\n");
   long rtt;

   // Get the current time from the server's clock
   get_current_time(&current_time);

   // Get the difference between the current time and the previous time to
   // determine the rtt.
   rtt = current_time - info.last_msg_send_time;

   // Divide the rtt to get the one sided delay (assuming the delays are equal
   // on the way to the client and the way back).
   //info.delay_times.push_back(rtt / 2);
   info.session_delay += rtt / 2;
   ++info.session_delay_counter;
   ++info.sync_counter;

   // If we have recv'd enough client sync messages to establish an avg. delay
   // for this client.
   if (info.session_delay_counter >= NUM_SYNC_TRIALS) {
      print_debug("Done syncing with client %d\n", info.fd);

      // Condense the temporary rtt average for this sync set to 1 avg value.
      info.delay_times.push_back(info.session_delay / info.sync_counter);
      info.session_delay = 0;
      info.session_delay_counter = 0;
      info.sync_counter = 0;

      // Compute the average delay for this client
      calc_delay(info);

      // If the client comes back alive, remove its tracks from other
      // clients that took over when it failed
      if (info.active == false){
         // for every track in client_to_track
         std::vector<int>::iterator it;
         std::vector<int> * tracks;
         tracks = &(client_to_track[info.fd]);
         std::unordered_map<int, ClientInfo>::iterator client_it;

         // The erase-remove idiom for the win
         for (it = tracks->begin(); it != tracks->end(); ++it) {
            // Search all other clients, remove from list
            for (client_it = fd_to_client_info.begin();
                  client_it != fd_to_client_info.end(); ++client_it) {
               // Ensure we don't remove the tracks from the returning client
               if (client_it->second.fd != info.fd) {
                  client_it->second.tracks.erase(std::remove(
                           client_it->second.tracks.begin(), client_it->second.tracks.end(),
                           *it), client_it->second.tracks.end());
               }
            }
         }

         // TODO: REMOVE
         for (client_it = fd_to_client_info.begin();
               client_it != fd_to_client_info.end(); ++client_it) {
            for (it = client_it->second.tracks.begin();
                  it != client_it->second.tracks.end(); ++it) {
              //  fprintf(stderr, "client %d: track: %d\n", client_it->second.fd,
                    //  *it);
            }
         }
         //

         // Set the client to active again
        //  fprintf(stderr, "marking client %d active: %d\n", info.fd, info.active);
         info.active = true;
      }

      // Increment sync_it and check delays of all clients as needed
      sync_next();

      // Get the next client to sync with from the sync_it. Note that we are
      // doing this because of some problematic issues surrounding a client
      // joining while the sync_it is running through the previous clients.
      print_debug("updated sync_client to %d\n", sync_client->fd);
      send_sync_packet(*sync_client);
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
   print_debug("Server::handle_new_client!\n");

   int result;
   ClientInfo info;
   print_debug("Made empty client!\n");

   // Recv message from client
   result = recv_buf(server_sock, &info.addr, buf, sizeof(Handshake_Packet));
   ASSERT(result == sizeof(Handshake_Packet));

   // Parse the handshake packet
   flag::Packet_Flag flag;
   flag = (flag::Packet_Flag)hs->header.flag;

   // Make sure the packet flag is a handshake
   ASSERT(flag == flag::HS);

   // Create a new socket to service this new client
   info.fd = socket(AF_INET, SOCK_DGRAM, 0);

   // Update max_sock
   if (info.fd > max_sock) {
      max_sock = info.fd;
   }

   // Update the client's sequence number
   info.seq_num = ++(hs->header.seq_num);

   // Update the client's expected_seq_num
   info.expected_seq_num = info.seq_num + 1;

   // Set the new client info's timing info to zero.
   get_current_time(&current_time);
   info.last_msg_send_time = current_time;
   info.avg_delay = 1000;
   info.session_delay = 0;
   info.session_delay_counter = 0;
   info.sync_counter = 0;

   // Mark the client as active
   info.active = true;

   // Build response packet to client
   memset(buf, '\0', MAX_BUF_SIZE);
   Packet_Header *ph = (Packet_Header *)buf;
   ph->seq_num = info.seq_num;
   ph->flag = flag::HS_GOOD;

   // Send hs ack to client
   result = send_buf(info.fd, &info.addr, buf, sizeof(Packet_Header));
   ASSERT(result == sizeof(Packet_Header));

   // Add the clinet to the fd_to_client_info mapping
   print_debug("assigning client %d to fd_to_client_info\n", info.fd);
   fd_to_client_info[info.fd] = info;
   print_debug("assigned client %d to fd_to_client_info\n", info.fd);

   // So we need to reset the iterator now that the underlying container
   // changed and I realize that by shoving it back to the front it could
   // "starve" some of the clients if we were flooded with connections, but
   // we won't be and this should be fine.
   sync_it = fd_to_client_info.begin();

   // If this is the first client then setup the sync_client to get the sync
   // train rolling.
   if (fd_to_client_info.size() == 1) {
      sync_client = &(sync_it->second);
      send_sync_packet(*sync_client);
   }

   print_state();
}

void Server::handle_normal_msg() {
   print_debug("Server::handle_normal_msg!\n");
   ClientInfo *info;
  //  for (int i = STDERR + 1; i <= max_sock + 1; ++i) {
  //     if (FD_ISSET(i, &normal_fds)) {
  //        // Pull out the client's info
  //        info = &(fd_to_client_info[i]);
   //
  //        // Receive the message into the buffer
  //        int bytes_recv = recv_buf(i, &info->addr, buf, MAX_BUF_SIZE);
  //        ASSERT(bytes_recv > 0);
   //
  //        // Handle its contents
  //        handle_client_packet(i);
  //     }
  //  }
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

      // Only send tracks to active clients
      print_debug("handle_play_song::client %d active %d\n",
         client_it->second.fd, client_it->second.active);
      if (client_it->second.active == true) {

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
                  while (track_deque->size() && (event.timestamp +
                           (max_client_delay - client_it->second.avg_delay)) <= midi_timer) {
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
   }

   // Go back to waiting for input from the clients, and expect that the
   // process_midi function will be called by the PortMidi thread every 1
   // millisecond to do the sending of the packets to the clients.
   state = server::WAIT_FOR_INPUT;
}

void Server::handle_priority_msg() {
   int result;
   ClientInfo *info;

   print_debug("Server::handle_priority_msg!\n");
   for (int i = STDERR + 1; i <= max_sock + 1; ++i) {
      if (FD_ISSET(i, &priority_fds)) {
         info = &(fd_to_client_info[i]);

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
               print_debug("Recv'd handshake_fin!\n");
               break;
            case flag::SYNC_ACK:
               print_debug("Recv'd sync_ack!\n");
               handle_client_timing(*info);
               break;
            case flag::MIDI_ACK:
               print_debug("Recv'd midi_ack!\n");
               handle_client_packet(i);
               break;
            default:
               fprintf(stderr, "handle_priority_message fell through!\n");
               handle_abort();
               break;
         }
      }
   }

   print_state();
}

void Server::handle_song_fin() {
   fprintf(stderr, "Server::handle_song_fin unimplemented!\n");
   exit(1);
}

void Server::handle_sync_timeout(ClientInfo *info) {
   // Increment the number of times we've tried to sync with this client
   ++info->session_delay_counter;

   // Check to see if this client has timed out fully
   if (info->session_delay_counter >= NUM_SYNC_TRIALS) {
      // Zero the client's sync bookkeeping
      info->session_delay = 0;
      info->session_delay_counter = 0;
      info->sync_counter = 0;

      if (info->active) {
        //  fprintf(stderr, "SETTING CLIENT %d to INACTIVE!\n", info->fd);
         // Mark the client as inactive
         info->active = false;

         ClientInfo *min_client;
         int min_client_tracks;
         std::vector<int>::iterator track_it;
         std::unordered_map<int, ClientInfo>::iterator client_it;

         // Redistribute the client's tracks to other active clients
         for (track_it = info->tracks.begin(); track_it != info->tracks.end();
               ++track_it) {

            // Reset to large number per iteration
            min_client_tracks = 1000;

            // Find the client with the least number of tracks
            for (client_it = fd_to_client_info.begin();
                  client_it != fd_to_client_info.end(); ++client_it) {
               // Only look at clients that are active
               if (client_it->second.active == true &&
                     client_it->second.tracks.size() < min_client_tracks) {

                  min_client_tracks = client_it->second.tracks.size();
                  min_client = &(client_it->second);
               }
            }

            // Push the current track from the inactive client onto the client with
            // the minimum number of tracks.
            print_debug("assigning track %d to client %d\n", *track_it, min_client->fd);
            print_debug("client %d tracks.size(): %d\n", info->fd, info->tracks.size());
            min_client->tracks.push_back(*track_it);
         }
         print_debug("DONESKIS!\n");
      }

      // Increment sync_it and check delays of all clients as needed
      sync_next();
   }
   // Send a new sync packet
   send_sync_packet(*sync_client);
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
      print_debug("stdin!\n");
      handle_stdin();
   }

   // Select on the server socket to see if any other clients are trying to chat
   // with us:
   config_fd_set_for_server_socket();
   num_connections_available = connection_ready(normal_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      // fprintf(stderr, "new client!\n");
      handle_new_client();
   }

   // Select on the priority client sockets to see if they are saying anything
   config_fd_set_for_priority_traffic();
   num_connections_available = connection_ready(priority_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      handle_priority_msg();
   }
   else {
      // Check the timeout on the current syncing client and act apprioriately
      get_current_time(&current_time);
      if (sync_client != NULL && sync_client->last_msg_send_time +
            MAX_SYNC_TIMEOUT * sync_client->avg_delay < current_time) {

        //  fprintf(stderr, "current_time: %lu\n", current_time);
        //  fprintf(stderr, "sync_client %d last_msg_send_time: %lu\n", sync_client->fd, sync_client->last_msg_send_time);
        //  fprintf(stderr, "sync_client %d avg_delay: %lu\n", sync_client->fd, sync_client->avg_delay);
        //  fprintf(stderr, "sync_cilent %d active: %d\n", sync_client->fd, sync_client->active);
         handle_sync_timeout(sync_client);
      }
   }

   // Select on the normal client sockets to see if they are saying anything
   config_fd_set_for_normal_traffic();
   num_connections_available = connection_ready(normal_fds);
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      handle_normal_msg();
   }

   // If the song is playing, fall into the play_song function to send more
   // notes to the clients.
   if (song_is_playing) {
      state = server::PLAY_SONG;
   }
}

void Server::init() {
   file_fd = 0;
   next_client_id = 0;
   midi_timer = 0;
   memset(buf, '\0', MAX_BUF_SIZE);

   // Overlay the midi header onto the buf for easy dereferencing later.
   midi_header = (Packet_Header *)buf;
   buf_offset = 0;

   // No song is playing at startup.
   song_is_playing = false;

   // Set the initial sync_client pointer to NULL
   sync_client = NULL;

   // Setup main socket for the server to listen for clients on.
   setup_udp_socket();

   // Overlay a Handshake_Packet over the front of the buffer for future use.
   hs = (Handshake_Packet *)buf;

   // Set the sync iterator to the front of the empty clients map
   sync_it = fd_to_client_info.begin();
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

      // Add track to appropriate client
      client_to_track[client_it->second.fd].push_back(track);

      // Increment the client iterator to the next client in the collection
      ++client_it;
   }

   return true;
}

void Server::print_state() {
   ClientInfo info;
   print_debug("Server state:\n");
   print_debug("\tfd_to_client_info:\n");
   std::unordered_map<int, ClientInfo>::iterator it;
   for (it = fd_to_client_info.begin(); it != fd_to_client_info.end(); ++it) {
      info = it->second;
      print_debug("\t\tfd:        %d\n", info.fd);
      print_debug("\t\tseq_num to send next:   %d\n", info.seq_num);
      print_debug("\t\texpected_seq_num to recv next:   %d\n", info.expected_seq_num);
      print_debug("\t\tavg_delay: %lu\n", info.avg_delay);
      print_debug("\t\tlast_send: %lu\n", info.last_msg_send_time);
      print_debug("\n");
   }
}

void Server::print_usage() {
   printf("Usage: server [remote-port]\n");
}

int Server::send_midi_msg(ClientInfo *info) {
   ASSERT(info != NULL);
   ASSERT(midi_header->flag == flag::MIDI);

   // Get the current server wall-clock time.
   get_current_time(&current_time);

   // Store the seq_num to send time mapping for this message. Note that we are
   // storing the seq_num + 1 (which is the expected seq_num for the ack to this
   // message) in the map for easy lookup later.
   //info->packet_to_send_time[info->seq_num + 1] = current_time;

   // Fire off the packet
   int bytes_sent = send_buf(info->fd, &info->addr, buf, buf_offset);

   // Reset the offset into the buffer for the next message to build on.
   buf_offset = 0;

   // Increment the seq_num so we know what the next packet should go out with
   info->seq_num += 2;
   print_debug("setting client %d seq_num to %d\n", info->fd, info->seq_num);

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

   max_sock = server_sock;

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

void Server::sync_next() {
   // Move the sync_it to the next client and sync
   ++sync_it;
   if (sync_it == fd_to_client_info.end()) {
      max_client_delay = 0;

      // Update the max_client_delay to be the maximum delay amongst clients
      // in the network.
      for (sync_it = fd_to_client_info.begin();
            sync_it != fd_to_client_info.end(); ++sync_it) {
         if (sync_it->second.avg_delay > max_client_delay) {
            max_client_delay = sync_it->second.avg_delay;
         }
      }
      get_current_time(&current_time);
      fprintf(stderr, "%lu, %lu\n", current_time, max_client_delay);
      // fprintf(stderr, "max_client_delay: %lu\n", max_client_delay);
      print_debug("max_client_delay: %lu\n", max_client_delay);

      // Reset the iterator to the front of the list
      sync_it = fd_to_client_info.begin();
   }

   sync_client = &(sync_it->second);
  //  fprintf(stderr, "sync_client: %d\n", sync_client->fd);
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
   print_debug("Server::play_music_locally\n");
   for (int j = 0; j < SIZEOF_MIDI_EVENT; ++j) {
      print_debug("%02x ", ptr[j]);
   }
   print_debug("\n");
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
