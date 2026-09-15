#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace deskhubp {

struct PortalStream {
    uint32_t nodeId = 0;
    int32_t x = 0, y = 0;
    uint32_t width = 0, height = 0;
    std::string name;
};

class PortalScreenCast {
public:
    static PortalScreenCast& Instance();

    bool Open();

    void Close();

    void ForgetSavedSelection();

    bool isOpen() const {
        return pipewireFd_ >= 0;
    }

    int OpenRemoteFd();

    const std::vector<PortalStream>& streams() const {
        return streams_;
    }

    const std::string& lastError() const {
        return lastError_;
    }

private:
    enum class AttemptResult { Ok,
        Cancelled,
        FailedWithToken,
        Failed };

    PortalScreenCast() = default;
    ~PortalScreenCast();
    PortalScreenCast(const PortalScreenCast&) = delete;
    PortalScreenCast& operator=(const PortalScreenCast&) = delete;

    AttemptResult OpenAttempt();

    void* bus_ = nullptr;
    int pipewireFd_ = -1;
    std::string sessionHandle_;
    std::string restoreToken_;
    std::vector<PortalStream> streams_;
    std::string lastError_;
};

}
