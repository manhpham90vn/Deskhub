#include "deskhub/session/host/SourceListResponder.h"

namespace deskhub {

size_t SourceListResponder::Reply(std::span<uint8_t> out, std::span<const uint8_t> pkt, bool trusted) const {
    if (!trusted) return 0;
    const auto h = ParseCommonHeader(pkt);
    if (!h) return 0;
    const auto payload = PayloadOf(pkt);

    switch (h->type) {
        case MsgType::ListSources: {
            return BuildSourceList(out, sources_, caps_, hostName_);
        }
        case MsgType::Ping: {
            if (h->sessionId != 0) return 0;
            const auto m = ParsePingPong(payload);
            if (!m) return 0;
            return BuildPong(out, 0, *m);
        }
        default:
            return 0;
    }
}

}
