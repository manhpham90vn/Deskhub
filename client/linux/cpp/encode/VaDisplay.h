#pragma once
#include <va/va.h>

#include <string>

class VaDisplay {
public:
    static VaDisplay& Instance();

    bool Open();

    VADisplay handle() const {
        return dpy_;
    }
    bool isOpen() const {
        return dpy_ != nullptr;
    }

    const std::string& driverName() const {
        return driverName_;
    }
    VAEntrypoint encodeEntrypoint() const {
        return encEntrypoint_;
    }
    const std::string& lastError() const {
        return lastError_;
    }

private:
    VaDisplay() = default;
    ~VaDisplay();
    VaDisplay(const VaDisplay&) = delete;
    VaDisplay& operator=(const VaDisplay&) = delete;

    VADisplay dpy_ = nullptr;
    int drmFd_ = -1;
    VAEntrypoint encEntrypoint_ = VAEntrypointEncSlice;
    std::string driverName_, lastError_;
};

bool VaHasEntrypoint(VADisplay dpy, VAProfile profile, VAEntrypoint entrypoint);
