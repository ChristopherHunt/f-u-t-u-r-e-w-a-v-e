#include "server/server.hpp"

int main(int argc, char **argv) {
   Server(argc - 1, argv + 1);
   return 0;
}
