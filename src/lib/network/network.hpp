#ifndef __NETWORK__HEADER__H__
#define __NETWORK__HEADER__H__

#include <netdb.h>   // sockaddr_in
#include <stdint.h>
#include <string.h>
#include <sstream>

#define FALSE 0
#define TRUE 1

#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define SIZEOF_MIDI_EVENT (3 * sizeof(uint8_t) + sizeof(uint32_t))
#define MAX_BUF_SIZE 2048

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

namespace flag {
   enum Packet_Flag { BLANK, MIDI, MIDI_ACK, SONG_START, SONG_FIN, HS, HS_GOOD,
      HS_FAIL, HS_FIN, SYNC, SYNC_ACK };
};

typedef uint8_t MyPmMessage[3];

typedef struct MyPmEvent {
   MyPmMessage message;
   uint32_t timestamp;

   MyPmEvent() {};

   MyPmEvent(const MyPmEvent& other) {
      message[0] = other.message[0];
      message[1] = other.message[1];
      message[2] = other.message[2];
      timestamp = other.timestamp;
   }

   void serialize(uint8_t *buf, uint64_t offset) {
      buf[offset++] = message[0];
      buf[offset++] = message[1];
      buf[offset++] = message[2];
      memcpy(buf + offset, &timestamp, sizeof(uint32_t));
   }
} __attribute__((packed)) MyPmEvent;

typedef struct Packet_Header {
   uint32_t seq_num;
   uint8_t flag;
   uint8_t num_midi_events;
} __attribute__((packed)) Packet_Header;

typedef struct Handeshake_Packet {
   Packet_Header header;
} __attribute__((packed)) Handshake_Packet;

int send_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len);

int recv_buf(int sock, sockaddr_in *remote, uint8_t *buf, uint32_t buf_len);

void get_current_time(long *milliseconds);

void print_debug(const char *format, ...);

#endif
