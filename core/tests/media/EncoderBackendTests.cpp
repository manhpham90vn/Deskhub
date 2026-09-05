#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/EncoderBackend.h"

#include <cstdio>

using namespace deskhub::media;

namespace {

bool Contains(std::span<const std::string_view> order, std::string_view name) {
    for (std::string_view id : order)
        if (id == name) return true;
    return false;
}

void TestMeasuredVendorsLeadWithTheirOwnEncoder() {
    std::printf("[encoder] each vendor with numbers behind it leads with its own encoder...\n");

    const EncoderBackendOrder nvidia = EncoderBackendOrderFor(GpuVendor::Nvidia);
    Check(nvidia.backends.front() == kEncoderBackendNvenc,
        "an NVIDIA adapter tries NVENC before anything else");
    Check(nvidia.measured, "and that order was measured on NVIDIA silicon, not guessed");

    const EncoderBackendOrder intel = EncoderBackendOrderFor(GpuVendor::Intel);
    Check(intel.backends.front() == kEncoderBackendMediaFoundation,
        "an Intel adapter goes straight to Media Foundation instead of loading nvEncodeAPI64");
    Check(intel.measured, "Quick Sync was measured on 04/09 and reported LTR and intra refresh");
}

void TestAmdIsAGuessAndSaysSo() {
    std::printf("[encoder] AMD has no measurement behind it and the table admits it...\n");

    const EncoderBackendOrder amd = EncoderBackendOrderFor(GpuVendor::Amd);
    Check(amd.backends.front() == kEncoderBackendMediaFoundation,
        "NVENC cannot drive an AMD adapter, so Media Foundation leads");
    Check(!amd.measured,
        "no AMD machine has run the bake-off, and a guess that claims to be measured is worse "
        "than a guess that says what it is");
}

void TestUnknownAdaptersKeepTodaysOrder() {
    std::printf("[encoder] an adapter nobody recognises keeps the try-in-order rule...\n");

    for (GpuVendor vendor : {GpuVendor::Microsoft, GpuVendor::Unknown}) {
        const EncoderBackendOrder order = EncoderBackendOrderFor(vendor);
        Check(order.backends.front() == kEncoderBackendNvenc,
            "the fallback is the order the factory used before this table existed");
        Check(!order.measured, "and it is a fallback, not a measurement");
    }
}

void TestNoVendorLosesAFallback() {
    std::printf("[encoder] no vendor is left without a second backend to fall back to...\n");

    for (GpuVendor vendor : {GpuVendor::Nvidia, GpuVendor::Intel, GpuVendor::Amd,
             GpuVendor::Microsoft, GpuVendor::Unknown}) {
        const EncoderBackendOrder order = EncoderBackendOrderFor(vendor);
        Check(Contains(order.backends, kEncoderBackendNvenc) &&
                Contains(order.backends, kEncoderBackendMediaFoundation),
            "reordering may not drop a backend: a wrong guess must still cost a slower start, "
            "never a source that cannot encode at all");
        for (std::string_view id : order.backends)
            Check(IsEncoderBackendName(id) && id != kEncoderBackendAuto,
                "every entry names a real backend, and auto is a request rather than one");
    }
}

}

void RunEncoderBackendTests() {
    TestMeasuredVendorsLeadWithTheirOwnEncoder();
    TestAmdIsAGuessAndSaysSo();
    TestUnknownAdaptersKeepTodaysOrder();
    TestNoVendorLosesAFallback();
}
