#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/RecentDevices.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

void TestRoundTrip() {
    std::printf("[recent] a saved list comes back exactly as it was...\n");
    std::vector<ui::RecentDevice> devices{
        {"192.168.1.10", 1754300000, ""},
        {"192.168.1.20:5000", 1754200000, ""},
    };
    const std::string text = ui::SerializeRecentDevices(devices);
    Check(ui::ParseRecentDevices(text) == devices, "serialize then parse is identity");
}

void TestLegacyPasscodesAreDiscarded() {
    std::printf("[recent] old passcodes are discarded while addresses remain...\n");
    std::vector<ui::RecentDevice> devices;
    ui::TouchRecentDevice(devices, "192.168.1.10", 100, "0417");
    Check(devices.size() == 1 && devices[0].passcode.empty(),
        "a supplied passcode is not kept in memory");

    const std::vector<ui::RecentDevice> saved{{"192.168.1.50", 1754300000, "0417"}};
    const std::string text = ui::SerializeRecentDevices(saved);
    Check(text == "1754300000 192.168.1.50\n", "the retired field is omitted entirely");
    Check(ui::ParseRecentDevices(text)[0].passcode.empty(),
        "a saved address does not recreate a passcode");

    const auto reloaded = ui::ParseRecentDevices(
        "1754300000 192.168.1.40 9182\n"
        "1754200000 192.168.1.41 999\n"
        "1754100000 192.168.1.42\n"
        "  1754000000   192.168.1.43   5150  \n");
    Check(reloaded.size() == 4, "all four lines parse");
    if (reloaded.size() == 4) {
        Check(reloaded[0].addr == "192.168.1.40" && reloaded[0].passcode.empty(),
            "a code on an old line is discarded");
        Check(reloaded[1].addr == "192.168.1.41" && reloaded[1].passcode.empty(),
            "a malformed code is dropped but the device is kept");
        Check(reloaded[2].passcode.empty(), "an old line without a code still parses");
        Check(reloaded[3].addr == "192.168.1.43" && reloaded[3].passcode.empty(),
            "runs of spaces around the fields do not swallow the address");
    }
}

void TestParseSkipsGarbage() {
    std::printf("[recent] hand-edited or corrupt lines are dropped, not fatal...\n");
    const std::string text =
        "1754300000 192.168.1.10\n"
        "\n"
        "not-a-timestamp 192.168.1.11\n"
        "1754200000\n"
        "  1754100000   192.168.1.12  \n"
        "99999999999999999999 192.168.1.13\n";
    const auto devices = ui::ParseRecentDevices(text);
    Check(devices.size() == 2, "only the two well-formed lines survive");
    Check(devices[0].addr == "192.168.1.10", "first good line kept");
    Check(devices[1].addr == "192.168.1.12" && devices[1].lastConnectedUnix == 1754100000,
        "whitespace around a good line is tolerated");
    Check(ui::ParseRecentDevices("").empty(), "an empty file is an empty list");
}

void TestParseDropsDuplicates() {
    std::printf("[recent] a duplicated address keeps only its newest entry...\n");
    const auto devices = ui::ParseRecentDevices(
        "1754300000 192.168.1.10\n"
        "1754200000 192.168.1.10\n");
    Check(devices.size() == 1, "one entry per address");
    Check(devices[0].lastConnectedUnix == 1754300000, "the first (newest) entry wins");
}

void TestTouchMovesToFront() {
    std::printf("[recent] connecting again moves the device to the top...\n");
    std::vector<ui::RecentDevice> devices{
        {"192.168.1.10", 100},
        {"192.168.1.20", 90},
    };
    ui::TouchRecentDevice(devices, "192.168.1.20", 200);
    Check(devices.size() == 2, "no duplicate is created");
    Check(devices[0].addr == "192.168.1.20" && devices[0].lastConnectedUnix == 200,
        "the touched device is first with the new timestamp");

    ui::TouchRecentDevice(devices, "  192.168.1.30 ", 300);
    Check(devices.size() == 3 && devices[0].addr == "192.168.1.30",
        "a new address is trimmed and inserted at the top");

    ui::TouchRecentDevice(devices, "   ", 400);
    Check(devices.size() == 3, "a blank address is ignored");
}

void TestCapKeepsNewest() {
    std::printf("[recent] the list never grows past the cap...\n");
    std::vector<ui::RecentDevice> devices;
    for (int i = 0; i < 25; ++i)
        ui::TouchRecentDevice(devices, "10.0.0." + std::to_string(i), 1000 + i);
    Check(devices.size() == ui::kMaxRecentDevices, "touch enforces the cap");
    Check(devices[0].addr == "10.0.0.24", "the newest device stays");

    std::string text;
    for (int i = 0; i < 25; ++i)
        text += std::to_string(2000 + i) + " 10.1.0." + std::to_string(i) + "\n";
    Check(ui::ParseRecentDevices(text).size() == ui::kMaxRecentDevices,
        "parse enforces the cap too");
}

void TestRemove() {
    std::printf("[recent] removing an address deletes exactly that entry...\n");
    std::vector<ui::RecentDevice> devices{
        {"192.168.1.10", 100},
        {"192.168.1.20", 90},
    };
    ui::RemoveRecentDevice(devices, "192.168.1.10");
    Check(devices.size() == 1 && devices[0].addr == "192.168.1.20", "the other entry stays");
    ui::RemoveRecentDevice(devices, "192.168.1.99");
    Check(devices.size() == 1, "removing an unknown address is a no-op");
}

void TestDefaultPortSpellingsAreOneDevice() {
    std::printf("[recent] a bare address and one with the default port are the same device...\n");
    std::vector<ui::RecentDevice> devices;
    ui::TouchRecentDevice(devices, "192.168.1.60:47777", 100, "0417");

    ui::TouchRecentDevice(devices, "192.168.1.60", 200, "");
    Check(devices.size() == 1, "touching the other spelling replaces the entry");
    Check(devices[0].passcode.empty(), "the replacement has no passcode");

    ui::RemoveRecentDevice(devices, "192.168.1.60:47777");
    Check(devices.empty(), "removing either spelling removes the device");
}

}

void RunRecentDevicesTests() {
    TestRoundTrip();
    TestLegacyPasscodesAreDiscarded();
    TestParseSkipsGarbage();
    TestParseDropsDuplicates();
    TestTouchMovesToFront();
    TestCapKeepsNewest();
    TestRemove();
    TestDefaultPortSpellingsAreOneDevice();
}
