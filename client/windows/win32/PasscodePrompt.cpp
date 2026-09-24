#include <wx/wx.h>

#include <wx/valtext.h>

#include <string>

#include "PasscodePrompt.h"
#include "WxUi.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/Strings.h"

namespace {

namespace ui = deskhub::ui;

wxString ToWx(const std::string& s) {
    return wxString::FromUTF8(s.c_str(), s.size());
}

wxString ToWx(const char* s) {
    return wxString::FromUTF8(s);
}

class PasscodePrompt final : public wxDialog {
public:
    PasscodePrompt(wxWindow* parent, const std::string& addr, const std::string& passcode)
        : wxDialog(parent, wxID_ANY, ToWx(ui::kConnectPromptTitle)) {
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        const wxSizerFlags pad = wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

        auto* device = new wxStaticText(this, wxID_ANY, ToWx(ui::AddressHost(addr)));
        device->SetFont(device->GetFont().Bold().Scaled(1.2f));
        sizer->Add(device, pad);

        const uint16_t knownPort = ui::AddressPort(addr);
        auto* portRow = new wxBoxSizer(wxHORIZONTAL);
        portRow->Add(new wxStaticText(this, wxID_ANY, ToWx(ui::kUdpPortLabel)),
            wxSizerFlags().CentreVertical());
        port_ = new wxTextCtrl(this, wxID_ANY,
            ToWx(std::to_string(knownPort != 0 ? knownPort : deskhub::kDeskhubPort)),
            wxDefaultPosition, FromDIP(wxSize(72, -1)), wxTE_PROCESS_ENTER,
            wxTextValidator(wxFILTER_DIGITS));
        port_->SetMaxLength(5);
        port_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { EndModal(wxID_OK); });
        portRow->Add(port_, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(10)));
        sizer->Add(portRow, pad);

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, ToWx(ui::kClientPasscodePrompt)),
            wxSizerFlags().CentreVertical());
        passcode_ = new wxTextCtrl(this, wxID_ANY, ToWx(passcode), wxDefaultPosition,
            FromDIP(wxSize(72, -1)), wxTE_PROCESS_ENTER, wxTextValidator(wxFILTER_DIGITS));
        passcode_->SetMaxLength(deskhub::kPasscodeDigits);
        passcode_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { EndModal(wxID_OK); });
        row->Add(passcode_, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(10)));
        sizer->Add(row, pad);

        auto* hint = new wxStaticText(this, wxID_ANY, ToWx(ui::kClientPasscodeHint));
        hint->SetForegroundColour(kMutedText);
        sizer->Add(hint, pad);

        if (auto* buttons = CreateStdDialogButtonSizer(wxOK | wxCANCEL)) {
            if (auto* ok = wxDynamicCast(FindWindow(wxID_OK), wxButton))
                ok->SetLabel(ToWx(ui::kConnectButton));
            sizer->Add(buttons, wxSizerFlags().Expand().Border(wxALL, FromDIP(16)));
        }

        SetSizerAndFit(sizer);
        CentreOnParent();
        passcode_->SetFocus();
        passcode_->SelectAll();
    }

    std::string typed() const {
        return std::string(passcode_->GetValue().utf8_str());
    }

    std::string typedPort() const {
        return std::string(port_->GetValue().utf8_str());
    }

private:
    wxTextCtrl* passcode_ = nullptr;
    wxTextCtrl* port_ = nullptr;
};

}

bool ShowPasscodePrompt(wxWindow* parent, std::string& addr, std::string& passcode) {
    ReleaseCapture();
    PasscodePrompt prompt(parent, addr, passcode);
    if (prompt.ShowModal() != wxID_OK) return false;
    passcode = prompt.typed();
    addr = ui::AddressWithPort(ui::AddressHost(addr), ui::PortOrDefault(prompt.typedPort()));
    return true;
}
