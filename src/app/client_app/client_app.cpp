#include "client/client.hpp"

int main(int argc, char **argv) {
   Client(argc - 1, argv + 1);
   return 0;
}
