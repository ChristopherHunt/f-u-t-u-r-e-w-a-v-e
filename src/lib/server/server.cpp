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

enum parse_args {ERROR_PERCENT, PORT};

Server::Server(int num_args, char **arg_list) {
   // Ensure that command line arguments are good.
   if (!parse_inputs(num_args, arg_list)) {
      print_usage();
      exit(1);
   }

   // Setup main socket for the server to listen for clients on.
   setup_udp_socket();

   // Begin servicing clients.
   state = server::WAIT_FOR_INPUT;
   ready_go();
}

Server::~Server() {
}

void Server::config_fd_set_for_stdin() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   FD_SET(STDIN, &rdfds);
}

void Server::config_fd_set_for_server_socket() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   FD_SET(server_sock, &rdfds);
}

void Server::config_fd_set_for_client_sockets() {
   // Clear initial fd_set.
   FD_ZERO(&rdfds);

   // Add client socket fd to the FD list to listen for
   std::unordered_map<int, ClientInfo>::iterator it;
   for (it = id_to_client_info.begin(); it != id_to_client_info.end(); ++it) {
      std::cout << "adding fd: " << it->second.fd << std::endl;
      FD_SET(it->second.fd, &rdfds);
   }
}

int Server::new_connection_ready() {
   // Just select on the world for now
   int num_fds_available = select(FD_SETSIZE, &rdfds, NULL, NULL, &tv);
   ASSERT(num_fds_available >= 0);

   return num_fds_available;
}

void Server::set_timeval(uint32_t timeout) {
   tv.tv_sec = timeout;
   tv.tv_usec = 0;
}

void Server::setup_udp_socket() {
   // Create the main socket the server will listen for clients on.
   server_sock = socket(AF_INET, SOCK_DGRAM, 0);
   ASSERT(server_sock >= 0);

   local.sin_family = AF_INET;                  // IPv4
   local.sin_addr.s_addr = htonl(INADDR_ANY);   // Match any IP
   local.sin_port = htons(port);                // Set server's port

   int result = 0;
   result = bind(server_sock, (struct sockaddr *)&local, sizeof(sockaddr_in));

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

bool Server::parse_inputs(int num_args, char **arg_list) {
   if (num_args != 1 && num_args != 2) {
      printf("Improper argument count.\n");
      return false;
   }

   char *endptr;

   error_percent = strtod(arg_list[ERROR_PERCENT], &endptr);
   if (endptr == arg_list[ERROR_PERCENT] || error_percent < 0 || error_percent > 1) {
      printf("Invalid error percent: '%s'\n", arg_list[ERROR_PERCENT]);
      printf("Error percent must be between 0 and 1 inclusive.\n");
      return false; 
   }

   if (num_args == 2) {
      port = (uint32_t)strtol(arg_list[PORT], &endptr, 10);
      if (endptr == arg_list[PORT]) {
         printf("Invalid port: '%s'\n", arg_list[PORT]);
         return false; 
      }
   }
   else {
      port = 0;
   }

   return true;
}

void Server::print_usage() {
   printf("Usage: server <error-rate> [remote-port]\n");
}


void Server::handle_wait_for_input() {
   int num_connections_available;
   //fprintf(stderr, "handle_wait_for_input!\n");

   // Select on stdin to see if the user wants to do something
   config_fd_set_for_stdin();
   num_connections_available = new_connection_ready();
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "stdin!\n");
      handle_stdin();
   }

   // Select on the server socket to see if any other clients are trying to chat
   // with us:
   config_fd_set_for_server_socket();
   num_connections_available = new_connection_ready();
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "new client!\n");
      handle_new_client();
   }

   // Select on the client sockets to see if they are saying anything
   config_fd_set_for_client_sockets();
   num_connections_available = new_connection_ready();
   ASSERT(num_connections_available >= 0);
   if (num_connections_available) {
      fprintf(stderr, "client msg!\n");
      handle_client_msg();
   }
}

// TODO: Make this actually parse the song, right now it is just failing open so
// we can continue to test the remainder of the system.
bool Server::parse_midi_input(){
   fprintf(stderr, "Server:parse_midi_input unimplemented!\n");
   // if successful parse of midi song
   // state = server::PLAY_SONG
   // else 
   // state = server::WAIT_FOR_INPUT
   return true;
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

void Server::handle_new_client() {
   fprintf(stderr, "Server::handle_new_client unimplemented!\n");
   exit(1);
}

void Server::handle_client_msg() {
   fprintf(stderr, "Server::handle_client_msg unimplemented!\n");
   exit(1);
}

void Server::handle_parse_song() {
   fprintf(stderr, "Server::handle_parse_song!\n");
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
      }
      else {
         fprintf(stderr, "Midi song no good!\n");
         state = server::WAIT_FOR_INPUT;
      }
   }
} 

void Server::handle_play_song() {
   fprintf(stderr, "Server::handle_play_song!\n");

   // Send each client a song_start packet
   // Get their ack
   exit(1);
}

void Server::handle_song_fin() {
   fprintf(stderr, "Server::handle_song_fin unimplemented!\n");
   exit(1);
}

void Server::handle_abort() {
   fprintf(stderr, "Server::handle_abort unimplemented!\n");
   exit(1);
}

void Server::handle_done() {
   fprintf(stderr, "Server::handle_done unimplemented!\n");
   exit(1);
}

void Server::handle_handshake() {
   fprintf(stderr, "Server::handle_handshake unimplemented!\n");
   exit(1);
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

void Server::wait_for_handshake() {
   fprintf(stderr, "Server::wait_for_handshake unimplemented!\n");
   exit(1);
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
