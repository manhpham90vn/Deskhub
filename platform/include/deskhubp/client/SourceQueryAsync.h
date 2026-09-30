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

struct ConnectOutcome : SourceQueryReply {
    bool ok = false;
};

class SourceQueryAsync {
public:
    using UiPost = std::function<void(std::function<void()>)>;
    using DoneHandler = std::function<void(const ConnectOutcome&)>;

    bool QueryAsync(const NetAddr& server, UiPost postToUi, DoneHandler onDone) {
        return QueryAsync(server, SourceQueryRequest{}, std::string{}, std::move(postToUi),
            std::move(onDone));
    }

    bool QueryAsync(const NetAddr& server, SourceQueryRequest request, std::string invite,
        UiPost postToUi, DoneHandler onDone) {
        if (pending_->exchange(true, std::memory_order_acq_rel)) return false;
        cancel_->store(false, std::memory_order_release);
        if (request.cancel == nullptr) request.cancel = cancel_.get();
        std::thread([pending = pending_, cancel = cancel_, server, request = std::move(request),
                        invite = std::move(invite), postToUi = std::move(postToUi),
                        onDone = std::move(onDone)] {
            auto outcome = std::make_shared<ConnectOutcome>();
            outcome->ok = invite.empty() ? QuerySources(server, *outcome, request)
                                         : QuerySourcesByInvite(invite, *outcome, request);
            pending->store(false, std::memory_order_release);
            postToUi([outcome, onDone] { onDone(*outcome); });
        }).detach();
        return true;
    }

    void Cancel() {
        cancel_->store(true, std::memory_order_release);
    }

private:
    std::shared_ptr<std::atomic<bool>> pending_ = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<std::atomic<bool>> cancel_ = std::make_shared<std::atomic<bool>>(false);
};

}
