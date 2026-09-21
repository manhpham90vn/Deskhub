#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <span>

using namespace deskhub;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::span<const uint8_t> d(data, size);
    const auto h = ParseCommonHeader(d);
    const auto pl = PayloadOf(d);

    ParseHello(pl);
    ParseHelloAck(pl);
    ParsePingPong(pl);
    ParseFeedback(pl);
    ParseReconfig(pl);
    ParseSetFocus(pl);
    ParseInvalidateRef(pl);
    ParseListSourcesPasscode(pl);

    SourceInfo sources[kMaxSources];
    ParseSourceList(pl, sources);

    uint32_t frameId = 0;
    uint16_t indices[kMaxNackIndices];
    ParseNack(pl, frameId, indices);

    uint32_t firstSeq = 0;
    InputEvent events[kMaxInputEvents];
    ParseInputEvents(pl, firstSeq, events);

    ParseAudioPacket(pl);
    ParseClipboardChunk(pl);

    ParseTermOpen(pl);
    ParseTermOpenAck(pl);
    ParseTermResize(pl);
    ParseTermExit(pl);
    ParseTermListAck(pl);

    ClassifyPacket(d);
    for (std::span<const uint8_t> rest = d; !rest.empty();) {
        const RecordView record = ReadRecord(rest);
        if (record.status != RecordStatus::Ok) break;
        ParseCommonHeader(record.message);
        rest = rest.subspan(record.consumed);
    }

    if (h) {
        ParseVideoPacket(*h, pl);
        ParseFecPacket(*h, pl);
    }
    return 0;
}
