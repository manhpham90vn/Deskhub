#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/FileReceiver.h"
#include "deskhub/session/client/FileSender.h"
#include "deskhub/transfer/Crc32.h"
#include "deskhub/transfer/SafeName.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

using Message = std::vector<uint8_t>;

struct Link {
    std::vector<Message> toReceiver{};
    std::vector<Message> toSender{};

    bool Quiet() const {
        return toReceiver.empty() && toSender.empty();
    }
};

struct FakeDisk {
    std::map<std::string, std::vector<uint8_t>> stored{};
    std::map<std::string, std::vector<uint8_t>> pending{};
    std::map<uint16_t, std::string> openName{};
    bool refuseOpen = false;
    bool refuseWrite = false;

    std::string Open(uint16_t index, const std::string& safeName) {
        if (refuseOpen) return {};
        const std::string name = UniqueFileName(safeName, [this](const std::string& n) {
            return stored.count(n) != 0 || pending.count(n) != 0;
        });
        if (name.empty()) return {};
        pending[name] = {};
        openName[index] = name;
        return name;
    }

    bool Write(uint16_t index, std::span<const uint8_t> data) {
        if (refuseWrite) return false;
        std::vector<uint8_t>& sink = pending[openName[index]];
        sink.insert(sink.end(), data.begin(), data.end());
        return true;
    }

    void Close(uint16_t index, bool keep) {
        const std::string name = openName[index];
        if (keep) stored[name] = pending[name];
        pending.erase(name);
        openName.erase(index);
    }
};

struct Rig {
    Link link{};
    FakeDisk disk{};
    std::map<uint16_t, std::vector<uint8_t>> source{};
    std::vector<std::string> audit{};
    TransferProgress lastSent{};
    TransferProgress lastStored{};
    bool readFails = false;
    size_t refuseSendsLeft = 0;

    FileSender sender;
    FileReceiver receiver;

    Rig()
        : sender(FileSenderCallbacks{
              [this](std::span<const uint8_t> m) {
                  if (refuseSendsLeft > 0) {
                      --refuseSendsLeft;
                      return false;
                  }
                  link.toReceiver.emplace_back(m.begin(), m.end());
                  return true;
              },
              [this](uint16_t index, uint64_t offset, std::span<uint8_t> out) -> size_t {
                  if (readFails) return 0;
                  const std::vector<uint8_t>& file = source[index];
                  if (offset >= file.size()) return 0;
                  const size_t take = std::min(out.size(), file.size() - size_t(offset));
                  std::memcpy(out.data(), file.data() + offset, take);
                  return take;
              },
              [this](const TransferProgress& p) { lastSent = p; }, {}}),
          receiver(FileReceiverCallbacks{
              [this](std::span<const uint8_t> m) { link.toSender.emplace_back(m.begin(),
                                                       m.end()); },
              [this](uint16_t index, const std::string& safe, uint64_t) {
                  return disk.Open(index, safe);
              },
              [this](uint16_t index, std::span<const uint8_t> d) { return disk.Write(index, d); },
              [this](uint16_t index, bool keep) {
                  disk.Close(index, keep);
                  return true;
              },
              [this](const TransferProgress& p) { lastStored = p; },
              [this](std::string_view line) { audit.emplace_back(line); }}) {
        receiver.SetAccepting(true);
    }

    std::vector<TransferFile> Stage(const std::vector<std::pair<std::string, size_t>>& spec) {
        std::vector<TransferFile> files;
        for (size_t i = 0; i < spec.size(); ++i) {
            std::vector<uint8_t> bytes(spec[i].second);
            for (size_t b = 0; b < bytes.size(); ++b) bytes[b] = uint8_t(Rnd());
            source[uint16_t(i)] = bytes;
            files.push_back(TransferFile{bytes.size(), spec[i].first});
        }
        return files;
    }

    void Settle() {
        for (int round = 0; round < 200000; ++round) {
            if (!link.toReceiver.empty()) {
                const Message m = link.toReceiver.front();
                link.toReceiver.erase(link.toReceiver.begin());
                receiver.HandleMessage(m);
                continue;
            }
            if (!link.toSender.empty()) {
                const Message m = link.toSender.front();
                link.toSender.erase(link.toSender.begin());
                sender.HandleMessage(m);
                continue;
            }
            if (sender.State() == FileSenderState::Sending) {
                const size_t queued = link.toReceiver.size();
                sender.Pump(1);
                if (link.toReceiver.size() != queued || !link.toSender.empty()) continue;
            }
            return;
        }
        Check(false, "the transfer settles instead of looping forever");
    }

    bool Landed(const std::string& name, uint16_t sourceIndex) const {
        const auto at = disk.stored.find(name);
        return at != disk.stored.end() && at->second == source.at(sourceIndex);
    }
};

void TestABatchLandsByteForByte() {
    std::printf("[xfer] a batch of files lands byte for byte...\n");
    Rig rig;
    const auto files = rig.Stage({{"notes.txt", 10}, {"video.mp4", 200000}, {"empty.bin", 0}});
    Check(rig.sender.Offer(9, files), "the batch is offered");
    rig.Settle();

    Check(rig.sender.State() == FileSenderState::Done, "the sender reports the batch done");
    Check(rig.receiver.State() == FileReceiverState::Done, "and so does the receiver");
    Check(rig.disk.stored.size() == 3, "three files reached the disk");
    Check(rig.Landed("notes.txt", 0), "the small file is identical");
    Check(rig.Landed("video.mp4", 1), "the multi-chunk file is identical");
    Check(rig.Landed("empty.bin", 2), "the empty file exists and is empty");
    Check(rig.disk.pending.empty(), "no half-written file is left behind");
    Check(rig.lastStored.batchBytes == rig.lastStored.batchSize,
        "progress finishes at the full batch size");
}

void TestAFileLargerThanOneChunkIsSplit() {
    std::printf("[xfer] a file larger than one record is split and rejoined...\n");
    Rig rig;
    const auto files = rig.Stage({{"big.bin", kMaxFileChunkBytes * 3 + 17}});
    Check(rig.sender.Offer(1, files), "the batch is offered");
    rig.Settle();
    Check(rig.Landed("big.bin", 0), "every chunk was rejoined in order");
}

void TestRefusedWhenNotAccepting() {
    std::printf("[xfer] a receiver that is not accepting refuses the batch...\n");
    Rig rig;
    rig.receiver.SetAccepting(false);
    const auto files = rig.Stage({{"notes.txt", 32}});
    Check(rig.sender.Offer(4, files), "the batch is offered anyway");
    rig.Settle();

    Check(rig.sender.State() == FileSenderState::Refused, "the sender learns it was refused");
    Check(rig.sender.Reason() == TransferReason::NotAccepting, "and why");
    Check(rig.disk.stored.empty(), "nothing was written");
}

void TestASecondBatchIsRefusedWhileOneRuns() {
    std::printf("[xfer] a second batch is refused while one is still running...\n");
    Rig rig;
    const auto files = rig.Stage({{"first.bin", kMaxFileChunkBytes * 4}});
    Check(rig.sender.Offer(1, files), "the first batch is offered");
    for (int i = 0; i < 3; ++i) {
        if (!rig.link.toReceiver.empty()) {
            const Message m = rig.link.toReceiver.front();
            rig.link.toReceiver.erase(rig.link.toReceiver.begin());
            rig.receiver.HandleMessage(m);
        }
        rig.sender.Pump(1);
    }
    Check(rig.receiver.Busy(), "the receiver is mid-batch");

    FileSender second(FileSenderCallbacks{
        [&rig](std::span<const uint8_t> m) {
            rig.link.toReceiver.emplace_back(m.begin(), m.end());
            return true;
        },
        {}, {}, {}});
    Check(second.Offer(2, {TransferFile{4, "other.bin"}}), "a second sender offers");
    while (!rig.link.toReceiver.empty()) {
        const Message m = rig.link.toReceiver.front();
        rig.link.toReceiver.erase(rig.link.toReceiver.begin());
        rig.receiver.HandleMessage(m);
    }
    const auto refusal = ParseFileAccept(PayloadOf(rig.link.toSender.back()));
    Check(refusal && refusal->batchId == 2 && refusal->reason == TransferReason::Busy,
        "and is told the receiver is busy");
}

void TestHostileNamesCannotEscapeTheFolder() {
    std::printf("[xfer] a hostile name cannot escape the destination folder...\n");
    Rig rig;
    rig.Stage({{"payload", 64}});
    Check(!rig.sender.Offer(1, {TransferFile{64, "../../.ssh/authorized_keys"}}) ||
              rig.sender.Files()[0].name == "authorized_keys",
        "the sender strips the path before it ever offers the file");

    Rig direct;
    direct.Stage({{"payload", 64}});
    FileOffer offer;
    offer.batchId = 5;
    offer.files.push_back(TransferFile{64, "..\\evil.dll"});
    std::vector<uint8_t> raw(kMaxRecordSize);
    const size_t n = BuildFileOffer(raw, offer);
    Check(n == 0, "and the wire refuses to carry a name with a separator at all");
}

void TestACollidingNameIsRenamed() {
    std::printf("[xfer] an arriving file never overwrites one already there...\n");
    Rig rig;
    rig.disk.stored["photo.png"] = {1, 2, 3};
    const auto files = rig.Stage({{"photo.png", 500}});
    Check(rig.sender.Offer(1, files), "the batch is offered");
    rig.Settle();

    Check(rig.disk.stored["photo.png"] == std::vector<uint8_t>({1, 2, 3}),
        "the file already there is untouched");
    Check(rig.Landed("photo (2).png", 0), "and the arrival is stored beside it");
    Check(rig.receiver.Stored()[0] == "photo (2).png", "the receiver reports the stored name");
}

void TestACorruptFileIsDiscarded() {
    std::printf("[xfer] a file whose checksum does not match is discarded...\n");
    Rig rig;
    const auto files = rig.Stage({{"data.bin", 5000}});
    Check(rig.sender.Offer(3, files), "the batch is offered");

    bool flipped = false;
    for (int round = 0; round < 1000; ++round) {
        if (!rig.link.toReceiver.empty()) {
            Message m = rig.link.toReceiver.front();
            rig.link.toReceiver.erase(rig.link.toReceiver.begin());
            const auto header = ParseCommonHeader(m);
            if (!flipped && header && header->type == MsgType::FileChunk) {
                m[kCommonHeaderSize + kFileChunkHeaderSize] ^= 0xFF;
                flipped = true;
            }
            rig.receiver.HandleMessage(m);
            continue;
        }
        if (!rig.link.toSender.empty()) {
            const Message m = rig.link.toSender.front();
            rig.link.toSender.erase(rig.link.toSender.begin());
            rig.sender.HandleMessage(m);
            continue;
        }
        if (rig.sender.State() == FileSenderState::Sending && rig.sender.Pump(1) > 0) continue;
        break;
    }

    Check(flipped, "a chunk really was corrupted in flight");
    Check(rig.receiver.State() == FileReceiverState::Failed, "the receiver fails the batch");
    Check(rig.receiver.Reason() == TransferReason::Corrupt, "because the checksum did not match");
    Check(rig.disk.stored.empty(), "the corrupt file is not kept");
    Check(rig.disk.pending.empty(), "and its half-written copy is cleaned up");
    Check(rig.sender.State() == FileSenderState::Failed, "the sender is told the batch failed");
}

void TestChunksOutOfPlaceAreRejected() {
    std::printf("[xfer] a chunk at the wrong offset ends the batch...\n");
    Rig rig;
    const auto files = rig.Stage({{"data.bin", 4000}});
    Check(rig.sender.Offer(2, files), "the batch is offered");
    rig.receiver.HandleMessage(rig.link.toReceiver.front());
    rig.link.toReceiver.clear();
    rig.link.toSender.clear();

    std::vector<uint8_t> raw(kMaxRecordSize);
    const uint8_t payload[8] = {9, 9, 9, 9, 9, 9, 9, 9};
    const size_t n = BuildFileChunk(raw, 2, 0, 512, payload);
    Check(n > 0, "a chunk claiming a gap is well-formed");
    rig.receiver.HandleMessage(std::span<const uint8_t>(raw.data(), n));

    Check(rig.receiver.State() == FileReceiverState::Failed, "the receiver refuses the gap");
    Check(rig.receiver.Reason() == TransferReason::Corrupt, "and calls the batch corrupt");
    Check(rig.disk.stored.empty(), "nothing is kept");
}

void TestOverrunIsRejected() {
    std::printf("[xfer] a sender that writes past the size it announced is cut off...\n");
    Rig rig;
    rig.Stage({{"small.bin", 16}});
    Check(rig.sender.Offer(7, {TransferFile{16, "small.bin"}}), "sixteen bytes are offered");
    rig.receiver.HandleMessage(rig.link.toReceiver.front());
    rig.link.toReceiver.clear();
    rig.link.toSender.clear();

    std::vector<uint8_t> raw(kMaxRecordSize);
    const std::vector<uint8_t> tooMuch(64, 0xAB);
    const size_t n = BuildFileChunk(raw, 7, 0, 0, tooMuch);
    rig.receiver.HandleMessage(std::span<const uint8_t>(raw.data(), n));

    Check(rig.receiver.State() == FileReceiverState::Failed, "the overrun ends the batch");
    Check(rig.disk.stored.empty() && rig.disk.pending.empty(), "and leaves no file behind");
}

void TestAWriteFailureStopsTheBatch() {
    std::printf("[xfer] a disk that cannot be written to stops the batch...\n");
    Rig rig;
    rig.disk.refuseWrite = true;
    const auto files = rig.Stage({{"data.bin", 4096}});
    Check(rig.sender.Offer(1, files), "the batch is offered");
    rig.Settle();

    Check(rig.receiver.Reason() == TransferReason::WriteFailed, "the receiver reports the write");
    Check(rig.sender.State() == FileSenderState::Failed, "and the sender stops sending");
    Check(rig.sender.Reason() == TransferReason::WriteFailed, "knowing exactly why");
    Check(rig.disk.pending.empty(), "the partial file is cleaned up");
}

void TestAnUnreadableSourceStopsTheBatch() {
    std::printf("[xfer] a source file that cannot be read stops the batch...\n");
    Rig rig;
    const auto files = rig.Stage({{"data.bin", 4096}});
    Check(rig.sender.Offer(1, files), "the batch is offered");
    rig.readFails = true;
    rig.Settle();

    Check(rig.sender.State() == FileSenderState::Failed, "the sender fails");
    Check(rig.sender.Reason() == TransferReason::ReadFailed, "because it could not read");
    Check(rig.receiver.State() == FileReceiverState::Failed, "the receiver is told");
    Check(rig.disk.stored.empty() && rig.disk.pending.empty(), "and keeps nothing");
}

void TestCancelMidFlight() {
    std::printf("[xfer] cancelling mid-flight discards the partial file...\n");
    Rig rig;
    const auto files = rig.Stage({{"big.bin", kMaxFileChunkBytes * 8}});
    Check(rig.sender.Offer(6, files), "the batch is offered");
    for (int i = 0; i < 6; ++i) {
        while (!rig.link.toReceiver.empty()) {
            const Message m = rig.link.toReceiver.front();
            rig.link.toReceiver.erase(rig.link.toReceiver.begin());
            rig.receiver.HandleMessage(m);
        }
        while (!rig.link.toSender.empty()) {
            const Message m = rig.link.toSender.front();
            rig.link.toSender.erase(rig.link.toSender.begin());
            rig.sender.HandleMessage(m);
        }
        rig.sender.Pump(1);
    }
    Check(rig.receiver.Busy(), "the batch is under way");

    rig.sender.Cancel();
    rig.Settle();

    Check(rig.sender.State() == FileSenderState::Failed, "the sender ends the batch");
    Check(rig.sender.Reason() == TransferReason::Cancelled, "as cancelled");
    Check(rig.receiver.Reason() == TransferReason::Cancelled, "the receiver agrees");
    Check(rig.disk.stored.empty(), "no partial file is kept");
    Check(rig.disk.pending.empty(), "and the temporary copy is gone");
}

void TestLinkLossEndsBothSides() {
    std::printf("[xfer] a dropped link ends the batch on both sides...\n");
    Rig rig;
    const auto files = rig.Stage({{"big.bin", kMaxFileChunkBytes * 4}});
    Check(rig.sender.Offer(8, files), "the batch is offered");
    rig.receiver.HandleMessage(rig.link.toReceiver.front());
    rig.link.toReceiver.clear();
    rig.link.toSender.clear();
    rig.sender.Pump(2);

    rig.sender.LinkLost();
    rig.receiver.LinkLost();

    Check(rig.sender.Reason() == TransferReason::LinkLost, "the sender says the link went");
    Check(rig.receiver.Reason() == TransferReason::LinkLost, "and so does the receiver");
    Check(rig.disk.pending.empty(), "the partial file is discarded, not left half-written");
}

void TestLimitsRefuseOversizedBatches() {
    std::printf("[xfer] a receiver refuses a batch beyond the limits it sets...\n");
    Rig rig;
    FileReceiverLimits tight;
    tight.maxFiles = 2;
    tight.maxFileBytes = 1024;
    tight.maxBatchBytes = 1500;
    rig.receiver.SetLimits(tight);

    const auto tooMany = rig.Stage({{"a", 4}, {"b", 4}, {"c", 4}});
    Check(rig.sender.Offer(1, tooMany), "three files are offered");
    rig.Settle();
    Check(rig.sender.Reason() == TransferReason::TooManyFiles, "and refused for the count");

    Rig second;
    second.receiver.SetLimits(tight);
    const auto tooBig = second.Stage({{"a", 2048}});
    Check(second.sender.Offer(1, tooBig), "an over-large file is offered");
    second.Settle();
    Check(second.sender.Reason() == TransferReason::TooLarge, "and refused for the size");

    Rig third;
    third.receiver.SetLimits(tight);
    const auto tooMuch = third.Stage({{"a", 1000}, {"b", 1000}});
    Check(third.sender.Offer(1, tooMuch), "a batch over the total is offered");
    third.Settle();
    Check(third.sender.Reason() == TransferReason::TooLarge, "and refused for the total");
}

void TestAnOfferOfNoFilesIsIgnored() {
    std::printf("[xfer] an offer carrying zero files never opens a batch...\n");
    Rig rig;
    const auto files = rig.Stage({{"a", 4}});
    Check(rig.sender.Offer(1, files), "a real offer leaves the sender");
    Message offer = rig.link.toReceiver.front();
    rig.link.toReceiver.clear();
    offer[kCommonHeaderSize + 4] = 0;
    rig.receiver.HandleMessage(offer);
    Check(rig.receiver.State() == FileReceiverState::Idle, "the receiver stays idle");
    Check(rig.link.Quiet(), "and answers nothing");
}

void TestTheOfferIsGuardedBeforeItLeaves() {
    std::printf("[xfer] the sender refuses to offer what it cannot carry...\n");
    Rig rig;
    Check(!rig.sender.Offer(1, {}), "an empty batch is not offered");
    Check(!rig.sender.Offer(1, {TransferFile{4, ".."}}), "a name with nothing safe in it is not");
    Check(!rig.sender.Offer(1, {TransferFile{kMaxTransferFileBytes + 1, "huge.bin"}}),
        "a file past the protocol limit is not");
    std::vector<TransferFile> tooMany;
    for (size_t i = 0; i <= kMaxTransferFiles; ++i)
        tooMany.push_back(TransferFile{1, "f" + std::to_string(i)});
    Check(!rig.sender.Offer(1, tooMany), "and neither is a batch of too many files");

    const auto files = rig.Stage({{"ok.bin", 8}});
    Check(rig.sender.Offer(1, files), "a sound batch is offered");
    Check(!rig.sender.Offer(2, files), "but not a second one while the first is live");
}

void TestAFullStreamIsRetriedNotSkipped() {
    std::printf("[xfer] a chunk the transport refuses is retried, never dropped...\n");
    Rig rig;
    const auto files = rig.Stage({{"big.bin", kMaxFileChunkBytes * 5 + 91}});
    Check(rig.sender.Offer(1, files), "the batch is offered");

    for (int round = 0; round < 100000; ++round) {
        if (!rig.link.toReceiver.empty()) {
            const Message m = rig.link.toReceiver.front();
            rig.link.toReceiver.erase(rig.link.toReceiver.begin());
            rig.receiver.HandleMessage(m);
            continue;
        }
        if (!rig.link.toSender.empty()) {
            const Message m = rig.link.toSender.front();
            rig.link.toSender.erase(rig.link.toSender.begin());
            rig.sender.HandleMessage(m);
            continue;
        }
        if (rig.sender.State() != FileSenderState::Sending) break;
        rig.refuseSendsLeft = size_t(round % 3);
        const size_t before = rig.link.toReceiver.size();
        rig.sender.Pump(2);
        rig.refuseSendsLeft = 0;
        if (rig.link.toReceiver.size() == before && rig.link.toSender.empty()) {
            if (rig.sender.Pump(2) == 0 && rig.link.toReceiver.empty()) break;
        }
    }

    Check(rig.sender.State() == FileSenderState::Done, "the batch still finishes");
    Check(rig.Landed("big.bin", 0), "and every refused chunk was sent again, in order");
}

void TestAuditTrailNamesTheFiles() {
    std::printf("[xfer] every batch leaves an audit trail...\n");
    Rig rig;
    rig.receiver.SetPeer(TransferPeer{"192.168.1.9:47777", "laptop", {}});
    const auto files = rig.Stage({{"notes.txt", 40}});
    Check(rig.sender.Offer(11, files), "the batch is offered");
    rig.Settle();

    bool sawOffer = false, sawStored = false, sawComplete = false;
    for (const std::string& line : rig.audit) {
        if (line.find("transfer offered batch=11") != std::string::npos) sawOffer = true;
        if (line.find("transfer stored") != std::string::npos &&
            line.find("notes.txt") == std::string::npos)
            sawStored = false;
        else if (line.find("transfer stored") != std::string::npos)
            sawStored = true;
        if (line.find("transfer complete") != std::string::npos) sawComplete = true;
        Check(line.find("192.168.1.9:47777") != std::string::npos,
            "every line names the machine it came from");
        Check(line.find("laptop") != std::string::npos, "and the name it gave");
    }
    Check(sawOffer, "the offer is logged");
    Check(sawStored, "each stored file is logged by name");
    Check(sawComplete, "and so is the end of the batch");
}

}

void RunFileTransferTests() {
    TestABatchLandsByteForByte();
    TestAFileLargerThanOneChunkIsSplit();
    TestRefusedWhenNotAccepting();
    TestASecondBatchIsRefusedWhileOneRuns();
    TestHostileNamesCannotEscapeTheFolder();
    TestACollidingNameIsRenamed();
    TestACorruptFileIsDiscarded();
    TestChunksOutOfPlaceAreRejected();
    TestOverrunIsRejected();
    TestAWriteFailureStopsTheBatch();
    TestAnUnreadableSourceStopsTheBatch();
    TestCancelMidFlight();
    TestLinkLossEndsBothSides();
    TestAFullStreamIsRetriedNotSkipped();
    TestLimitsRefuseOversizedBatches();
    TestAnOfferOfNoFilesIsIgnored();
    TestTheOfferIsGuardedBeforeItLeaves();
    TestAuditTrailNamesTheFiles();
}
