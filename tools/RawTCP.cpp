#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>

#include "connect/raw_tcpsocket.h"
#include "utils/stream_copy.h"

using namespace std;

void show_usage(const char* argv0) {
    cerr << "Usage: " << argv0 << " [-l] <host> <port>\n\n";
}

int main(int argc, char** argv) {
    try {
        if (argc <= 0) {
            abort();
        }

        auto args = span(argv, argc);

        bool server_mode = true;
        bool file_mode = false;
        // NOLINTNEXTLINE(bugprone-assignment-*)
        if (argc < 3) {
            show_usage(args[0]);
            return EXIT_FAILURE;
        }

        if (strncmp("-ld", args[1], 4) == 0) {
            file_mode = true;
        }

        auto socket = [&] {
            if (server_mode) {
                RawTCPSocket listening_socket;
                listening_socket.set_reuseaddr();
                listening_socket.bind({args[2], args[3]});
                listening_socket.listen();
                return listening_socket.accept();
            }
            RawTCPSocket connecting_socket;
            connecting_socket.connect({args[1], args[2]});
            return connecting_socket;
        }();

        bidirectional_stream_copy(socket);
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
