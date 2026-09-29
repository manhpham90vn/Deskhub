#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/ClientKeys.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

void TestKeyNames() {
    std::printf("[keys] a key name is a plain file-safe word...\n");
    Check(ui::IsValidClientKeyName("default") && ui::IsValidClientKeyName("work-laptop_2"),
        "letters, digits, '-' and '_' are allowed");
    Check(!ui::IsValidClientKeyName(""), "an empty name is refused");
    Check(!ui::IsValidClientKeyName("-work"), "a name cannot start with '-'");
    Check(!ui::IsValidClientKeyName("_work"), "or with '_'");
    Check(!ui::IsValidClientKeyName("../id"), "path characters are refused");
    Check(!ui::IsValidClientKeyName("my key"), "a space is refused");
    Check(!ui::IsValidClientKeyName(std::string(ui::kMaxClientKeyNameBytes + 1, 'a')),
        "an over-long name is refused");
}

void TestPublicKeysCarryAReadableLabel() {
    std::printf("[keys] a copied public key names the machine it came from...\n");
    Check(ui::ClientKeyLabel("manh-laptop", "default") == "manh-laptop",
        "the default key is labelled with the machine name");
    Check(ui::ClientKeyLabel("manh-laptop", "work") == "manh-laptop (work)",
        "another key also names itself");
    Check(ui::ClientKeyLabel("", "work") == "work", "a machine without a name still has a label");
    Check(ui::ClientKeyLabel("tab\there", "default") == "tabhere",
        "control characters never reach the key line");
    Check(ui::ClientKeyLabel(std::string(200, 'a'), "work").size() <= kMaxClientNameBytes,
        "a long name is cut to what a key line can carry");
}

void TestEveryKeyErrorHasText() {
    std::printf("[keys] every key refusal can be shown to the user...\n");
    for (uint8_t e = 1; e <= uint8_t(ui::ClientKeyError::WriteFailed); ++e)
        Check(*ui::ClientKeyErrorText(ui::ClientKeyError(e)) != '\0', "each error has a sentence");
    Check(*ui::ClientKeyErrorText(ui::ClientKeyError::None) == '\0', "success says nothing");
}

}

void RunClientKeysTests() {
    TestKeyNames();
    TestPublicKeysCarryAReadableLabel();
    TestEveryKeyErrorHasText();
}
