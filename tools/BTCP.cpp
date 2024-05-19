#include <arpa/inet.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <tuple>

#include "config/tcp_config.h"
#include "connect/b_socket.h"
#include "tun/tun.h"
#include "tun/tun_adapter.h"
#include "utils/stream_copy.h"

using namespace std;

static void show_usage(const char* argv0, const char* msg) {
    cout
        << "Usage: " << argv0 << " [options] <host> <port>\n\n"
        << "   选项                                                            "
           "默认值\n"
        << "   ----                                                            "
           "------\n\n"

        << "   -l              监听模式                            "
        << "            输入的 <host>:<port>.\n\n"

        << "   -a <addr>       设置源地址 (仅客户端)                           "
        << LOCAL_ADDRESS_DFLT << "\n"
        << "   -s <port>       设置源端口 (仅客户端)                           "
           "(随机)\n\n"

        << "   -w <winsz>      窗口大小 (bytes)                                "
        << TCPConfig::DEFAULT_CAPACITY << "\n\n"

        << "   -t <tmout>      重传超时时间                                   "
           " "
        << TCPConfig::TIMEOUT_DFLT << "\n\n"

        << "   -d <tundev>     连接到tun设备的设备号                           "
        << TUN_DFLT << "\n\n"

        << "   -Lu <loss>      上行丢包率 (0-1的小数)                        "
           "  "
           "(0)\n"
        << "   -Ld <loss>      下行丢包率 (0-1的小数)                        "
           "  "
           "(0)\n\n"

        << "   -h              显示该内容\n\n";

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

static tuple<TCPConfig, FdAdapterConfig, bool, const char*, const char*>
get_config(const span<char*>& args) {
    TCPConfig c_fsm{};
    FdAdapterConfig c_filt{};
    const char* tundev = nullptr;
    const char* file_path = nullptr;

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
            check_argc(args, curr, "ERROR: -a 未给定");
            source_address = args[curr + 1];
            curr += 2;

        } else if (strncmp("-s", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -s 未给定");
            source_port = args[curr + 1];
            curr += 2;

        } else if (strncmp("-w", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -w 未给定");
            c_fsm.recv_capacity = strtol(args[curr + 1], nullptr, 0);
            curr += 2;

        } else if (strncmp("-t", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -t 未给定");
            c_fsm.rt_timeout = strtol(args[curr + 1], nullptr, 0);
            curr += 2;
        } else if (strncmp("-d", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -d 未给定");
            tundev = args[curr + 1];
            curr += 2;
        } else if (strncmp("-f", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -f 未给定");
            file_path = args[curr + 1];
            curr += 2;
        } else if (strncmp("-Lu", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -Lu 未给定");
            const float lossrate = strtof(args[curr + 1], nullptr);
            using LossRateUpT = decltype(c_filt.loss_rate_up);
            c_filt.loss_rate_up = static_cast<LossRateUpT>(
                static_cast<float>(numeric_limits<LossRateUpT>::max()) *
                lossrate);
            curr += 2;

        } else if (strncmp("-Ld", args[curr], 3) == 0) {
            check_argc(args, curr, "ERROR: -Ld 未给定");
            const float lossrate = strtof(args[curr + 1], nullptr);
            using LossRateDnT = decltype(c_filt.loss_rate_dn);
            c_filt.loss_rate_dn = static_cast<LossRateDnT>(
                static_cast<float>(numeric_limits<LossRateDnT>::max()) *
                lossrate);
            curr += 2;

        } else if (strncmp("-h", args[curr], 3) == 0) {
            show_usage(args[0], nullptr);
            exit(0);

        } else {
            show_usage(args[0],
                       string("ERROR: 无法识别 " + string(args[curr])).c_str());
            exit(1);
        }
    }

    // parse positional command-line arguments
    if (listen) {
        c_filt.source = {"0", args[curr + 1]};
        if (c_filt.source.port() == 0) {
            show_usage(args[0], "ERROR: 服务器监听端口不能为0");
            exit(1);
        }
    } else {
        c_filt.destination = {args[curr], args[curr + 1]};
        c_filt.source = {source_address, source_port};
    }

    return make_tuple(c_fsm, c_filt, listen, tundev, file_path);
}

int main(int argc, char** argv) {
    try {
        if (argc <= 0) {
            abort();
        }

        auto args = span(argv, argc);

        if (argc < 3) {
            show_usage(args.front(), "ERROR: 输入参数不足");
            return EXIT_FAILURE;
        }

        auto [c_fsm, c_filt, listen, tun_dev_name, file_path] =
            get_config(args);
        B_TCPSocketEpoll socket(tun_dev_name == nullptr ? TUN_DFLT
                                                        : tun_dev_name);

        if (listen) {
            socket.listen_and_accept(c_fsm, c_filt);
        } else {
            socket.connect(c_fsm, c_filt);
        }

        if (file_path == nullptr) {
            bidirectional_stream_copy(socket);
        } else {
            const auto start_time = std::chrono::steady_clock::now();
            std::ifstream file(file_path);
            if (!file.is_open()) {
                std::cerr << "Failed to open file" << std::endl;
                return EXIT_FAILURE;
            }
            int input_len = 0;
            std::string line;
            while (std::getline(file, line)) {
                input_len += line.size();
                socket.write(line);
            }
            file.close();
            const auto stop_time = std::chrono::steady_clock::now();
            auto test_duration = duration_cast<std::chrono::duration<double>>(
                stop_time - start_time);
            auto bytes_per_second =
                static_cast<double>(input_len) / test_duration.count();
            auto bits_per_second = 8 * bytes_per_second;
            auto gigabits_per_second = bits_per_second / 1e9;

            cout << "传输时间 " << test_duration.count() << "s\n"
                 << "传输速度 " << gigabits_per_second << " Gbit/s\n";
        }
        socket.wait_until_closed();
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
