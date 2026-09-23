#pragma once

namespace deskhub {

class OpenViewerCount {
public:
    void Opened() {
        ++count_;
    }

    bool Closed() {
        if (count_ > 0) --count_;
        return count_ <= 0;
    }

private:
    int count_ = 0;
};

}
