#pragma once
#include <wx/wx.h>

#include <string>

inline constexpr int kHintWrapDip = 620;

inline const wxColour kHeadingText(17, 24, 39);
inline const wxColour kMutedText(107, 114, 128);

inline wxString ToWx(const std::string& s) {
    return wxString::FromUTF8(s.c_str(), s.size());
}

inline wxString ToWx(const char* s) {
    return wxString::FromUTF8(s);
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

inline wxStaticText* MakeHint(wxWindow* parent, const wxString& text) {
    auto* hint = new wxStaticText(parent, wxID_ANY, text);
    hint->SetForegroundColour(kMutedText);
    hint->Wrap(parent->FromDIP(kHintWrapDip));
    return hint;
}
