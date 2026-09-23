#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/terminal/Screen.h"
#include "deskhub/terminal/VtParser.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;
using namespace deskhub::term;

namespace {

std::string ScrollbackText(const Screen& s, size_t row) {
    std::string out;
    for (uint16_t col = 0; col < kMaxTermCols; ++col) out += EncodeUtf8(s.ScrollbackAt(row, col).ch);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

const std::string kVimSession =
    "\x1B[?1049h"
    "\x1B[?1h\x1B="
    "\x1B[1;24r"
    "\x1B[?25l"
    "\x1B[H\x1B[2J"
    "\x1B[1;1H"
    "This is line one\r\n"
    "second line here"
    "\x1B[3;1H"
    "\x1B[34m~\x1B[m\r\n"
    "\x1B[34m~\x1B[m\r\n"
    "\x1B[34m~\x1B[m"
    "\x1B[24;1H"
    "\x1B[7m\"notes.txt\" 2L, 34C\x1B[m"
    "\x1B[1;1H"
    "\x1B[?25h";

const std::string kHtopFrame =
    "\x1B[H\x1B[2J"
    "\x1B[1;1H\x1B[1;32m  1  \x1B[m\x1B[42m    \x1B[m\x1B[44m      \x1B[m 12.5%\r\n"
    "\x1B[2;1H\x1B[1;32m  2  \x1B[m\x1B[42m  \x1B[m\x1B[44m        \x1B[m  6.2%\r\n"
    "\x1B[3;1H\x1B[38;5;208mMem\x1B[m\x1B[48;2;20;30;40m  4.1G/16.0G \x1B[m\r\n"
    "\x1B[5;1H\x1B[7m  PID USER      CPU% MEM%   TIME+  Command\x1B[m\r\n"
    "\x1B[6;1H 1234 manh       2.0  0.4  0:01.20 htop\r\n"
    "\x1B[7;1H 5678 manh       0.0  1.1  0:00.03 bash";

Screen MakeScreen(uint16_t cols = 40, uint16_t rows = 10) {
    return Screen(TermSize{cols, rows});
}

void TestPrintingAndWrap() {
    std::printf("[screen] text lands in the grid and wraps only when it must...\n");
    Screen s = MakeScreen(5, 3);
    s.Write("abcde");
    Check(s.RowText(0) == "abcde", "a line that exactly fills the width is written");
    Check(s.Cursor().row == 0 && s.Cursor().col == 4,
        "the cursor waits on the last column instead of jumping early");
    s.Write("f");
    Check(s.RowText(1) == "f", "the next character is what actually wraps");
    Check(s.Cursor().row == 1 && s.Cursor().col == 1, "and the cursor follows it");

    Screen noWrap = MakeScreen(5, 3);
    noWrap.Write("\x1B[?7l");
    noWrap.Write("abcdefgh");
    Check(noWrap.RowText(0) == "abcdh", "with autowrap off the last column is overwritten");
    Check(noWrap.Cursor().row == 0, "and the cursor never leaves the line");

    Screen wide = MakeScreen(10, 3);
    wide.Write("ti\xE1\xBA\xBFng vi\xE1\xBB\x87t");
    Check(wide.RowText(0) == "ti\xE1\xBA\xBFng vi\xE1\xBB\x87t",
        "UTF-8 text is stored by codepoint and reads back unchanged");
}

void TestCursorMovement() {
    std::printf("[screen] every cursor move is clamped inside the window...\n");
    Screen s = MakeScreen(10, 5);
    s.Write("\x1B[3;4H");
    Check(s.Cursor().row == 2 && s.Cursor().col == 3, "CUP is one-based on the wire");
    s.Write("\x1B[A");
    Check(s.Cursor().row == 1, "CUU steps up one row");
    s.Write("\x1B[2B");
    Check(s.Cursor().row == 3, "CUD takes a count");
    s.Write("\x1B[99A");
    Check(s.Cursor().row == 0, "moving further than the window allows stops at the edge");
    s.Write("\x1B[99C");
    Check(s.Cursor().col == 9, "and so does moving right");
    s.Write("\x1B[99D");
    Check(s.Cursor().col == 0, "and left");
    s.Write("\x1B[5G");
    Check(s.Cursor().col == 4, "CHA sets the column outright");
    s.Write("\x1B[4d");
    Check(s.Cursor().row == 3, "VPA sets the row outright");
    s.Write("\x1B[2;3f");
    Check(s.Cursor().row == 1 && s.Cursor().col == 2, "HVP is CUP by another name");
    s.Write("\x1B[E");
    Check(s.Cursor().row == 2 && s.Cursor().col == 0, "CNL goes down and to column one");
    s.Write("\x1B[F");
    Check(s.Cursor().row == 1 && s.Cursor().col == 0, "CPL goes back up the same way");

    s.Write("\x1B[H\tx");
    Check(s.Cursor().col == 9, "a tab jumps to the next eight-column stop");
    Check(s.RowText(0) == "        x", "and the character lands there");
    s.Write("\x1B[H\x1B[3I");
    Check(s.Cursor().col == 9, "a forward tab past the edge stops at the last column");
    s.Write("\x1B[1;9H\x1B[Z");
    Check(s.Cursor().col == 0, "CBT walks back to the previous stop");
    s.Write("\x1B[Z");
    Check(s.Cursor().col == 0, "and stops at the left edge");

    s.Write("\x1B[H\bx");
    Check(s.Cursor().col == 1, "backspace at column one cannot go further left");
}

void TestEraseAndInsert() {
    std::printf("[screen] the erase, insert and delete families...\n");
    Screen s = MakeScreen(10, 4);
    s.Write("abcdefghij");
    s.Write("\x1B[1;5H\x1B[K");
    Check(s.RowText(0) == "abcd", "EL clears from the cursor to the end of the line");

    s.Write("\x1B[2;1Habcdefghij\x1B[2;5H\x1B[1K");
    Check(s.RowText(1) == "     fghij", "EL 1 clears back to the start");

    s.Write("\x1B[3;1Habcdefghij\x1B[2K");
    Check(s.RowText(2) == "", "EL 2 clears the whole line");

    s.Write("\x1B[H\x1B[2J");
    Check(s.RowText(0).empty() && s.RowText(3).empty(), "ED 2 clears the screen");

    s.Write("\x1B[1;1Habc\x1B[2;1Hdef\x1B[3;1Hghi");
    s.Write("\x1B[2;2H\x1B[J");
    Check(s.RowText(0) == "abc" && s.RowText(1) == "d" && s.RowText(2).empty(),
        "ED 0 clears from the cursor to the bottom");

    s.Write("\x1B[1;1Habc\x1B[2;1Hdef\x1B[3;1Hghi");
    s.Write("\x1B[2;2H\x1B[1J");
    Check(s.RowText(0).empty() && s.RowText(1) == "  f" && s.RowText(2) == "ghi",
        "ED 1 clears from the top down to the cursor");

    s.Write("\x1B[H\x1B[2J\x1B[1;1Habcdef");
    s.Write("\x1B[1;3H\x1B[2P");
    Check(s.RowText(0) == "abef", "DCH pulls the rest of the line left");
    s.Write("\x1B[1;3H\x1B[2@");
    Check(s.RowText(0) == "ab  ef", "ICH pushes it back right");
    s.Write("\x1B[1;1H\x1B[3X");
    Check(s.RowText(0) == "    ef", "ECH blanks a run without moving anything");

    s.Write("\x1B[H\x1B[2J\x1B[1;1Hone\x1B[2;1Htwo\x1B[3;1Hthree");
    s.Write("\x1B[2;1H\x1B[L");
    Check(s.RowText(1).empty() && s.RowText(2) == "two", "IL opens a blank line and pushes down");
    s.Write("\x1B[2;1H\x1B[M");
    Check(s.RowText(1) == "two" && s.RowText(2) == "three", "DL takes it back out");

    s.Write(
        "\x1B[H\x1B[2J\x1B[4h"
        "abc\x1B[1;1Hxy");
    Check(s.RowText(0) == "xyabc", "insert mode pushes the line right as you type");
    s.Write("\x1B[4l\x1B[1;1Hz");
    Check(s.RowText(0) == "zyabc", "and replace mode overwrites again");
}

void TestScrollingAndScrollback() {
    std::printf("[screen] scrolling moves lines off the top into the scrollback...\n");
    Screen s(TermSize{10, 3}, 4);
    s.Write("one\r\ntwo\r\nthree\r\nfour");
    Check(s.RowText(0) == "two" && s.RowText(2) == "four", "the window follows the newest line");
    Check(s.ScrollbackRows() == 1 && ScrollbackText(s, 0) == "one",
        "the line that left the top is kept");

    s.Write("\r\nfive\r\nsix\r\nseven\r\neight");
    Check(s.ScrollbackRows() == 4, "the scrollback stops growing at its limit");
    Check(ScrollbackText(s, 0) == "two", "and drops its oldest line first");

    Screen r(TermSize{10, 5}, 10);
    r.Write("\x1B[2;4r");
    Check(r.Cursor().row == 0 && r.Cursor().col == 0, "setting a scroll region homes the cursor");
    r.Write("\x1B[2;1Ha\r\nb\r\nc\r\nd");
    Check(r.RowText(1) == "b" && r.RowText(3) == "d",
        "scrolling inside a region leaves the rows outside it alone");
    Check(r.ScrollbackRows() == 0, "and nothing outside the top row reaches the scrollback");

    r.Write("\x1B[r");
    r.Write("\x1B[1;1H\x1B[2J\x1B[1;1Ha\r\nb\r\nc");
    r.Write("\x1B[1;1H\x1B[2S");
    Check(r.RowText(0) == "c", "SU scrolls the whole window up");
    r.Write("\x1B[1;1H\x1B[T");
    Check(r.RowText(1) == "c", "SD scrolls it back down");

    Screen ri(TermSize{10, 3}, 10);
    ri.Write("a\r\nb\r\nc");
    ri.Write("\x1B[1;1H\x1BM");
    Check(ri.RowText(0).empty() && ri.RowText(1) == "a",
        "reverse index at the top opens a line above");
    ri.Write(
        "\x1B[3;1H\x1B"
        "E");
    Check(ri.Cursor().col == 0, "NEL goes to the start of the next line");
}

void TestBulkScrollEqualsRepeatedSingleScroll() {
    std::printf("[screen] scrolling n lines at once equals scrolling one line n times...\n");
    const auto seed = [](Screen& s) {
        s.Write("r0\r\nr1\r\nr2\r\nr3\r\nr4\r\nr5");
        s.Write("\x1B[2;5r");
    };
    const auto sameGrid = [](const Screen& a, const Screen& b, const char* what) {
        bool same = a.ScrollbackRows() == b.ScrollbackRows();
        for (uint16_t row = 0; same && row < 6; ++row) same = a.RowText(row) == b.RowText(row);
        for (size_t row = 0; same && row < a.ScrollbackRows(); ++row)
            same = ScrollbackText(a, row) == ScrollbackText(b, row);
        Check(same, what);
    };

    Screen bulkUp(TermSize{10, 6}, 8);
    Screen slowUp(TermSize{10, 6}, 8);
    seed(bulkUp);
    seed(slowUp);
    bulkUp.Write("\x1B[3S");
    slowUp.Write("\x1B[S\x1B[S\x1B[S");
    sameGrid(bulkUp, slowUp, "SU 3 inside a region matches three SU 1");
    Check(bulkUp.RowText(0) == "r0" && bulkUp.RowText(5) == "r5",
        "rows outside the region never move");
    Check(bulkUp.RowText(1) == "r4" && bulkUp.RowText(2).empty(),
        "the survivor lands at the region top and blanks fill the rest");

    Screen bulkDown(TermSize{10, 6}, 8);
    Screen slowDown(TermSize{10, 6}, 8);
    seed(bulkDown);
    seed(slowDown);
    bulkDown.Write("\x1B[3T");
    slowDown.Write("\x1B[T\x1B[T\x1B[T");
    sameGrid(bulkDown, slowDown, "SD 3 inside a region matches three SD 1");
    Check(bulkDown.RowText(4) == "r1" && bulkDown.RowText(1).empty(),
        "the survivor lands at the region bottom and blanks fill the top");

    Screen big(TermSize{10, 6}, 8);
    Screen small(TermSize{10, 6}, 8);
    seed(big);
    seed(small);
    big.Write("\x1B[9S");
    small.Write("\x1B[S\x1B[S\x1B[S\x1B[S\x1B[S\x1B[S\x1B[S\x1B[S\x1B[S");
    sameGrid(big, small, "SU past the region height matches the repeated form");
    Check(big.RowText(1).empty() && big.RowText(4).empty(), "and empties the whole region");

    Screen cap(TermSize{10, 3}, 3);
    cap.Write("a0\r\na1\r\na2");
    cap.Write("\x1B[1;1H\x1B[5S");
    Check(cap.ScrollbackRows() == 3, "a bulk full-window scroll still respects the limit");
    Check(ScrollbackText(cap, 0) == "a0" && ScrollbackText(cap, 1) == "a1" &&
              ScrollbackText(cap, 2) == "a2",
        "and the scrollback keeps the departed lines oldest-first");
}

void TestOriginMode() {
    std::printf("[screen] origin mode makes the scroll region the whole world...\n");
    Screen s = MakeScreen(10, 6);
    s.Write("\x1B[3;5r\x1B[?6h");
    Check(s.Cursor().row == 2, "turning origin mode on homes to the top of the region");
    s.Write("\x1B[1;1H");
    Check(s.Cursor().row == 2, "row one now means the first row of the region");
    s.Write("\x1B[99;1H");
    Check(s.Cursor().row == 4, "and the cursor cannot leave the region");
    s.Write("\x1B[6n");
    Check(s.TakeResponse() == "\x1B[3;1R", "a position report is relative to the region too");
    s.Write("\x1B[?6l");
    Check(s.Cursor().row == 0, "turning it off homes to the real top-left");
}

void TestSgr() {
    std::printf("[screen] colours and attributes stick to the cells they were set on...\n");
    Screen s = MakeScreen(20, 3);
    s.Write("\x1B[1;4;31mbold\x1B[m plain");
    Check(s.At(0, 0).pen.attrs == (kAttrBold | kAttrUnderline), "two attributes can be on at once");
    Check(s.At(0, 0).pen.fg == PaletteColor(1), "the foreground colour is remembered per cell");
    Check(s.At(0, 5).pen.attrs == 0 && s.At(0, 5).pen.fg == Color{},
        "SGR 0 puts everything back to the default");

    s.Write("\x1B[H\x1B[38;5;208mx");
    Check(s.At(0, 0).pen.fg == PaletteColor(208), "a 256-colour foreground is stored as its index");
    s.Write("\x1B[H\x1B[48;2;10;20;30my");
    Check(s.At(0, 0).pen.bg == RgbColor(10, 20, 30), "a true-colour background keeps all three parts");
    s.Write("\x1B[H\x1B[38:2::1:2:3mz");
    Check(s.At(0, 0).pen.fg == RgbColor(1, 2, 3),
        "the colon form with an empty colour space is understood too");
    s.Write("\x1B[H\x1B[38;9mq");
    Check(s.At(0, 0).pen.fg == Color{}, "an extended colour with no known kind falls back to default");

    s.Write("\x1B[H\x1B[90;101mb");
    Check(s.At(0, 0).pen.fg == PaletteColor(8) && s.At(0, 0).pen.bg == PaletteColor(9),
        "the bright ranges map onto palette entries 8 to 15");
    s.Write("\x1B[H\x1B[39;49mc");
    Check(s.At(0, 0).pen.fg == Color{} && s.At(0, 0).pen.bg == Color{},
        "39 and 49 reset one channel each");

    s.Write("\x1B[H\x1B[2;3;5;7;8;9md");
    const uint8_t all = kAttrDim | kAttrItalic | kAttrBlink | kAttrReverse | kAttrHidden |
                        kAttrStrike;
    Check(s.At(0, 0).pen.attrs == all, "every attribute we model can be switched on");
    s.Write(
        "\x1B[22;23;25;27;28;29m\x1B[H"
        "e");
    Check(s.At(0, 0).pen.attrs == 0, "and every one of them switched off again");
    s.Write(
        "\x1B[1m\x1B[21m\x1B[H"
        "f");
    Check(s.At(0, 0).pen.attrs == 0, "21 clears bold the way most terminals treat it");
    s.Write(
        "\x1B[4m\x1B[24m\x1B[H"
        "g");
    Check(s.At(0, 0).pen.attrs == 0, "24 clears the underline");
    s.Write("\x1B[7m\x1B[H\x1B[K");
    Check(s.At(0, 3).pen.bg == Color{}, "erasing with only reverse set does not stain the row");
    s.Write("\x1B[m\x1B[41m\x1B[H\x1B[K");
    Check(s.At(0, 3).pen.bg == PaletteColor(1),
        "but erasing with a background colour set paints it, as every terminal does");
}

void TestAlternateScreen() {
    std::printf("[screen] the alternate screen keeps the shell's output safe...\n");
    Screen s = MakeScreen(10, 4);
    s.Write("shell line\r\n$ vim");
    Check(!s.AlternateScreen(), "we start on the primary screen");
    s.Write("\x1B[?1049h");
    Check(s.AlternateScreen(), "1049 switches over");
    Check(s.RowText(0).empty(), "and hands the program a clean screen");
    s.Write("editor");
    s.Write("\x1B[?1049l");
    Check(!s.AlternateScreen() && s.RowText(0) == "shell line",
        "leaving it puts the shell's output back exactly as it was");
    Check(s.Cursor().row == 1 && s.Cursor().col == 5, "and the cursor with it");

    s.Write(
        "\x1B[?47h"
        "alt\x1B[?47l");
    Check(!s.AlternateScreen() && s.RowText(0) == "shell line",
        "the older 47 switch behaves the same way");

    Screen sb(TermSize{10, 2}, 10);
    sb.Write(
        "\x1B[?1049h"
        "a\r\nb\r\nc\r\nd");
    Check(sb.ScrollbackRows() == 0, "nothing scrolled off the alternate screen is kept");
}

void TestCharsetsAndReset() {
    std::printf("[screen] line-drawing charset, saved cursor and hard reset...\n");
    Screen s = MakeScreen(10, 3);
    s.Write("\x1B(0lqk\x1B(B");
    Check(s.At(0, 0).ch == U'\u250C' && s.At(0, 1).ch == U'\u2500' && s.At(0, 2).ch == U'\u2510',
        "the DEC graphics set draws boxes instead of letters");
    s.Write("\x1B[Hx");
    Check(s.At(0, 0).ch == U'x', "and switching back prints letters again");

    s.Write("\x1B)0\x0Eq\x0Fq");
    Check(s.At(0, 1).ch == U'\u2500' && s.At(0, 2).ch == U'q',
        "shift-out selects G1 and shift-in comes back to G0");

    s.Write(
        "\x1B[3;5H\x1B[1m\x1B"
        "7\x1B[1;1H\x1B[m\x1B"
        "8");
    Check(s.Cursor().row == 2 && s.Cursor().col == 4, "ESC 8 restores where ESC 7 left the cursor");
    Check(s.CurrentPen().attrs == kAttrBold, "and the pen that was in force at the time");

    s.Write("\x1B[5;5H\x1B[s\x1B[1;1H\x1B[u");
    Check(s.Cursor().row == 2 && s.Cursor().col == 4,
        "the CSI form of save and restore works the same, clamped to the window");

    s.Write("\x1B#8");
    Check(s.RowText(0) == "EEEEEEEEEE", "the alignment pattern fills the screen");

    s.Write(
        "\x1B[?25l\x1B[1;30m\x1B"
        "c");
    Check(s.RowText(0).empty(), "a hard reset clears the screen");
    Check(s.Cursor() == CursorState{} && s.CurrentPen() == Pen{},
        "and puts the cursor, the pen and the cursor visibility back");

    Screen r = MakeScreen(10, 3);
    r.Write("text\x1B[?25l");
    r.Reset();
    Check(r.RowText(0).empty() && r.ScrollbackRows() == 0 && r.Cursor().visible,
        "Reset() empties the scrollback too");
}

void TestModesAndReports() {
    std::printf("[screen] the modes a program asks about, and what we answer...\n");
    Screen s = MakeScreen(20, 5);
    s.Write("\x1B[?1h");
    Check(s.Modes().applicationCursor, "application cursor keys can be turned on");
    s.Write("\x1B[?1l");
    Check(!s.Modes().applicationCursor, "and off");
    s.Write("\x1B=");
    Check(s.Modes().applicationKeypad, "so can the application keypad");
    s.Write("\x1B>");
    Check(!s.Modes().applicationKeypad, "and off again");
    s.Write("\x1B[?2004h");
    Check(s.Modes().bracketedPaste, "bracketed paste is a mode we have to track for the client");
    s.Write("\x1B[?1000h");
    Check(s.Modes().mouseReporting, "so is mouse reporting");
    s.Write("\x1B[?5h");
    Check(s.Modes().reverseVideo, "and reverse video");
    s.Write("\x1B[?25l");
    Check(!s.Cursor().visible, "hiding the cursor is a mode too");
    s.Write("\x1B[?9999h");
    Check(true, "a mode we do not know is ignored rather than crashing");

    s.Write("\x1B[c");
    Check(s.TakeResponse() == "\x1B[?1;2c", "we answer the device attributes question");
    Check(s.TakeResponse().empty(), "and only once");
    s.Write("\x1B[5n");
    Check(s.TakeResponse() == "\x1B[0n", "a status report says we are fine");
    s.Write("\x1B[2;7H\x1B[6n");
    Check(s.TakeResponse() == "\x1B[2;7R", "a position report is one-based like the wire");

    for (int i = 0; i < 500; ++i) s.Write("\x1B[6n");
    Check(s.TakeResponse().size() <= kMaxResponseBytes,
        "a program that asks over and over cannot make the reply buffer grow without limit");

    Screen title = MakeScreen(20, 3);
    title.Write("\x1B]0;my shell\x07");
    Check(title.Title() == "my shell", "OSC 0 sets the window title");
    title.Write("\x1B]2;other\x1B\\");
    Check(title.Title() == "other", "OSC 2 does the same");
    title.Write("\x1B]52;c;junk\x07");
    Check(title.Title() == "other", "an OSC we do not implement leaves the title alone");
    std::string huge = "\x1B]0;";
    huge.append(kMaxTitleBytes + 100, 'x');
    huge += "\x07";
    title.Write(huge);
    Check(title.Title().size() == kMaxTitleBytes, "and a runaway title is bounded");
}

void TestResize() {
    std::printf("[screen] resizing keeps what is on screen and clamps the cursor...\n");
    Screen s(TermSize{10, 4}, 10);
    s.Write("one\r\ntwo\r\nthree\r\nfour");
    s.Resize(TermSize{10, 2});
    Check(s.Size().rows == 2, "the window really shrinks");
    Check(s.RowText(1) == "four", "the newest line stays on screen");
    Check(s.ScrollbackRows() == 2 && ScrollbackText(s, 1) == "two",
        "the rows pushed off the top are kept");
    Check(s.Cursor().row < 2, "and the cursor is still inside the window");

    s.Resize(TermSize{4, 2});
    Check(s.RowText(1) == "four", "narrowing keeps as much of each line as fits");
    s.Resize(TermSize{12, 4});
    Check(s.Size().cols == 12 && s.Size().rows == 4, "growing back works too");
    Check(s.RowText(0) == "thre" && s.RowText(1) == "four",
        "the rows that were on screen keep the columns that survived the narrowing");
    Check(s.RowText(3).empty(), "and the new rows are blank rather than invented");

    s.Resize(TermSize{12, 4});
    Check(s.Size().cols == 12, "resizing to the size we already are changes nothing");
    s.Resize(TermSize{0, 0});
    Check(s.Size().cols == kMinTermCols && s.Size().rows == kMinTermRows,
        "an impossible size is clamped rather than accepted");

    Screen alt(TermSize{10, 4}, 10);
    alt.Write(
        "\x1B[?1049h"
        "a\r\nb\r\nc\r\nd");
    alt.Resize(TermSize{10, 2});
    Check(alt.ScrollbackRows() == 0, "shrinking the alternate screen never fills the scrollback");
    alt.Write("\x1B[?1049l");
    Check(alt.Size().rows == 2, "and coming back from it keeps the new size");

    Screen region(TermSize{10, 6}, 10);
    region.Write("\x1B[2;4r\x1B[3;3H");
    region.Resize(TermSize{10, 3});
    region.Write("\x1B[99;99H");
    Check(region.Cursor().row == 2 && region.Cursor().col == 9,
        "the scroll region is dropped on resize so the cursor can reach the whole window");
}

void TestVimReplay() {
    std::printf("[screen] a recorded vim session lands on the expected grid...\n");
    Screen s(TermSize{40, 24}, 100);
    s.Write(kVimSession);

    Check(s.AlternateScreen(), "vim moved onto the alternate screen");
    Check(s.Modes().applicationCursor, "and asked for application cursor keys");
    Check(s.Modes().applicationKeypad, "and the application keypad");
    Check(s.RowText(0) == "This is line one", "the first line of the file is on screen");
    Check(s.RowText(1) == "second line here", "and the second");
    Check(s.RowText(2) == "~" && s.RowText(4) == "~",
        "the empty-buffer markers are drawn below it");
    Check(s.At(2, 0).pen.fg == PaletteColor(4), "and they are drawn in blue");
    Check(s.RowText(23) == "\"notes.txt\" 2L, 34C", "the status line is at the bottom");
    Check(s.At(23, 0).pen.attrs == kAttrReverse, "shown in reverse video");
    Check(s.Cursor().row == 0 && s.Cursor().col == 0 && s.Cursor().visible,
        "and the cursor is back at the top of the file");

    s.Write("\x1B[?1049l\x1B[?1l\x1B>");
    Check(!s.AlternateScreen() && !s.Modes().applicationCursor &&
              !s.Modes().applicationKeypad,
        "quitting vim puts every mode back the way it found them");
}

void TestHtopReplay() {
    std::printf("[screen] a recorded htop frame lands on the expected grid...\n");
    Screen s(TermSize{60, 20}, 100);
    s.Write(kHtopFrame);

    Check(s.RowText(4) == "  PID USER      CPU% MEM%   TIME+  Command",
        "the process table header is drawn");
    Check(s.At(4, 0).pen.attrs == kAttrReverse, "in reverse video, the way htop draws it");
    Check(s.RowText(5).find("htop") != std::string::npos, "the first process row is there");
    Check(s.RowText(6).find("bash") != std::string::npos, "and the second");
    Check(s.At(0, 0).pen.fg == PaletteColor(2) &&
              (s.At(0, 0).pen.attrs & kAttrBold) != 0,
        "the CPU number is bold green");
    Check(s.At(0, 5).pen.bg == PaletteColor(2), "the used part of the meter is green");
    Check(s.At(0, 9).pen.bg == PaletteColor(4), "and the rest of it blue");
    Check(s.At(2, 0).pen.fg == PaletteColor(208), "the memory label uses a 256-colour entry");
    Check(s.At(2, 4).pen.bg == RgbColor(20, 30, 40), "and its bar a true-colour background");
    Check(s.RowText(3).empty(), "the row htop skipped is left blank");

    const uint64_t before = s.Revision();
    s.Write(kHtopFrame);
    Check(s.Revision() > before, "redrawing the frame tells the client something changed");
    Check(s.RowText(4) == "  PID USER      CPU% MEM%   TIME+  Command",
        "and the second frame is identical to the first");
}

void TestSplitWrites() {
    std::printf("[screen] the same bytes give the same grid however they are split up...\n");
    Screen whole(TermSize{40, 24}, 100);
    whole.Write(kVimSession);

    for (size_t cut : {size_t(1), size_t(7), size_t(23), kVimSession.size() - 1}) {
        Screen split(TermSize{40, 24}, 100);
        split.Write(std::string_view(kVimSession).substr(0, cut));
        split.Write(std::string_view(kVimSession).substr(cut));
        Check(split.Text() == whole.Text(),
            "an escape sequence cut in half still ends up on the right cells");
        Check(split.Cursor() == whole.Cursor(), "and leaves the cursor in the same place");
    }

    Screen byteByByte(TermSize{60, 20}, 100);
    for (char c : kHtopFrame) byteByByte.Write(std::string_view(&c, 1));
    Screen atOnce(TermSize{60, 20}, 100);
    atOnce.Write(kHtopFrame);
    Check(byteByByte.Text() == atOnce.Text(),
        "even one byte at a time gives the same picture");

    Screen empty(TermSize{10, 3}, 10);
    const uint64_t rev = empty.Revision();
    empty.Write(std::span<const uint8_t>());
    Check(empty.Revision() == rev, "an empty write is not a change");
}

void TestOutOfRangeReads() {
    std::printf("[screen] reading outside the grid gives a blank cell, never a crash...\n");
    Screen s = MakeScreen(5, 3);
    s.Write("abc");
    Check(s.At(99, 0) == Cell{} && s.At(0, 99) == Cell{},
        "a cell past the edge reads as blank");
    Check(s.RowText(99).empty(), "so does a row past the bottom");
    Check(s.ScrollbackAt(99, 0) == Cell{} && ScrollbackText(s, 99).empty(),
        "and a scrollback row that does not exist");
    Check(s.Text().find("abc") == 0, "the whole-screen dump starts with what we wrote");
}

void TestGarbageStream() {
    std::printf("[screen] 400 rounds of random bytes leave the model consistent...\n");
    Screen s(TermSize{40, 12}, 50);
    for (int round = 0; round < 400; ++round) {
        std::vector<uint8_t> soup(Rnd() % 300);
        for (auto& b : soup) b = uint8_t(Rnd());
        s.Write(soup);
        s.TakeResponse();

        const TermSize size = s.Size();
        Check(size.cols >= kMinTermCols && size.rows >= kMinTermRows, "the window stays real");
        Check(s.Cursor().row < size.rows && s.Cursor().col < size.cols,
            "the cursor never leaves the grid");
        Check(s.ScrollbackRows() <= 50, "the scrollback never passes its limit");
        Check(s.Title().size() <= kMaxTitleBytes, "the title never passes its limit");
        for (uint16_t r = 0; r < size.rows; ++r)
            Check(s.RowText(r).size() <= size_t(size.cols) * 4,
                "no row ever holds more than its cells can encode");
        if (round % 37 == 0)
            s.Resize(TermSize{uint16_t(10 + Rnd() % 60), uint16_t(3 + Rnd() % 20)});
    }
    Check(true, "the screen survived the whole soup");
}

}

void RunScreenTests() {
    TestPrintingAndWrap();
    TestCursorMovement();
    TestEraseAndInsert();
    TestScrollingAndScrollback();
    TestBulkScrollEqualsRepeatedSingleScroll();
    TestOriginMode();
    TestSgr();
    TestAlternateScreen();
    TestCharsetsAndReset();
    TestModesAndReports();
    TestResize();
    TestVimReplay();
    TestHtopReplay();
    TestSplitWrites();
    TestOutOfRangeReads();
    TestGarbageStream();
}
