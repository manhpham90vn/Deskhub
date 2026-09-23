#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/ShellPicker.h"
#include "deskhub/ui/Strings.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

TermSessionEntry MakeEntry(uint32_t termId, TerminalState state, std::string clientName) {
    TermSessionEntry entry;
    entry.termId = termId;
    entry.state = state;
    entry.size = TermSize{80, 24};
    entry.clientName = std::move(clientName);
    return entry;
}

void TestEmptyList() {
    std::printf("[shellpicker] a host keeping nothing offers nothing to pick...\n");
    Check(ui::BuildShellPickerRows(TermSessionList{}).empty(), "no sessions, no rows");
}

void TestRowsDescribeEachShell() {
    std::printf("[shellpicker] every kept shell becomes a row saying whose it is...\n");
    TermSessionList list;
    list.sessions.push_back(MakeEntry(3, TerminalState::Detached, "Pixel 9"));
    list.sessions.push_back(MakeEntry(4, TerminalState::Live, "MacBook"));
    list.sessions.push_back(MakeEntry(5, TerminalState::Local, "MacBook"));

    const std::vector<ui::ShellPickerRow> rows = ui::BuildShellPickerRows(list);
    Check(rows.size() == 3, "one row per shell, whatever its state");

    Check(rows[0].termId == 3 && rows[0].label == "#3  80x24", "the id and grid name the row");
    Check(rows[0].detail == std::string("Pixel 9 ") + ui::kTerminalDetached,
        "a detached shell names the machine that left it");
    Check(rows[0].resumable && rows[0].closable, "and it can be picked up or closed");

    Check(rows[1].detail == std::string("MacBook ") + ui::kTerminalInUse,
        "a shell someone is typing in says so");
    Check(!rows[1].resumable, "and cannot be taken over while they hold it");
    Check(rows[1].closable, "though it can still be closed");

    Check(rows[2].detail == ui::kTerminalLocalElsewhere,
        "a shell taken over at the host belongs to the host");
    Check(!rows[2].resumable && !rows[2].closable, "and is neither ours to resume nor to close");
}

void TestUnnamedAndSkipped() {
    std::printf("[shellpicker] a nameless machine still reads as something...\n");
    TermSessionList list;
    list.sessions.push_back(MakeEntry(7, TerminalState::Detached, ""));
    list.sessions.push_back(MakeEntry(0, TerminalState::Detached, "nobody"));

    const std::vector<ui::ShellPickerRow> rows = ui::BuildShellPickerRows(list);
    Check(rows.size() == 1, "a session with no id is not a session");
    Check(rows[0].detail == std::string(ui::kTerminalUnnamedClient) + " " + ui::kTerminalDetached,
        "and an unnamed one is described rather than left blank");
    Check(ui::ShellPickerLine(rows[0]) == rows[0].label + "  " + rows[0].detail,
        "the one-line form is the label and the detail");
}

}

void RunShellPickerTests() {
    TestEmptyList();
    TestRowsDescribeEachShell();
    TestUnnamedAndSkipped();
}
