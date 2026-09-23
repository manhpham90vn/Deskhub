#include "deskhub/terminal/Screen.h"

#include <algorithm>

namespace deskhub::term {

namespace {

constexpr uint8_t kBel = 0x07;
constexpr uint8_t kBs = 0x08;
constexpr uint8_t kHt = 0x09;
constexpr uint8_t kLf = 0x0A;
constexpr uint8_t kVt = 0x0B;
constexpr uint8_t kFf = 0x0C;
constexpr uint8_t kCr = 0x0D;
constexpr uint8_t kSo = 0x0E;
constexpr uint8_t kSi = 0x0F;

constexpr std::string_view kDeviceAttributes = "\x1B[?1;2c";

uint16_t ClampCount(int32_t value, uint16_t limit) {
    if (value < 1) return 1;
    if (value > int32_t(limit)) return limit;
    return uint16_t(value);
}

Color ExtendedColor(const VtEvent& ev, size_t& at) {
    const int32_t kind = ev.Param(at + 1, 0);
    if (kind == 5) {
        const int32_t index = ev.Param(at + 2, 0);
        at += 2;
        return PaletteColor(uint8_t(std::clamp(index, 0, 255)));
    }
    if (kind == 2) {
        size_t first = at + 2;
        if (ev.paramCount > first && ev.params[first] == kVtParamOmitted) ++first;
        const int32_t r = ev.Param(first, 0);
        const int32_t g = ev.Param(first + 1, 0);
        const int32_t b = ev.Param(first + 2, 0);
        at = first + 2;
        return RgbColor(uint8_t(std::clamp(r, 0, 255)), uint8_t(std::clamp(g, 0, 255)),
            uint8_t(std::clamp(b, 0, 255)));
    }
    at += 1;
    return Color{};
}

}

char32_t DecSpecialGraphic(char32_t cp) {
    switch (cp) {
        case U'`': return U'\u25C6';
        case U'a': return U'\u2592';
        case U'b': return U'\u2409';
        case U'c': return U'\u240C';
        case U'd': return U'\u240D';
        case U'e': return U'\u240A';
        case U'f': return U'\u00B0';
        case U'g': return U'\u00B1';
        case U'h': return U'\u2424';
        case U'i': return U'\u240B';
        case U'j': return U'\u2518';
        case U'k': return U'\u2510';
        case U'l': return U'\u250C';
        case U'm': return U'\u2514';
        case U'n': return U'\u253C';
        case U'o': return U'\u23BA';
        case U'p': return U'\u23BB';
        case U'q': return U'\u2500';
        case U'r': return U'\u23BC';
        case U's': return U'\u23BD';
        case U't': return U'\u251C';
        case U'u': return U'\u2524';
        case U'v': return U'\u2534';
        case U'w': return U'\u252C';
        case U'x': return U'\u2502';
        case U'y': return U'\u2264';
        case U'z': return U'\u2265';
        case U'{': return U'\u03C0';
        case U'|': return U'\u2260';
        case U'}': return U'\u00A3';
        case U'~': return U'\u00B7';
        default: return cp;
    }
}

Screen::Screen(TermSize size, size_t scrollbackLimit)
    : size_(ClampTermSize(size)),
      scrollbackLimit_(std::min(scrollbackLimit, kMaxScrollback)) {
    grid_.assign(size_.rows, Row(size_.cols, Cell{}));
    scrollBottom_ = uint16_t(size_.rows - 1);
}

Cell Screen::BlankCell() const {
    Cell c;
    c.pen.bg = pen_.bg;
    return c;
}

const Cell& Screen::At(uint16_t row, uint16_t col) const {
    static const Cell kBlank{};
    if (row >= Rows() || col >= Cols()) return kBlank;
    return grid_[row][col];
}

Cell& Screen::CellAt(uint16_t row, uint16_t col) {
    return grid_[row][col];
}

std::string Screen::RowText(uint16_t row) const {
    std::string out;
    if (row >= Rows()) return out;
    for (const Cell& c : grid_[row]) out += EncodeUtf8(c.ch);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string Screen::Text() const {
    std::string out;
    for (uint16_t r = 0; r < Rows(); ++r) {
        out += RowText(r);
        if (r + 1 < Rows()) out.push_back('\n');
    }
    return out;
}

const Cell& Screen::ScrollbackAt(size_t row, uint16_t col) const {
    static const Cell kBlank{};
    if (row >= scrollback_.size() || col >= scrollback_[row].size()) return kBlank;
    return scrollback_[row][col];
}

std::string Screen::TakeResponse() {
    std::string out;
    out.swap(response_);
    return out;
}

void Screen::Respond(std::string_view text) {
    if (response_.size() + text.size() > kMaxResponseBytes) return;
    response_ += text;
}

Screen::Row Screen::PushScrollback(Row&& row) {
    if (scrollbackLimit_ == 0) return std::move(row);
    scrollback_.push_back(std::move(row));
    Row recycled;
    while (scrollback_.size() > scrollbackLimit_) {
        recycled = std::move(scrollback_.front());
        scrollback_.pop_front();
    }
    return recycled;
}

void Screen::Write(std::string_view text) {
    Write(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

void Screen::Write(std::span<const uint8_t> bytes) {
    if (bytes.empty()) return;
    events_.clear();
    parser_.Parse(bytes, events_);
    for (const VtEvent& ev : events_) Apply(ev);
    ++revision_;
}

void Screen::Apply(const VtEvent& ev) {
    switch (ev.action) {
        case VtAction::Print: Print(ev.codepoint); return;
        case VtAction::Execute: Execute(ev.final); return;
        case VtAction::Csi: Csi(ev); return;
        case VtAction::Esc: Esc(ev); return;
        case VtAction::Osc: Osc(ev); return;
    }
}

void Screen::Print(char32_t cp) {
    if (pendingWrap_ && modes_.autoWrap) {
        cursor_.col = 0;
        LineFeed();
        pendingWrap_ = false;
    }
    pendingWrap_ = false;
    if (cursor_.col >= Cols()) cursor_.col = uint16_t(Cols() - 1);

    const bool graphics = shiftedOut_ ? graphicsG1_ : graphicsG0_;
    const char32_t glyph = graphics ? DecSpecialGraphic(cp) : cp;

    if (modes_.insert) {
        Row& row = grid_[cursor_.row];
        row.insert(row.begin() + cursor_.col, BlankCell());
        row.resize(Cols());
    }

    Cell& cell = CellAt(cursor_.row, cursor_.col);
    cell.ch = glyph;
    cell.pen = pen_;

    if (cursor_.col + 1 >= Cols()) {
        pendingWrap_ = true;
    } else {
        ++cursor_.col;
    }
}

void Screen::Execute(uint8_t control) {
    switch (control) {
        case kBel: return;
        case kBs: Backspace(); return;
        case kHt: Tab(1); return;
        case kLf:
        case kVt:
        case kFf: LineFeed(); return;
        case kCr: CarriageReturn(); return;
        case kSo: shiftedOut_ = true; return;
        case kSi: shiftedOut_ = false; return;
        default: return;
    }
}

void Screen::CarriageReturn() {
    cursor_.col = 0;
    pendingWrap_ = false;
}

void Screen::Backspace() {
    if (pendingWrap_) {
        pendingWrap_ = false;
        return;
    }
    if (cursor_.col > 0) --cursor_.col;
}

void Screen::Tab(uint16_t count) {
    for (uint16_t i = 0; i < count; ++i) {
        const uint16_t next = uint16_t((cursor_.col / kTabWidth + 1) * kTabWidth);
        cursor_.col = next >= Cols() ? uint16_t(Cols() - 1) : next;
    }
    pendingWrap_ = false;
}

void Screen::BackTab(uint16_t count) {
    for (uint16_t i = 0; i < count; ++i) {
        if (cursor_.col == 0) break;
        cursor_.col = uint16_t(((cursor_.col - 1) / kTabWidth) * kTabWidth);
    }
    pendingWrap_ = false;
}

void Screen::LineFeed() {
    pendingWrap_ = false;
    if (cursor_.row == scrollBottom_) {
        ScrollUp(1);
        return;
    }
    if (cursor_.row + 1 < Rows()) ++cursor_.row;
}

void Screen::ReverseIndex() {
    pendingWrap_ = false;
    if (cursor_.row == scrollTop_) {
        ScrollDown(1);
        return;
    }
    if (cursor_.row > 0) --cursor_.row;
}

void Screen::ScrollUp(uint16_t count) {
    const uint16_t span = uint16_t(scrollBottom_ - scrollTop_ + 1);
    const uint16_t n = std::min(count, span);
    if (n == 0) return;
    for (uint16_t i = 0; i < n; ++i) {
        Row& leaving = grid_[scrollTop_ + i];
        if (scrollTop_ == 0 && !alternate_) leaving = PushScrollback(std::move(leaving));
        leaving.assign(Cols(), BlankCell());
    }
    std::rotate(grid_.begin() + scrollTop_, grid_.begin() + scrollTop_ + n,
        grid_.begin() + scrollBottom_ + 1);
}

void Screen::ScrollDown(uint16_t count) {
    const uint16_t span = uint16_t(scrollBottom_ - scrollTop_ + 1);
    const uint16_t n = std::min(count, span);
    if (n == 0) return;
    std::rotate(grid_.begin() + scrollTop_, grid_.begin() + scrollBottom_ + 1 - n,
        grid_.begin() + scrollBottom_ + 1);
    for (uint16_t i = 0; i < n; ++i) grid_[scrollTop_ + i].assign(Cols(), BlankCell());
}

void Screen::MoveTo(int32_t row, int32_t col) {
    const int32_t top = modes_.origin ? scrollTop_ : 0;
    const int32_t bottom = modes_.origin ? scrollBottom_ : Rows() - 1;
    const int32_t target = std::clamp(top + row, top, bottom);
    cursor_.row = uint16_t(target);
    cursor_.col = uint16_t(std::clamp(col, 0, int32_t(Cols()) - 1));
    pendingWrap_ = false;
}

void Screen::MoveBy(int32_t rows, int32_t cols) {
    const int32_t top = modes_.origin ? scrollTop_ : 0;
    const int32_t bottom = modes_.origin ? scrollBottom_ : Rows() - 1;
    cursor_.row = uint16_t(std::clamp(int32_t(cursor_.row) + rows, top, bottom));
    cursor_.col = uint16_t(std::clamp(int32_t(cursor_.col) + cols, 0, int32_t(Cols()) - 1));
    pendingWrap_ = false;
}

void Screen::SaveCursor() {
    savedCursor_ = cursor_;
    savedPen_ = pen_;
    savedShiftedOut_ = shiftedOut_;
}

void Screen::RestoreCursor() {
    cursor_.row = std::min(savedCursor_.row, uint16_t(Rows() - 1));
    cursor_.col = std::min(savedCursor_.col, uint16_t(Cols() - 1));
    pen_ = savedPen_;
    shiftedOut_ = savedShiftedOut_;
    pendingWrap_ = false;
}

void Screen::SetScrollRegion(int32_t top, int32_t bottom) {
    if (top < 1) top = 1;
    if (bottom < 1 || bottom > int32_t(Rows())) bottom = Rows();
    if (top >= bottom) {
        scrollTop_ = 0;
        scrollBottom_ = uint16_t(Rows() - 1);
    } else {
        scrollTop_ = uint16_t(top - 1);
        scrollBottom_ = uint16_t(bottom - 1);
    }
    MoveTo(0, 0);
}

void Screen::EraseInDisplay(int32_t how) {
    const Cell blank = BlankCell();
    if (how == 1) {
        for (uint16_t r = 0; r < cursor_.row; ++r) grid_[r].assign(Cols(), blank);
        for (uint16_t c = 0; c <= cursor_.col && c < Cols(); ++c) CellAt(cursor_.row, c) = blank;
        return;
    }
    if (how == 2 || how == 3) {
        if (how == 3) scrollback_.clear();
        for (uint16_t r = 0; r < Rows(); ++r) grid_[r].assign(Cols(), blank);
        return;
    }
    for (uint16_t c = cursor_.col; c < Cols(); ++c) CellAt(cursor_.row, c) = blank;
    for (uint16_t r = uint16_t(cursor_.row + 1); r < Rows(); ++r) grid_[r].assign(Cols(), blank);
}

void Screen::EraseInLine(int32_t how) {
    const Cell blank = BlankCell();
    if (how == 1) {
        for (uint16_t c = 0; c <= cursor_.col && c < Cols(); ++c) CellAt(cursor_.row, c) = blank;
        return;
    }
    if (how == 2) {
        grid_[cursor_.row].assign(Cols(), blank);
        return;
    }
    for (uint16_t c = cursor_.col; c < Cols(); ++c) CellAt(cursor_.row, c) = blank;
}

void Screen::InsertLines(uint16_t count) {
    if (cursor_.row < scrollTop_ || cursor_.row > scrollBottom_) return;
    const uint16_t savedTop = scrollTop_;
    scrollTop_ = cursor_.row;
    ScrollDown(count);
    scrollTop_ = savedTop;
    cursor_.col = 0;
    pendingWrap_ = false;
}

void Screen::DeleteLines(uint16_t count) {
    if (cursor_.row < scrollTop_ || cursor_.row > scrollBottom_) return;
    const uint16_t savedTop = scrollTop_;
    scrollTop_ = cursor_.row;
    ScrollUp(count);
    scrollTop_ = savedTop;
    cursor_.col = 0;
    pendingWrap_ = false;
}

void Screen::InsertChars(uint16_t count) {
    Row& row = grid_[cursor_.row];
    const uint16_t n = std::min<uint16_t>(count, uint16_t(Cols() - cursor_.col));
    row.insert(row.begin() + cursor_.col, n, BlankCell());
    row.resize(Cols());
    pendingWrap_ = false;
}

void Screen::DeleteChars(uint16_t count) {
    Row& row = grid_[cursor_.row];
    const uint16_t n = std::min<uint16_t>(count, uint16_t(Cols() - cursor_.col));
    row.erase(row.begin() + cursor_.col, row.begin() + cursor_.col + n);
    row.resize(Cols(), BlankCell());
    pendingWrap_ = false;
}

void Screen::EraseChars(uint16_t count) {
    const Cell blank = BlankCell();
    const uint16_t last = std::min<uint16_t>(uint16_t(cursor_.col + count), Cols());
    for (uint16_t c = cursor_.col; c < last; ++c) CellAt(cursor_.row, c) = blank;
    pendingWrap_ = false;
}

void Screen::Csi(const VtEvent& ev) {
    if (ev.prefix == '?') {
        if (ev.final == 'h' || ev.final == 'l') {
            const bool on = ev.final == 'h';
            for (size_t i = 0; i < ev.paramCount; ++i) SetPrivateMode(ev.Param(i, 0), on);
        }
        return;
    }
    if (ev.prefix != 0) return;
    if (ev.intermediate != 0) return;

    switch (ev.final) {
        case 'A': MoveBy(-ClampCount(ev.Param(0, 1), Rows()), 0); return;
        case 'B': MoveBy(ClampCount(ev.Param(0, 1), Rows()), 0); return;
        case 'C': MoveBy(0, ClampCount(ev.Param(0, 1), Cols())); return;
        case 'D': MoveBy(0, -ClampCount(ev.Param(0, 1), Cols())); return;
        case 'E':
            MoveBy(ClampCount(ev.Param(0, 1), Rows()), 0);
            cursor_.col = 0;
            return;
        case 'F':
            MoveBy(-ClampCount(ev.Param(0, 1), Rows()), 0);
            cursor_.col = 0;
            return;
        case 'G':
        case '`': MoveTo(cursor_.row - (modes_.origin ? scrollTop_ : 0), ev.Param(0, 1) - 1); return;
        case 'H':
        case 'f': MoveTo(ev.Param(0, 1) - 1, ev.Param(1, 1) - 1); return;
        case 'I': Tab(ClampCount(ev.Param(0, 1), Cols())); return;
        case 'J': EraseInDisplay(ev.Param(0, 0)); return;
        case 'K': EraseInLine(ev.Param(0, 0)); return;
        case 'L': InsertLines(ClampCount(ev.Param(0, 1), Rows())); return;
        case 'M': DeleteLines(ClampCount(ev.Param(0, 1), Rows())); return;
        case 'P': DeleteChars(ClampCount(ev.Param(0, 1), Cols())); return;
        case 'S': ScrollUp(ClampCount(ev.Param(0, 1), Rows())); return;
        case 'T': ScrollDown(ClampCount(ev.Param(0, 1), Rows())); return;
        case 'X': EraseChars(ClampCount(ev.Param(0, 1), Cols())); return;
        case 'Z': BackTab(ClampCount(ev.Param(0, 1), Cols())); return;
        case '@': InsertChars(ClampCount(ev.Param(0, 1), Cols())); return;
        case 'd': MoveTo(ev.Param(0, 1) - 1 - (modes_.origin ? scrollTop_ : 0), cursor_.col); return;
        case 'c':
            if (ev.Param(0, 0) == 0) Respond(kDeviceAttributes);
            return;
        case 'm': Sgr(ev); return;
        case 'n':
            if (ev.Param(0, 0) == 5) Respond("\x1B[0n");
            if (ev.Param(0, 0) == 6) {
                const int row = cursor_.row - (modes_.origin ? scrollTop_ : 0) + 1;
                Respond("\x1B[" + std::to_string(row) + ";" + std::to_string(cursor_.col + 1) + "R");
            }
            return;
        case 'r': SetScrollRegion(ev.Param(0, 1), ev.Param(1, int32_t(Rows()))); return;
        case 's': SaveCursor(); return;
        case 'u': RestoreCursor(); return;
        case 'h':
        case 'l': {
            const bool on = ev.final == 'h';
            for (size_t i = 0; i < ev.paramCount; ++i) SetAnsiMode(ev.Param(i, 0), on);
            return;
        }
        default: return;
    }
}

void Screen::Sgr(const VtEvent& ev) {
    if (ev.paramCount == 0) {
        pen_ = Pen{};
        return;
    }
    for (size_t i = 0; i < ev.paramCount; ++i) {
        const int32_t code = ev.Param(i, 0);
        switch (code) {
            case 0: pen_ = Pen{}; break;
            case 1: pen_.attrs |= kAttrBold; break;
            case 2: pen_.attrs |= kAttrDim; break;
            case 3: pen_.attrs |= kAttrItalic; break;
            case 4: pen_.attrs |= kAttrUnderline; break;
            case 5:
            case 6: pen_.attrs |= kAttrBlink; break;
            case 7: pen_.attrs |= kAttrReverse; break;
            case 8: pen_.attrs |= kAttrHidden; break;
            case 9: pen_.attrs |= kAttrStrike; break;
            case 21:
            case 22: pen_.attrs &= uint8_t(~(kAttrBold | kAttrDim)); break;
            case 23: pen_.attrs &= uint8_t(~kAttrItalic); break;
            case 24: pen_.attrs &= uint8_t(~kAttrUnderline); break;
            case 25: pen_.attrs &= uint8_t(~kAttrBlink); break;
            case 27: pen_.attrs &= uint8_t(~kAttrReverse); break;
            case 28: pen_.attrs &= uint8_t(~kAttrHidden); break;
            case 29: pen_.attrs &= uint8_t(~kAttrStrike); break;
            case 38: pen_.fg = ExtendedColor(ev, i); break;
            case 39: pen_.fg = Color{}; break;
            case 48: pen_.bg = ExtendedColor(ev, i); break;
            case 49: pen_.bg = Color{}; break;
            default:
                if (code >= 30 && code <= 37)
                    pen_.fg = PaletteColor(uint8_t(code - 30));
                else if (code >= 40 && code <= 47)
                    pen_.bg = PaletteColor(uint8_t(code - 40));
                else if (code >= 90 && code <= 97)
                    pen_.fg = PaletteColor(uint8_t(code - 90 + 8));
                else if (code >= 100 && code <= 107)
                    pen_.bg = PaletteColor(uint8_t(code - 100 + 8));
                break;
        }
    }
}

void Screen::SetPrivateMode(int32_t mode, bool on) {
    switch (mode) {
        case 1: modes_.applicationCursor = on; return;
        case 5: modes_.reverseVideo = on; return;
        case 6:
            modes_.origin = on;
            MoveTo(0, 0);
            return;
        case 7: modes_.autoWrap = on; return;
        case 25: cursor_.visible = on; return;
        case 47: SwitchScreen(on, false); return;
        case 1047:
            if (!on && alternate_) EraseInDisplay(2);
            SwitchScreen(on, false);
            return;
        case 1048:
            if (on)
                SaveCursor();
            else
                RestoreCursor();
            return;
        case 1049:
            if (on) SaveCursor();
            SwitchScreen(on, on);
            if (!on) RestoreCursor();
            return;
        case 1000:
        case 1002:
        case 1003:
        case 1006:
        case 1015: modes_.mouseReporting = on; return;
        case 2004: modes_.bracketedPaste = on; return;
        default: return;
    }
}

void Screen::SetAnsiMode(int32_t mode, bool on) {
    if (mode == 4) modes_.insert = on;
}

void Screen::SwitchScreen(bool alternate, bool clear) {
    if (alternate == alternate_) {
        if (alternate && clear) EraseInDisplay(2);
        return;
    }
    alternate_ = alternate;
    std::swap(grid_, savedGrid_);
    if (grid_.size() != Rows()) grid_.assign(Rows(), Row(Cols(), Cell{}));
    for (Row& row : grid_) row.resize(Cols(), Cell{});
    if (clear) EraseInDisplay(2);
}

void Screen::Esc(const VtEvent& ev) {
    if (ev.intermediate == '(') {
        graphicsG0_ = ev.final == '0';
        return;
    }
    if (ev.intermediate == ')') {
        graphicsG1_ = ev.final == '0';
        return;
    }
    if (ev.intermediate == '#') {
        if (ev.final == '8') {
            for (uint16_t r = 0; r < Rows(); ++r)
                for (uint16_t c = 0; c < Cols(); ++c) CellAt(r, c) = Cell{U'E', pen_};
        }
        return;
    }
    if (ev.intermediate != 0) return;

    switch (ev.final) {
        case '7': SaveCursor(); return;
        case '8': RestoreCursor(); return;
        case '=': modes_.applicationKeypad = true; return;
        case '>': modes_.applicationKeypad = false; return;
        case 'D': LineFeed(); return;
        case 'E':
            LineFeed();
            cursor_.col = 0;
            return;
        case 'M': ReverseIndex(); return;
        case 'c': FullReset(); return;
        default: return;
    }
}

void Screen::Osc(const VtEvent& ev) {
    const int32_t command = ev.Param(0, -1);
    if (command != 0 && command != 2) return;
    title_.assign(ev.text.substr(0, std::min(ev.text.size(), kMaxTitleBytes)));
}

void Screen::FullReset() {
    cursor_ = CursorState{};
    savedCursor_ = CursorState{};
    pen_ = Pen{};
    savedPen_ = Pen{};
    modes_ = TerminalModes{};
    scrollTop_ = 0;
    scrollBottom_ = uint16_t(Rows() - 1);
    pendingWrap_ = false;
    alternate_ = false;
    graphicsG0_ = false;
    graphicsG1_ = false;
    shiftedOut_ = false;
    savedShiftedOut_ = false;
    title_.clear();
    savedGrid_.clear();
    for (Row& row : grid_) row.assign(Cols(), Cell{});
}

void Screen::Reset() {
    parser_.Reset();
    scrollback_.clear();
    response_.clear();
    FullReset();
    ++revision_;
}

void Screen::Resize(TermSize size) {
    const TermSize target = ClampTermSize(size);
    if (target == size_) return;

    if (target.rows < size_.rows && !alternate_) {
        const uint16_t drop =
            std::min<uint16_t>(uint16_t(size_.rows - target.rows), cursor_.row);
        for (uint16_t i = 0; i < drop; ++i) {
            PushScrollback(std::move(grid_.front()));
            grid_.erase(grid_.begin());
        }
        cursor_.row = uint16_t(cursor_.row - drop);
    }

    grid_.resize(target.rows, Row(target.cols, Cell{}));
    for (Row& row : grid_) row.resize(target.cols, Cell{});
    if (!savedGrid_.empty()) {
        savedGrid_.resize(target.rows, Row(target.cols, Cell{}));
        for (Row& row : savedGrid_) row.resize(target.cols, Cell{});
    }

    size_ = target;
    scrollTop_ = 0;
    scrollBottom_ = uint16_t(target.rows - 1);
    cursor_.row = std::min(cursor_.row, uint16_t(target.rows - 1));
    cursor_.col = std::min(cursor_.col, uint16_t(target.cols - 1));
    savedCursor_.row = std::min(savedCursor_.row, uint16_t(target.rows - 1));
    savedCursor_.col = std::min(savedCursor_.col, uint16_t(target.cols - 1));
    pendingWrap_ = false;
    ++revision_;
}

}
