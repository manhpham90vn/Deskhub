#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "capture/CaptureTypes.h"
#include "capture/KmsDevice.h"

class KmsBlitter {
public:
    KmsBlitter();
    ~KmsBlitter();
    KmsBlitter(const KmsBlitter&) = delete;
    KmsBlitter& operator=(const KmsBlitter&) = delete;

    bool Open(const std::string& renderNode);
    void Close();

    std::optional<LinuxFrameInfo> Compose(const KmsScanout& scanout, KmsBuffer screen,
        std::optional<KmsBuffer> cursor);

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};
