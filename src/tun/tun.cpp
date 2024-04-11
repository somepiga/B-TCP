#include "tun/tun.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include <cstring>

#include "utils/exception.h"

static constexpr const char* CLONEDEV = "/dev/net/tun";

using namespace std;

TunTapFD::TunTapFD(const string& devname, const bool is_tun)
    : FileDescriptor(
          ::CheckSystemCall("open", open(CLONEDEV, O_RDWR | O_CLOEXEC))) {
  struct ifreq tun_req {};

  tun_req.ifr_flags = static_cast<int16_t>((is_tun ? IFF_TUN : IFF_TAP) |
                                           IFF_NO_PI);  // no packetinfo

  // copy devname to ifr_name, making sure to null terminate

  strncpy(static_cast<char*>(tun_req.ifr_name), devname.data(), IFNAMSIZ - 1);
  tun_req.ifr_name[IFNAMSIZ - 1] = '\0';

  CheckSystemCall("ioctl",
                  ioctl(fd_num(), TUNSETIFF, static_cast<void*>(&tun_req)));
}
