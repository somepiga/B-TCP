#pragma once

#include "buffer.h"
#include "ethernet_header.h"
#include "parser.h"

#include <vector>

struct EthernetFrame {
    EthernetHeader header{};
    std::vector<Buffer> payload{};

    void parse(Parser& parser) {
        header.parse(parser);
        parser.all_remaining(payload);
    }

    void serialize(Serializer& serializer) const {
        header.serialize(serializer);
        serializer.buffer(payload);
    }
};
