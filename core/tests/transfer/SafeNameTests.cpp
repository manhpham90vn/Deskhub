#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/transfer/SafeName.h"

#include <cstdio>
#include <set>
#include <string>

using namespace deskhub;

namespace {

void TestOrdinaryNamesSurvive() {
    std::printf("[name] an ordinary file name is passed through unchanged...\n");
    Check(SafeFileName("report.pdf") == "report.pdf", "a plain name is left alone");
    Check(SafeFileName("Ảnh chụp màn hình.png") == "Ảnh chụp màn hình.png",
        "a UTF-8 name keeps its accents");
    Check(SafeFileName("archive.tar.gz") == "archive.tar.gz", "a double extension survives");
    Check(SafeFileName(".gitignore") == ".gitignore", "a leading dot is not an escape");
}

void TestDirectoryComponentsAreStripped() {
    std::printf("[name] every path component is stripped before the name is used...\n");
    Check(SafeFileName("/etc/passwd") == "passwd", "an absolute POSIX path keeps only the leaf");
    Check(SafeFileName("../../.ssh/authorized_keys") == "authorized_keys",
        "a traversal path keeps only the leaf");
    Check(SafeFileName("C:\\Windows\\System32\\drivers\\etc\\hosts") == "hosts",
        "a Windows path keeps only the leaf");
    Check(SafeFileName("..").empty(), "a bare parent reference yields nothing");
    Check(SafeFileName(".").empty(), "a bare current reference yields nothing");
    Check(SafeFileName("dir/..").empty(), "a trailing parent reference yields nothing");
    Check(SafeFileName("").empty(), "an empty name yields nothing");
}

void TestWindowsHostilityIsDefused() {
    std::printf("[name] names Windows would read as something else are defused...\n");
    Check(SafeFileName("a:stream.txt") == "a_stream.txt",
        "an alternate data stream colon becomes an underscore");
    Check(SafeFileName("what?.txt") == "what_.txt", "a wildcard becomes an underscore");
    Check(SafeFileName("pipe|it") == "pipe_it", "a pipe becomes an underscore");
    Check(SafeFileName("trailing.txt.") == "trailing.txt", "a trailing dot is dropped");
    Check(SafeFileName("trailing.txt   ") == "trailing.txt", "trailing spaces are dropped");
    Check(SafeFileName("CON") == "_CON", "a reserved device name is pushed out of the way");
    Check(SafeFileName("nul.txt") == "_nul.txt", "a reserved stem is caught with an extension");
    Check(SafeFileName("COM4") == "_COM4", "a numbered serial-port name is caught");
    Check(SafeFileName("lpt9.log") == "_lpt9.log", "a numbered printer name is caught");
    Check(SafeFileName("console.txt") == "console.txt", "a longer word is not a device name");
    Check(SafeFileName("com10.txt") == "com10.txt", "a two-digit port is not a device name");
    Check(SafeFileName("CONIN$") == "_CONIN$", "the console input device is caught");
    Check(SafeFileName("conout$.txt") == "_conout$.txt", "and so is the console output one");
    Check(SafeFileName("COM\xC2\xB9") == "_COM\xC2\xB9",
        "a superscript-numbered serial port is caught");
    Check(SafeFileName("lpt\xC2\xB3.log") == "_lpt\xC2\xB3.log",
        "and a superscript-numbered printer too");
    Check(SafeFileName("com\xE2\x81\xB4.txt") == "com\xE2\x81\xB4.txt",
        "a superscript Windows never reserved is left alone");
    Check(SafeFileName("nul .txt") == "_nul .txt",
        "a reserved stem followed by spaces before the dot is still caught");
    Check(SafeFileName("company.txt") == "company.txt",
        "a word that merely starts like a port is not a device name");
}

void TestControlCharactersAreRemoved() {
    std::printf("[name] control bytes never reach the filesystem...\n");
    Check(SafeFileName(std::string("we\x01ird.txt")) == "weird.txt", "a control byte is removed");
    Check(SafeFileName(std::string("nul\0byte.txt", 12)) == "nulbyte.txt",
        "an embedded NUL is removed");
    Check(SafeFileName(std::string("\x7F")).empty(), "a name of only DEL yields nothing");
    Check(SafeFileName("new\nline.txt") == "newline.txt", "a newline is removed");
}

void TestLongNamesAreShortened() {
    std::printf("[name] an over-long name is cut on a character boundary...\n");
    const std::string longStem(400, 'x');
    const std::string shortened = SafeFileName(longStem + ".png");
    Check(shortened.size() <= kMaxSafeNameBytes, "the result fits the filesystem limit");
    Check(shortened.size() > 4 && shortened.substr(shortened.size() - 4) == ".png",
        "and it keeps its extension");

    std::string accented;
    for (int i = 0; i < 200; ++i) accented += "ệ";
    const std::string cut = SafeFileName(accented + ".txt");
    Check(cut.size() <= kMaxSafeNameBytes, "a UTF-8 name is cut to the limit");
    Check(cut.size() > 4 && cut.substr(cut.size() - 4) == ".txt", "it keeps its extension");
    Check((cut.size() - 4) % 3 == 0, "and the cut never lands inside a character");
}

void TestWireLegalityIsNarrowerThanSafety() {
    std::printf("[name] the parser rejects what the wire must never carry...\n");
    Check(IsWireLegalFileName("report.pdf"), "an ordinary name is legal on the wire");
    Check(!IsWireLegalFileName("../secret"), "a name with a separator is not");
    Check(!IsWireLegalFileName("dir\\file"), "a Windows separator is not either");
    Check(!IsWireLegalFileName(""), "an empty name is not");
    Check(!IsWireLegalFileName("."), "a current-directory reference is not");
    Check(!IsWireLegalFileName(".."), "a parent-directory reference is not");
    Check(!IsWireLegalFileName(std::string("a\0b", 3)), "an embedded NUL is not");
    Check(!IsWireLegalFileName(std::string(kMaxTransferNameBytes + 1, 'x')),
        "an over-long name is not");
    Check(IsWireLegalFileName(std::string(kMaxTransferNameBytes, 'x')),
        "a name exactly at the limit is");
}

void TestCollisionsGetTheirOwnName() {
    std::printf("[name] a colliding name is renamed, never overwritten...\n");
    std::set<std::string> present{"photo.png", "photo (2).png", "notes"};
    const auto taken = [&present](const std::string& n) { return present.count(n) != 0; };

    Check(UniqueFileName("fresh.png", taken) == "fresh.png", "a free name is used as it is");
    Check(UniqueFileName("photo.png", taken) == "photo (3).png",
        "a taken name skips past every taken variant");
    Check(UniqueFileName("notes", taken) == "notes (2)",
        "an extensionless name is disambiguated too");

    const std::string longStem(250, 'y');
    present.insert(longStem + ".png");
    const std::string renamed = UniqueFileName(longStem + ".png", taken);
    Check(renamed.size() <= kMaxSafeNameBytes, "the renamed file still fits the limit");
    Check(renamed.find(" (2)") != std::string::npos,
        "and shortening never eats the disambiguating suffix");

    const auto everythingTaken = [](const std::string&) { return true; };
    Check(UniqueFileName("busy.png", everythingTaken).empty(),
        "a directory that answers yes forever gives up rather than looping");
}

}

void RunSafeNameTests() {
    TestOrdinaryNamesSurvive();
    TestDirectoryComponentsAreStripped();
    TestWindowsHostilityIsDefused();
    TestControlCharactersAreRemoved();
    TestLongNamesAreShortened();
    TestWireLegalityIsNarrowerThanSafety();
    TestCollisionsGetTheirOwnName();
}
