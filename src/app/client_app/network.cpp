#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>       
#include <stdio.h>
#include "network/network.hpp"

int send_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len) {
   socklen_t sockaddr_in_len = sizeof(sockaddr_in);
   return sendtoErr(sock, buf, buf_len, 0, (const sockaddr*)remote,
         sockaddr_in_len); 
}

int recv_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len) {
   socklen_t sockaddr_in_len = sizeof(sockaddr_in);
   return recvfromErr(sock, buf, buf_len, 0, (struct sockaddr*)remote,
         &sockaddr_in_len); 
}
