#pragma once
#include "pe_image.hpp"
#include <optional>
namespace rnet {
// Offline reassembly of the 0x1e46/0x1e47 family observed in the supplied 2017
// captures. Sequence numbers occupy CAN-ID bits 15..0; bit 16 marks the final
// frame. All DLC bytes are data. This is not a general POP decoder or transport.
class CapturedBlockAssembler {
    Bytes payload_;
    std::size_t limit_;
    std::uint32_t expected_=1;
    bool active_=false;
    [[noreturn]] void reject(const char* message);
public:
    explicit CapturedBlockAssembler(std::size_t limit=1024*1024):limit_(limit){}
    std::optional<Bytes> push(std::uint32_t can_id,const Bytes& data);
    void finish()const;
};
}
