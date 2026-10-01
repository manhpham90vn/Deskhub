#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/ClipboardSync.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

std::vector<std::vector<uint8_t>> CollectDatagrams(ClipboardSync& sync, uint64_t nowUs) {
    std::vector<std::vector<uint8_t>> out;
    sync.Flush(nowUs, [&out](std::span<const uint8_t> d) {
        out.emplace_back(d.begin(), d.end());
    });
    return out;
}

std::optional<ClipboardChunkView> ChunkOf(const std::vector<uint8_t>& datagram) {
    return ParseClipboardChunk(PayloadOf(datagram));
}

void TestShortTextTravelsInOneChunk() {
    std::printf("[clip] a short copy travels as one datagram and is applied once...\n");
    ClipboardSync sender, receiver;
    sender.SetSessionId(7);
    Check(sender.OfferLocal("hello viewer"), "new text is accepted for sending");

    const auto sent = CollectDatagrams(sender, 1'000);
    Check(sent.size() == 1, "one chunk suffices for short text");
    const auto chunk = ChunkOf(sent[0]);
    Check(chunk.has_value(), "the chunk parses back");
    Check(chunk->chunkCount == 1 && chunk->chunkIndex == 0, "and says it is the only one");

    Check(receiver.Accept(*chunk), "the receiver completes on the single chunk");
    const auto text = receiver.TakeCompleted();
    Check(text.has_value() && *text == "hello viewer", "the text survives the trip");
    Check(!receiver.TakeCompleted().has_value(), "a completed text is taken only once");
}

void TestRedundantResendsAreDeduped() {
    std::printf("[clip] the redundant resends never apply the same copy twice...\n");
    ClipboardSync sender, receiver;
    Check(sender.OfferLocal("once"), "offer");

    size_t applied = 0;
    uint64_t now = 0;
    for (int round = 0; round < 5; ++round) {
        now += kClipboardResendIntervalUs;
        for (const auto& d : CollectDatagrams(sender, now))
            if (receiver.Accept(*ChunkOf(d))) ++applied;
    }
    Check(applied == 1, "three sends, one application");
}

void TestChunkedTextSurvivesLossAndReorder() {
    std::printf("[clip] a long copy is chunked and survives loss + reordering...\n");
    std::string longText;
    while (longText.size() < kMaxClipboardChunkPayload * 2 + 100) longText += "chunky ";

    ClipboardSync sender, receiver;
    Check(sender.OfferLocal(longText), "offer long text");

    auto first = CollectDatagrams(sender, 1'000);
    Check(first.size() == 3, "the text needs three chunks");

    Check(!receiver.Accept(*ChunkOf(first[2])), "the last chunk alone is not enough");
    Check(!receiver.Accept(*ChunkOf(first[0])), "nor two of three");
    Check(!receiver.Accept(*ChunkOf(first[0])), "a duplicate chunk changes nothing");

    const auto resend = CollectDatagrams(sender, 1'000 + kClipboardResendIntervalUs);
    Check(resend.size() == 3, "the resend carries all chunks again");
    Check(receiver.Accept(*ChunkOf(resend[1])), "the missing middle chunk completes it");
    const auto text = receiver.TakeCompleted();
    Check(text.has_value() && *text == longText, "the reassembled text matches");
}

void TestEchoIsSuppressed() {
    std::printf("[clip] applying a remote copy never bounces it back...\n");
    ClipboardSync a, b;
    Check(a.OfferLocal("ping"), "A sends its clipboard");
    for (const auto& d : CollectDatagrams(a, 1'000)) b.Accept(*ChunkOf(d));
    const auto text = b.TakeCompleted();
    Check(text.has_value() && *text == "ping", "B applied it");

    Check(!b.OfferLocal("ping"), "B's poller re-reading the applied text does not resend it");
    Check(!a.OfferLocal("ping"), "and A does not resend what it already sent");
    Check(b.OfferLocal("pong"), "genuinely new text still goes out");
}

void TestOversizeIsCutOnUtf8Boundary() {
    std::printf("[clip] an oversized copy is cut at a UTF-8 boundary...\n");
    std::string big;
    while (big.size() < kMaxClipboardTextBytes + 30) big += "\xE1\xBA\xA1";
    ClipboardSync sender;
    Check(sender.OfferLocal(big), "the oversized text is still offered");

    ClipboardSync receiver;
    for (const auto& d : CollectDatagrams(sender, 1'000)) receiver.Accept(*ChunkOf(d));
    const auto text = receiver.TakeCompleted();
    Check(text.has_value(), "the bounded text arrives");
    Check(text->size() <= kMaxClipboardTextBytes && text->size() % 3 == 0,
        "cut to the limit on a whole character");
    Check(sender.OfferLocal(big) == false,
        "re-reading the same oversized clipboard does not resend");
}

void TestEmptyAndStaleInputAreIgnored() {
    std::printf("[clip] empty text and stale revisions do nothing...\n");
    ClipboardSync sender, receiver;
    Check(!sender.OfferLocal(""), "empty text is never sent");

    Check(sender.OfferLocal("first"), "send first");
    std::vector<std::vector<uint8_t>> first = CollectDatagrams(sender, 1'000);
    Check(sender.OfferLocal("second"), "send second");
    for (const auto& d : CollectDatagrams(sender, 500'000)) receiver.Accept(*ChunkOf(d));
    Check(receiver.TakeCompleted().value_or("") == "second", "second applied");
    Check(!receiver.Accept(*ChunkOf(first[0])), "a late duplicate of first is stale");
    Check(!receiver.TakeCompleted().has_value(), "and produces nothing");
}

void TestARestartedSenderIsHeardAgain() {
    std::printf("[clip] a sender that starts counting again is heard once the session is new...\n");
    ClipboardSync before, receiver;
    for (const char* text : {"one", "two", "three"}) {
        before.OfferLocal(text);
        for (const auto& d : CollectDatagrams(before, 1'000)) receiver.Accept(*ChunkOf(d));
        receiver.TakeCompleted();
    }

    ClipboardSync restarted;
    restarted.OfferLocal("fresh");
    const auto first = CollectDatagrams(restarted, 1'000);
    Check(!receiver.Accept(*ChunkOf(first[0])),
        "within one session a lower revision is still stale");
    receiver.ForgetReceived();
    Check(receiver.Accept(*ChunkOf(first[0])) &&
              receiver.TakeCompleted().value_or("") == "fresh",
        "after the receiver forgets, the restarted sender's first copy lands");
}

}

void RunClipboardSyncTests() {
    TestShortTextTravelsInOneChunk();
    TestRedundantResendsAreDeduped();
    TestChunkedTextSurvivesLossAndReorder();
    TestEchoIsSuppressed();
    TestOversizeIsCutOnUtf8Boundary();
    TestEmptyAndStaleInputAreIgnored();
    TestARestartedSenderIsHeardAgain();
}
