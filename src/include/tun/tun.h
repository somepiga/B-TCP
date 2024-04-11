#pragma once

#include <string>

#include "datagram/file_descriptor.h"

class TunTapFD : public FileDescriptor {
 public:
  explicit TunTapFD(const std::string& devname, bool is_tun);
};

class TunFD : public TunTapFD {
 public:
  explicit TunFD(const std::string& devname) : TunTapFD(devname, true) {}
};
