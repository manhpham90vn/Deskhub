#include "Perf.h"
#include "PerfHarness.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/FileReceiver.h"
#include "deskhub/session/client/FileSender.h"
#include "deskhub/transfer/Crc32.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

using namespace deskhub;
using namespace deskhub::perf;

namespace {

constexpr size_t kCrcChunkBytes = 64 * 1024;
constexpr size_t kCrcTotalBytes = 4 * 1024 * 1024;
constexpr uint64_t kBatchBytes = 4 * 1024 * 1024;
constexpr uint64_t kSmallBatchBytes = 1024 * 1024;
constexpr size_t kChunksPerPump = 16;

size_t ChunksFor(uint64_t bytes) {
    return size_t((bytes + kMaxFileChunkBytes - 1) / kMaxFileChunkBytes);
}

class MessageQueue {
public:
    void Push(std::span<const uint8_t> message) {
        if (tail_ == slots_.size()) slots_.emplace_back();
        slots_[tail_].assign(message.begin(), message.end());
        ++tail_;
    }

    bool Empty() const {
        return head_ == tail_;
    }

    const std::vector<uint8_t>& Pop() {
        return slots_[head_++];
    }

    void Rewind() {
        head_ = 0;
        tail_ = 0;
    }

private:
    std::vector<std::vector<uint8_t>> slots_{};
    size_t head_ = 0;
    size_t tail_ = 0;
};

struct TransferRig {
    std::vector<uint8_t> source{};
    MessageQueue toReceiver{};
    MessageQueue toSender{};
    uint32_t batchId = 0;
    FileSender sender;
    FileReceiver receiver;

    explicit TransferRig(size_t sourceBytes)
        : source(sourceBytes),
          sender(FileSenderCallbacks{
              [this](std::span<const uint8_t> message) {
                  toReceiver.Push(message);
                  return true;
              },
              [this](uint16_t, uint64_t offset, std::span<uint8_t> out) -> size_t {
                  if (offset >= source.size()) return 0;
                  const size_t take = std::min(out.size(), source.size() - size_t(offset));
                  std::memcpy(out.data(), source.data() + offset, take);
                  return take;
              },
              {}, {}}),
          receiver(FileReceiverCallbacks{
              [this](std::span<const uint8_t> message) { toSender.Push(message); },
              [](uint16_t, const std::string& safeName, uint64_t) { return safeName; },
              [](uint16_t, std::span<const uint8_t> data) {
                  Consume(data.size());
                  return true;
              },
              [](uint16_t, bool) { return true; }, {}, {}}) {
        FillRandom(source);
        receiver.SetAccepting(true);
    }

    void Settle() {
        for (;;) {
            if (!toReceiver.Empty()) {
                receiver.HandleMessage(toReceiver.Pop());
                continue;
            }
            if (!toSender.Empty()) {
                sender.HandleMessage(toSender.Pop());
                continue;
            }
            if (sender.State() == FileSenderState::Sending && sender.Pump(kChunksPerPump) != 0)
                continue;
            return;
        }
    }

    void SendBatch(uint64_t bytes) {
        toReceiver.Rewind();
        toSender.Rewind();
        ++batchId;
        sender.Offer(batchId, {TransferFile{bytes, "perf.bin"}});
        Settle();
    }
};

}

void RunTransferPerf() {
    BeginGroup("transfer");

    std::vector<uint8_t> payload(kCrcTotalBytes);
    FillRandom(payload);

    Crc32 rolling;
    Measure(Workload{"transfer/crc32-streamed", "chunk", kCrcTotalBytes / kCrcChunkBytes,
        kCrcTotalBytes, 0.0, [&] {
            rolling.Reset();
            for (size_t offset = 0; offset < payload.size(); offset += kCrcChunkBytes)
                rolling.Update(std::span<const uint8_t>(payload.data() + offset, kCrcChunkBytes));
            Consume(rolling.Value());
        }});

    TransferRig rig{size_t(kBatchBytes)};
    Measure(Workload{"transfer/send-and-receive", "chunk", ChunksFor(kBatchBytes), kBatchBytes,
        0.2, [&] { rig.SendBatch(kBatchBytes); }});

    TransferRig scalingRig{size_t(kBatchBytes)};
    MeasureScaling(ScalingWorkload{"transfer/send-and-receive-scaling", "chunk",
        ChunksFor(kSmallBatchBytes), ChunksFor(kBatchBytes), 6.0, [&](uint64_t units) {
            scalingRig.SendBatch(units * kMaxFileChunkBytes);
        }});
}
