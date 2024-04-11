#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

#include "connect/b_socket.h"

void get_URL(const std::string& host, const std::string& path) {
  B_TCPSocket socket;
  socket.connect(Address{host, "http"});
  socket.write("GET " + path + " HTTP/1.1\r\n");
  socket.write("Host: " + host + "\r\n");
  socket.write("Connection: close\r\n\r\n");
  std::string data;
  std::string datapart;
  while (!socket.eof()) {
    socket.read(datapart);
    data += datapart;
  }
  std::cout << data << std::endl;
}

int main(int argc, char* argv[]) {
  // 实现socket
  try {
    if (argc <= 0) {
      abort();
    }

    auto args = std::span(argv, argc);

    if (argc != 3) {
      std::cerr << "Usage: " << args.front() << " HOST PATH\n";
      std::cerr << "\tExample: " << args.front()
                << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    const std::string host{args[1]};
    const std::string path{args[2]};

    get_URL(host, path);
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}