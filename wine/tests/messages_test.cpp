#include "DongleMessages.hpp"
#include "DongleIdentity.hpp"
#include <cstdlib>
#include <iostream>
using namespace dongle_recovered;
int main() {
    const char* profiles[] = {"Dealer", "OEM", "Factory"};
    const unsigned checksums[] = {0x979d, 0x1a3e, 0x615f};
    for (unsigned i = 0; i < 3; ++i) {
        auto profile = parse_dongle_profile(profiles[i]);
        if (!profile) return 3;
        const auto identity = dongle_identity(*profile);
        if (identity[6] != i + 1 || identity.back() != checksums[i] ||
            identity_crc(identity) != checksums[i]) return 4;
    }
    for (const char* invalid : {"", "factory", "OEMx", "Therapist"})
        if (parse_dongle_profile(invalid)) return 5;
    for (bool extended : {false, true}) for (bool remote : {false, true}) {
        CCANMsg msg;
        msg.SetID(extended ? 0x18300f06 : 0x790);
        msg.SetExtended(extended);
        msg.SetRemote(remote);
        msg.SetDLC(remote ? 0 : 8);
        for (int i=0; i<8; ++i) msg[i] = i+1;
        CRawMsg raw(msg);
        CCANMsg decoded(raw);
        if (decoded.GetID()!=msg.GetID() || decoded.IsExtended()!=extended ||
            decoded.IsRemote()!=remote || decoded.GetDLC()!=msg.GetDLC()) return 1;
    }
    CRawMsg invalid;
    invalid[0]=1; invalid[13]=15;
    CCANMsg decoded;
    if (decoded.Convert(invalid)) return 2;
    std::cout << "CAN ID, DLC, EXT and RTR conversion passed\n";
}
