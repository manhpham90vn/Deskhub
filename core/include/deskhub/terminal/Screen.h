#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/terminal/VtParser.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub::term {

inline constexpr size_t kDefaultScrollback = 2000;
inline constexpr size_t kMaxScrollback = 100000;
inline constexpr uint16_t kTabWidth = 8;
inline constexpr size_t kMaxTitleBytes = 256;
inline constexpr size_t kMaxResponseBytes = 4096;

enum class ColorKind : uint8_t {
    Default = 0,
    Palette = 1,
    Rgb = 2,
};

struct Color {
    ColorKind kind = ColorKind::Default;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const Color&) const = default;
};

constexpr Color PaletteColor(uint8_t index) {
    return Color{ColorKind::Palette, index, 0, 0};
}

constexpr Color RgbColor(uint8_t r, uint8_t g, uint8_t b) {
    return Color{ColorKind::Rgb, r, g, b};
}

inline constexpr uint8_t kAttrBold = 1u << 0;
inline constexpr uint8_t kAttrDim = 1u << 1;
inline constexpr uint8_t kAttrItalic = 1u << 2;
inline constexpr uint8_t kAttrUnderline = 1u << 3;
inline constexpr uint8_t kAttrBlink = 1u << 4;
inline constexpr uint8_t kAttrReverse = 1u << 5;
inline constexpr uint8_t kAttrHidden = 1u << 6;
inline constexpr uint8_t kAttrStrike = 1u << 7;

struct Pen {
    Color fg{};
    Color bg{};
    uint8_t attrs = 0;

    bool operator==(const Pen&) const = default;
};

struct Cell {
    char32_t ch = U' ';
    Pen pen{};

    bool operator==(const Cell&) const = default;
};

struct CursorState {
    uint16_t row = 0;
    uint16_t col = 0;
    bool visible = true;

    bool operator==(const CursorState&) const = default;
};

struct TerminalModes {
    bool applicationCursor = false;
    bool applicationKeypad = false;
    bool bracketedPaste = false;
    bool autoWrap = true;
    bool insert = false;
    bool origin = false;
    bool mouseReporting = false;
    bool reverseVideo = false;

    bool operator==(const TerminalModes&) const = default;
};

struct ScrollRegion {
    uint16_t top = 0;
    uint16_t bottom = 0;

    bool operator==(const ScrollRegion&) const = default;
};

struct CharsetState {
    bool graphicsG0 = false;
    bool graphicsG1 = false;
    bool shiftedOut = false;

    bool operator==(const CharsetState&) const = default;
};

class Screen {
public:
    explicit Screen(TermSize size = TermSize{}, size_t scrollbackLimit = kDefaultScrollback);

    void Write(std::span<const uint8_t> bytes);
    void Write(std::string_view text);
    void Resize(TermSize size);
    void Reset();

    TermSize Size() const {
        return size_;
    }
    const Cell& At(uint16_t row, uint16_t col) const;
    std::string RowText(uint16_t row) const;
    std::string Text() const;

    CursorState Cursor() const {
        return cursor_;
    }
    const TerminalModes& Modes() const {
        return modes_;
    }
    const Pen& CurrentPen() const {
        return pen_;
    }
    std::string_view Title() const {
        return title_;
    }
    bool AlternateScreen() const {
        return alternate_;
    }
    ScrollRegion Region() const {
        return ScrollRegion{scrollTop_, scrollBottom_};
    }
    CharsetState Charset() const {
        return CharsetState{graphicsG0_, graphicsG1_, shiftedOut_};
    }

    size_t ScrollbackRows() const {
        return scrollback_.size();
    }
    const Cell& ScrollbackAt(size_t row, uint16_t col) const;
    uint64_t Revision() const {
        return revision_;
    }
    std::string TakeResponse();

private:
    using Row = std::vector<Cell>;

    Cell& CellAt(uint16_t row, uint16_t col);
    Cell BlankCell() const;
    void Apply(const VtEvent& ev);
    void Print(char32_t cp);
    void Execute(uint8_t control);
    void Csi(const VtEvent& ev);
    void Esc(const VtEvent& ev);
    void Osc(const VtEvent& ev);
    void Sgr(const VtEvent& ev);
    void SetPrivateMode(int32_t mode, bool on);
    void SetAnsiMode(int32_t mode, bool on);
    void EraseInDisplay(int32_t how);
    void EraseInLine(int32_t how);
    void InsertLines(uint16_t count);
    void DeleteLines(uint16_t count);
    void InsertChars(uint16_t count);
    void DeleteChars(uint16_t count);
    void EraseChars(uint16_t count);
    void ScrollUp(uint16_t count);
    void ScrollDown(uint16_t count);
    void LineFeed();
    void ReverseIndex();
    void CarriageReturn();
    void Backspace();
    void Tab(uint16_t count);
    void BackTab(uint16_t count);
    void MoveTo(int32_t row, int32_t col);
    void MoveBy(int32_t rows, int32_t cols);
    void SaveCursor();
    void RestoreCursor();
    void SetScrollRegion(int32_t top, int32_t bottom);
    void SwitchScreen(bool alternate, bool clear);
    void Respond(std::string_view text);
    Row PushScrollback(Row&& row);
    void FullReset();
    uint16_t Cols() const {
        return size_.cols;
    }
    uint16_t Rows() const {
        return size_.rows;
    }

    TermSize size_{};
    size_t scrollbackLimit_ = kDefaultScrollback;
    std::vector<Row> grid_{};
    std::vector<Row> savedGrid_{};
    std::deque<Row> scrollback_{};
    VtParser parser_{};
    std::vector<VtEvent> events_{};

    CursorState cursor_{};
    CursorState savedCursor_{};
    Pen pen_{};
    Pen savedPen_{};
    TerminalModes modes_{};
    uint16_t scrollTop_ = 0;
    uint16_t scrollBottom_ = 0;
    bool pendingWrap_ = false;
    bool alternate_ = false;
    bool graphicsG0_ = false;
    bool graphicsG1_ = false;
    bool shiftedOut_ = false;
    bool savedShiftedOut_ = false;
    std::string title_{};
    std::string response_{};
    uint64_t revision_ = 0;
};

char32_t DecSpecialGraphic(char32_t cp);

}
