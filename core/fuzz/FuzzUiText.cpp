#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/UiSettings.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);

    const auto settings = deskhub::ui::ParseUiSettings(text);
    deskhub::ui::ParseUiSettings(deskhub::ui::SerializeUiSettings(settings));

    const auto devices = deskhub::ui::ParseRecentDevices(text);
    deskhub::ui::ParseRecentDevices(deskhub::ui::SerializeRecentDevices(devices));

    return 0;
}
