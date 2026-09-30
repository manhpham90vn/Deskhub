#pragma once
#include <wx/spinbutt.h>
#include <wx/wx.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>

#include "deskhub/ui/Controls.h"
#include "deskhub/ui/HostRows.h"
#include "deskhub/ui/Theme.h"

inline constexpr int kHintWrapChars = 64;
inline constexpr int kFieldPaddingDip = 6;

inline wxColour ThemeColour(deskhub::ui::ThemeColor color) {
    const deskhub::ui::Rgb rgb = deskhub::ui::ThemeRgb(color, deskhub::ui::ThemeMode::Light);
    return wxColour(rgb.r, rgb.g, rgb.b);
}

inline const wxColour kHeadingText = ThemeColour(deskhub::ui::ThemeColor::Heading);
inline const wxColour kMutedText = ThemeColour(deskhub::ui::ThemeColor::Muted);

inline wxString ToWx(const std::string& s) {
    return wxString::FromUTF8(s.c_str(), s.size());
}

inline wxString ToWx(const char* s) {
    return wxString::FromUTF8(s);
}

inline int HintWrapWidth(const wxWindow* window) {
    return window->GetCharWidth() * kHintWrapChars;
}

inline wxStaticText* MakeHeading(wxWindow* parent, const char* text) {
    auto* heading = new wxStaticText(parent, wxID_ANY, ToWx(text));
    heading->SetFont(heading->GetFont().Bold().Scaled(1.35f));
    heading->SetForegroundColour(kHeadingText);
    return heading;
}

inline wxStaticText* MakeSection(wxWindow* parent, const char* text) {
    auto* section = new wxStaticText(parent, wxID_ANY, ToWx(text));
    section->SetFont(section->GetFont().Bold().Scaled(1.1f));
    section->SetForegroundColour(kHeadingText);
    return section;
}

inline void PaintRowButton(wxButton* button, const wxColour& background) {
    button->SetBackgroundColour(background);
    button->SetForegroundColour(*wxWHITE);
}

inline void PaintButton(wxButton* button, const wxColour& background) {
    PaintRowButton(button, background);
    button->SetFont(button->GetFont().Bold());
}

inline wxButton* MakeButton(wxWindow* parent, const wxString& label) {
    auto* button = new wxButton(parent, wxID_ANY, label);
    const wxSize least =
        parent->FromDIP(wxSize(deskhub::ui::kButtonMinWidth, deskhub::ui::kButtonHeight));
    button->SetMinSize(wxSize(std::max(least.x, button->GetBestSize().x), least.y));
    return button;
}

inline wxButton* MakePrimaryButton(wxWindow* parent, const wxString& label) {
    auto* button = new wxButton(parent, wxID_ANY, label);
    button->SetMinSize(parent->FromDIP(wxSize(-1, deskhub::ui::kPrimaryButtonHeight)));
    return button;
}

inline wxButton* MakeRowButton(wxWindow* parent, const wxString& label,
    const wxColour& background) {
    auto* button = new wxButton(parent, wxID_ANY, label);
    button->SetMinSize(
        parent->FromDIP(wxSize(deskhub::ui::kHostActionWidth, deskhub::ui::kHostActionHeight)));
    PaintRowButton(button, background);
    return button;
}

template <typename Control>
struct Field {
    wxPanel* box = nullptr;
    Control* control = nullptr;
};

inline wxPanel* MakeFieldBox(wxWindow* parent, int widthDip) {
    auto* box = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME);
    box->SetBackgroundColour(*wxWHITE);
    box->SetMinSize(box->FromDIP(wxSize(widthDip, deskhub::ui::kFieldHeight)));
    return box;
}

inline void CentreInFieldBox(wxPanel* box, wxWindow* control) {
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(control,
        wxSizerFlags(1).CentreVertical().Border(wxLEFT | wxRIGHT, box->FromDIP(kFieldPaddingDip)));
    box->SetSizer(sizer);
}

inline Field<wxTextCtrl> MakeTextField(wxWindow* parent, const wxString& value, int widthDip,
    long style = 0) {
    auto* box = MakeFieldBox(parent, widthDip);
    auto* control = new wxTextCtrl(box, wxID_ANY, value, wxDefaultPosition, wxDefaultSize,
        style | wxBORDER_NONE);
    CentreInFieldBox(box, control);
    return {box, control};
}

class NumberField final : public wxPanel {
public:
    NumberField(wxWindow* parent, uint32_t value, uint32_t maxValue)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME) {
        SetBackgroundColour(*wxWHITE);
        SetMinSize(FromDIP(wxSize(deskhub::ui::kNumberFieldWidth, deskhub::ui::kFieldHeight)));
        text_ = new wxTextCtrl(this, wxID_ANY, wxString::Format("%u", value), wxDefaultPosition,
            wxDefaultSize, wxBORDER_NONE);
        spin_ = new wxSpinButton(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxSP_VERTICAL | wxSP_ARROW_KEYS);
        spin_->SetRange(1, int(maxValue));
        spin_->SetValue(int(value));
        auto* sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(text_,
            wxSizerFlags(1).CentreVertical().Border(wxLEFT | wxRIGHT, FromDIP(kFieldPaddingDip)));
        sizer->Add(spin_, wxSizerFlags().Expand());
        SetSizer(sizer);
        spin_->Bind(wxEVT_SPIN, [this](wxSpinEvent& event) {
            text_->ChangeValue(wxString::Format("%d", event.GetPosition()));
            Notify();
        });
        text_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            long typed = 0;
            if (!text_->GetValue().ToLong(&typed)) return;
            spin_->SetValue(int(std::clamp<long>(typed, spin_->GetMin(), spin_->GetMax())));
            Notify();
        });
    }

    int GetValue() const {
        return spin_->GetValue();
    }

    void OnChange(std::function<void()> onChange) {
        onChange_ = std::move(onChange);
    }

private:
    void Notify() {
        if (onChange_) onChange_();
    }

    wxTextCtrl* text_ = nullptr;
    wxSpinButton* spin_ = nullptr;
    std::function<void()> onChange_;
};

inline wxStaticText* MakeHint(wxWindow* parent, const wxString& text) {
    auto* hint = new wxStaticText(parent, wxID_ANY, text);
    hint->SetForegroundColour(kMutedText);
    hint->Wrap(HintWrapWidth(hint));
    return hint;
}
