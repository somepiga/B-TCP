#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <tuple>

#include "config/tcp_config.h"
#include "connect/b_socket.h"
#include "tun/tun.h"
#include "utils/stream_copy.h"

using namespace std;

static void show_usage(const char* argv0, const char* msg) {
  cout << "Usage: " << argv0 << " [options] <host> <port>\n\n"
       << "   Option                                                          "
          "Default\n"
       << "   --                                                              "
          "--\n\n"

       << "   -l              Server (listen) mode.                           "
          "(client mode)\n"
       << "                   In server mode, <host>:<port> is the address to "
          "bind.\n\n"

       << "   -a <addr>       Set source address (client mode only)           "
       << LOCAL_ADDRESS_DFLT << "\n"
       << "   -s <port>       Set source port (client mode only)              "
          "(random)\n\n"

       << "   -w <winsz>      Use a window of <winsz> bytes                   "
       << TCPConfig::MAX_PAYLOAD_SIZE << "\n\n"

       << "   -t <tmout>      Set rt_timeout to tmout                         "
       << TCPConfig::TIMEOUT_DFLT << "\n\n"

       << "   -d <tundev>     Connect to tun <tundev>                         "
       << TUN_DFLT << "\n\n"

       << "   -h              Show this message.\n\n";

  if (msg != nullptr) {
    cout << msg;
  }
  cout << endl;
}

static void check_argc(const span<char*>& args, size_t curr, const char* err) {
  if (curr + 3 >= args.size()) {
    show_usage(args.front(), err);
    exit(1);
  }
}

static tuple<TCPConfig, FdAdapterConfig, bool, const char*> get_config(
    const span<char*>& args) {
  TCPConfig c_fsm{};
  FdAdapterConfig c_filt{};
  const char* tundev = nullptr;

  size_t curr = 1;
  bool listen = false;
  const size_t argc = args.size();

  string source_address = LOCAL_ADDRESS_DFLT;
  string source_port = to_string(uint16_t(random_device()()));

  while (argc - curr > 2) {
    if (strncmp("-l", args[curr], 3) == 0) {
      listen = true;
      curr += 1;

    } else if (strncmp("-a", args[curr], 3) == 0) {
      check_argc(args, curr, "ERROR: -a requires one argument.");
      source_address = args[curr + 1];
      curr += 2;

    } else if (strncmp("-s", args[curr], 3) == 0) {
      check_argc(args, curr, "ERROR: -s requires one argument.");
      source_port = args[curr + 1];
      curr += 2;

    } else if (strncmp("-w", args[curr], 3) == 0) {
      check_argc(args, curr, "ERROR: -w requires one argument.");
      c_fsm.recv_capacity = strtol(args[curr + 1], nullptr, 0);
      curr += 2;

    } else if (strncmp("-t", args[curr], 3) == 0) {
      check_argc(args, curr, "ERROR: -t requires one argument.");
      c_fsm.rt_timeout = strtol(args[curr + 1], nullptr, 0);
      curr += 2;

    } else if (strncmp("-d", args[curr], 3) == 0) {
      check_argc(args, curr, "ERROR: -t requires one argument.");
      tundev = args[curr + 1];
      curr += 2;

    } else if (strncmp("-h", args[curr], 3) == 0) {
      show_usage(args[0], nullptr);
      exit(0);

    } else {
      show_usage(
          args[0],
          string("ERROR: unrecognized option " + string(args[curr])).c_str());
      exit(1);
    }
  }

  if (listen) {
    c_filt.source = {"0", args[curr + 1]};
    if (c_filt.source.port() == 0) {
      show_usage(args[0], "ERROR: listen port cannot be zero in server mode.");
      exit(1);
    }
  } else {
    c_filt.destination = {args[curr], args[curr + 1]};
    c_filt.source = {source_address, source_port};
  }

  return make_tuple(c_fsm, c_filt, listen, tundev);
}

int main(int argc, char** argv) {
  try {
    if (argc <= 0) {
      abort();
    }

    auto args = span(argv, argc);

    if (argc < 3) {
      show_usage(args.front(), "ERROR: required arguments are missing.");
      return EXIT_FAILURE;
    }

    auto [c_fsm, c_filt, listen, tun_dev_name] = get_config(args);
    B_TCPSocketEpoll socket(tun_dev_name == nullptr ? TUN_DFLT : tun_dev_name);

    if (listen) {
      socket.listen_and_accept(c_fsm, c_filt);
    } else {
      socket.connect(c_fsm, c_filt);
    }

    bidirectional_stream_copy(socket);
    socket.wait_until_closed();
  } catch (const exception& e) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
