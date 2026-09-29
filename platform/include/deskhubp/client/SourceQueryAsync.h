#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/net/UdpSocket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace deskhubp {

struct ConnectOutcome {
    bool ok = false;
    std::vector<deskhub::SourceInfo> sources;
    deskhub::HostCaps caps{};
};

class SourceQueryAsync {
public:
    using UiPost = std::function<void(std::function<void()>)>;
    using DoneHandler = std::function<void(const ConnectOutcome&)>;

    bool QueryAsync(const NetAddr& server, UiPost postToUi, DoneHandler onDone) {
        if (pending_->exchange(true, std::memory_order_acq_rel)) return false;
        std::thread([pending = pending_, server,
                        postToUi = std::move(postToUi), onDone = std::move(onDone)] {
            auto outcome = std::make_shared<ConnectOutcome>();
            outcome->ok = QuerySources(server, outcome->sources, nullptr, &outcome->caps);
            pending->store(false, std::memory_order_release);
            postToUi([outcome, onDone] { onDone(*outcome); });
        }).detach();
        return true;
    }

private:
    std::shared_ptr<std::atomic<bool>> pending_ = std::make_shared<std::atomic<bool>>(false);
};

}
