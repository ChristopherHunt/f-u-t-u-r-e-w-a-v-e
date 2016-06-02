#include <chrono>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>       
#include <sys/time.h>
#include <stdio.h>
#include "network/network.hpp"

int send_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len) {
   socklen_t sockaddr_in_len = sizeof(sockaddr_in);
   fprintf(stderr, "\nnetwork::sendto: \n");
   for (int i = 0; i < MAX_BUF_SIZE; ++i) {
      fprintf(stderr, "%02x ", *(buf + i));
   }
   uint32_t bytes = sendto(sock, buf, buf_len, 0, (const sockaddr*)remote,
         sockaddr_in_len); 
   fprintf(stderr, "\nnetwork::send_buf sent %d bytes\n", bytes);
   return bytes;
}

int recv_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len) {
   socklen_t sockaddr_in_len = sizeof(sockaddr_in);
   uint32_t bytes = recvfrom(sock, buf, buf_len, 0, (struct sockaddr*)remote,
         &sockaddr_in_len); 
   fprintf(stderr, "\nnetwork::recv_buf recv'd %d bytes\n", bytes);
   fprintf(stderr, "network::recv_buf: \n");
   for (int i = 0; i < MAX_BUF_SIZE; ++i) {
      fprintf(stderr, "%02x ", *(buf + i));
   }
   fprintf(stderr, "\n");
   return bytes;
} 

void get_current_time(long *milliseconds) {
   // Get the current time.
   struct timeval tp;
   gettimeofday(&tp, NULL);
   *milliseconds = tp.tv_sec * 1000 + tp.tv_usec / 1000;
}
