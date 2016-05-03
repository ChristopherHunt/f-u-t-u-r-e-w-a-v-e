#ifndef __NETWORK__HEADER__H__
#define __NETWORK__HEADER__H__

#include <stdint.h>
#include <string>
#include <netdb.h>   // sockaddr_in

#define FALSE 0
#define TRUE 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define MAX_BUF_SIZE 32768

enum Packet_Flag { MIDI, ACK, SONG_START, SONG_FIN, HS, HS_GOOD, HS_FAIL,
   CLOCK_SYNC };

#define ASSERT(expression) {\
   if (!(expression)) {\
      perror("\n!!! ASSERT FAILED !!!\n\tError ");\
      fprintf(stderr, "\tFile : \"%s\"\n\tFunction : \"%s\"\n\t"\
            "Line : %d\n\n", __FILE__, __func__, __LINE__);\
      exit(1);\
   }\
}

#define EXCEPT(expression, todo) {\
   if (!(expression)) {\
      perror("\n!!! ASSERT FAILED !!!\n\tError ");\
      fprintf(stderr, "\tFile : \"%s\"\n\tFunction : \"%s\"\n\t"\
            "Line : %d\n\n", __FILE__, __func__, __LINE__);\
      todo;\
   }\
}

typedef struct Packet_Header {
   uint32_t seq_num;
   uint8_t flag;
} __attribute__((packed)) Packet_Header;

typedef struct Midi_Header {
   Packet_Header header;
   uint8_t num_midi_events;
} __attribute__((packed)) Midi_Header;

typedef struct Handeshake_Packet {
   Packet_Header header;
} __attribute__((packed)) Handshake_Packet;

int send_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len);

int recv_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len);

#endif
