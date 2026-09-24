#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/SettingsLayout.h"
#include "deskhub/ui/Strings.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace deskhub;

namespace {

std::vector<const char*> AreaTitles() {
    std::vector<const char*> titles;
    for (const ui::SettingsEntry& entry : ui::DesktopSettingsLayout())
        if (entry.kind == ui::SettingsEntryKind::Area) titles.push_back(entry.text);
    return titles;
}

const char* AreaOf(ui::SettingField field) {
    const char* area = nullptr;
    for (const ui::SettingsEntry& entry : ui::DesktopSettingsLayout()) {
        if (entry.kind == ui::SettingsEntryKind::Area) area = entry.text;
        if (entry.kind == ui::SettingsEntryKind::Setting && entry.field == field) return area;
    }
    return nullptr;
}

bool InArea(ui::SettingField field, const char* area) {
    const char* found = AreaOf(field);
    return found != nullptr && std::strcmp(found, area) == 0;
}

void TestEverySettingShowsUpExactlyOnce() {
    std::printf("[settings] every stored setting has exactly one place on the page...\n");
    std::array<int, size_t(ui::SettingField::Count)> seen{};
    for (const ui::SettingsEntry& entry : ui::DesktopSettingsLayout())
        if (entry.kind == ui::SettingsEntryKind::Setting) ++seen[size_t(entry.field)];
    Check(seen[size_t(ui::SettingField::None)] == 0, "no setting row is left without a field");
    for (size_t field = size_t(ui::SettingField::None) + 1; field < seen.size(); ++field)
        Check(seen[field] == 1, "each setting appears once, so no app can drop or repeat it");
}

void TestThePageSplitsIntoHostClientAndGeneral() {
    std::printf("[settings] the page is the Host box, then Client, then General...\n");
    const std::vector<const char*> titles = AreaTitles();
    Check(titles.size() == 3, "three boxes");
    Check(titles.size() == 3 && std::strcmp(titles[0], ui::kSidebarHost) == 0, "Host first");
    Check(titles.size() == 3 && std::strcmp(titles[1], ui::kSidebarClient) == 0, "Client next");
    Check(titles.size() == 3 && std::strcmp(titles[2], ui::kSettingsGeneralArea) == 0,
        "General last");
    Check(ui::DesktopSettingsLayout().front().kind == ui::SettingsEntryKind::Area,
        "nothing floats above the first box");
}

void TestEachSettingSitsOnTheSideThatUsesIt() {
    std::printf("[settings] a setting lives in the box of the side that reads it...\n");
    Check(InArea(ui::SettingField::Passcode, ui::kSidebarHost), "the passcode guards sharing");
    Check(InArea(ui::SettingField::PlayAudio, ui::kSidebarClient), "playing sound is watching");
    Check(InArea(ui::SettingField::Port, ui::kSettingsGeneralArea),
        "the port is where the host listens and where the scan knocks");
    Check(InArea(ui::SettingField::ClipboardSync, ui::kSettingsGeneralArea),
        "each machine in a session needs its own clipboard switch");
    Check(InArea(ui::SettingField::KeepAwake, ui::kSettingsGeneralArea),
        "hosts and viewers alike stay awake");
}

void TestEveryVisibleEntryHasText() {
    std::printf("[settings] boxes, hints, sections and settings all carry their text...\n");
    for (const ui::SettingsEntry& entry : ui::DesktopSettingsLayout()) {
        const bool drawnByTheApp = entry.field == ui::SettingField::Permissions;
        Check(entry.text != nullptr, "no entry points at nothing");
        if (!drawnByTheApp)
            Check(entry.text != nullptr && entry.text[0] != '\0', "every row says something");
    }
}

void TestEachSwitchWritesItsOwnFlag() {
    std::printf("[settings] ticking a switch changes that one stored flag and nothing else...\n");
    for (size_t raw = 0; raw < size_t(ui::SettingField::Count); ++raw) {
        const auto field = ui::SettingField(raw);
        ui::UiSettings settings;
        bool* flag = ui::SettingFlag(settings, field);
        if (flag == nullptr) continue;
        const ui::UiSettings before = settings;
        *flag = !*flag;
        Check(!(settings == before), "the switch reaches the stored settings");
        *flag = !*flag;
        Check(settings == before, "and flipping it back restores them exactly");
    }
    ui::UiSettings settings;
    Check(ui::SettingFlag(settings, ui::SettingField::CloseToTray) == &settings.startHidden,
        "keep running in the background is the start-hidden flag");
    Check(ui::SettingFlag(settings, ui::SettingField::Fps) == nullptr,
        "a number is not a switch");
    Check(ui::SettingFlag(settings, ui::SettingField::Count) == nullptr,
        "past the end is not a switch");
}

}

void RunSettingsLayoutTests() {
    TestEverySettingShowsUpExactlyOnce();
    TestThePageSplitsIntoHostClientAndGeneral();
    TestEachSettingSitsOnTheSideThatUsesIt();
    TestEveryVisibleEntryHasText();
    TestEachSwitchWritesItsOwnFlag();
}
