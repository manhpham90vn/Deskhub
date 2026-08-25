#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "FileSendWindow.h"

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

#include <system_error>
#include <utility>

#include "deskhub/ui/Strings.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/FileTransferClient.h"
#include "deskhubp/client/FileUpload.h"
#include "WinControls.h"
#include "WinText.h"

namespace {

namespace ui = deskhub::ui;

constexpr wchar_t kWndClass[] = L"DeskhubFileSend";

constexpr int kIdChosen = 400;
constexpr int kIdHistory = 401;
constexpr int kIdProgress = 402;
constexpr int kIdSubtitle = 403;
constexpr int kIdError = 404;
constexpr int kIdStatus = 405;
constexpr int kIdStep = 406;
constexpr int kIdSentHeading = 407;
constexpr int kIdChoose = 408;
constexpr int kIdSend = 409;
constexpr int kIdStop = 410;

constexpr UINT kTimerPoll = 7;
constexpr UINT kPollMs = 200;

constexpr int kW = 560;
constexpr int kH = 540;
constexpr int kPad = 12;
constexpr int kListH = 150;

constexpr const char* kSentGlyph = "\xE2\x9C\x93";
constexpr const char* kNotSentGlyph = "\xE2\x9C\x95";

constexpr size_t kPathBufferChars = 64 * 1024;

std::string NameOf(const std::filesystem::path& path) {
    return ToUtf8(path.filename().wstring());
}

uint64_t SizeOf(const std::filesystem::path& path) {
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? 0 : uint64_t(size);
}

struct State {
    HWND hwnd = nullptr;
    HWND chosenList = nullptr;
    HWND historyList = nullptr;
    HWND progress = nullptr;
    HWND errorText = nullptr;
    HWND statusText = nullptr;
    HWND stepText = nullptr;
    HWND chooseButton = nullptr;
    HWND sendButton = nullptr;
    HWND stopButton = nullptr;
    const FileSendHooks* hooks = nullptr;
    std::vector<std::filesystem::path> chosen;
    std::vector<uint64_t> sizes;
    deskhub::ui::TransferView view;
    bool settled = true;
    bool done = false;
};

void SetText(HWND control, const std::string& text) {
    SetWindowTextW(control, FromUtf8(text).c_str());
}

void AddLine(HWND list, const std::string& text) {
    SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)FromUtf8(text).c_str());
}

void ShowChosen(State& st) {
    SendMessageW(st.chosenList, LB_RESETCONTENT, 0, 0);
    if (st.chosen.empty()) {
        AddLine(st.chosenList, ui::kTransferNoneChosen);
        return;
    }
    for (size_t i = 0; i < st.chosen.size(); ++i)
        AddLine(st.chosenList,
            NameOf(st.chosen[i]) + "   " + ui::FileSizeText(st.sizes[i]));
}

void Refresh(State& st) {
    const deskhub::ui::TransferView& view = st.view;
    const bool active = view.active;

    EnableWindow(st.chooseButton, !active);
    EnableWindow(st.sendButton, !active && !st.chosen.empty());
    ShowWindow(st.stopButton, active ? SW_SHOW : SW_HIDE);

    const int shown = view.Idle() ? SW_HIDE : SW_SHOW;
    ShowWindow(st.progress, shown);
    ShowWindow(st.statusText, shown);
    ShowWindow(st.stepText, shown);

    SendMessageW(st.progress, PBM_SETPOS, WPARAM(view.Percent()), 0);
    SetText(st.statusText, view.StatusLine());
    SetText(st.stepText, view.Step());
}

void Settle(State& st) {
    if (st.settled || st.view.active) return;
    if (!st.view.done && !st.view.failed) return;
    st.settled = true;

    for (size_t i = 0; i < st.chosen.size(); ++i) {
        const bool sent = st.view.done || i < size_t(st.view.fileIndex);
        const std::string detail =
            sent ? ui::FileSizeText(st.sizes[i]) : st.view.message;
        AddLine(st.historyList,
            std::string(sent ? kSentGlyph : kNotSentGlyph) + "  " + NameOf(st.chosen[i]) +
                "   " + detail);
    }

    st.chosen.clear();
    st.sizes.clear();
    ShowChosen(st);
}

bool AskAboutChangedKey(const State& st) {
    std::wstring body = FromUtf8(ui::kTrustChangedBody);
    body += L"\n\n";
    body += FromUtf8(ui::kTrustFingerprintLabel);
    body += L" ";
    body += FromUtf8(st.view.fingerprint);

    const std::wstring title = FromUtf8(ui::kTrustChangedTitle);
    const std::wstring accept = FromUtf8(ui::kTrustAccept);
    const std::wstring reject = FromUtf8(ui::kTrustReject);
    const TASKDIALOG_BUTTON choices[] = {{IDYES, accept.c_str()}, {IDNO, reject.c_str()}};
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof(dialog);
    dialog.hwndParent = st.hwnd;
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    dialog.pszWindowTitle = L"Deskhub";
    dialog.pszMainIcon = TD_WARNING_ICON;
    dialog.pszMainInstruction = title.c_str();
    dialog.pszContent = body.c_str();
    dialog.pButtons = choices;
    dialog.cButtons = 2;
    dialog.nDefaultButton = IDNO;
    int answer = IDNO;
    if (FAILED(TaskDialogIndirect(&dialog, &answer, nullptr, nullptr)))
        answer = MessageBoxW(st.hwnd, body.c_str(), title.c_str(),
            MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING);
    return answer == IDYES;
}

void SettleChangedKey(State& st) {
    if (AskAboutChangedKey(st) && st.hooks->acceptKey && st.hooks->acceptKey()) {
        st.view = deskhub::ui::TransferView{};
        st.view.active = true;
        Refresh(st);
        return;
    }
    Settle(st);
    Refresh(st);
}

void Poll(State& st) {
    const deskhub::ui::TransferView view = st.hooks->view ? st.hooks->view()
                                                          : deskhub::ui::TransferView{};
    if (view == st.view) return;
    st.view = view;
    if (st.view.keyChanged && !st.settled) {
        Refresh(st);
        SettleChangedKey(st);
        return;
    }
    Settle(st);
    Refresh(st);
}

std::vector<std::filesystem::path> AskForFiles(HWND owner) {
    std::vector<std::filesystem::path> picked;
    std::vector<wchar_t> buffer(kPathBufferChars, L'\0');

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = DWORD(buffer.size());
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return picked;

    const std::wstring first(buffer.data());
    const wchar_t* next = buffer.data() + first.size() + 1;
    if (*next == L'\0') {
        picked.push_back(std::filesystem::path(first));
        return picked;
    }
    while (*next != L'\0') {
        const std::wstring name(next);
        picked.push_back(std::filesystem::path(first) / name);
        next += name.size() + 1;
    }
    return picked;
}

void ChooseFiles(State& st) {
    std::vector<std::filesystem::path> picked = AskForFiles(st.hwnd);
    if (picked.empty()) return;

    const bool overflowed = picked.size() > deskhub::kMaxTransferFiles;
    if (overflowed) picked.resize(deskhub::kMaxTransferFiles);

    st.chosen = std::move(picked);
    st.sizes.clear();
    for (const std::filesystem::path& path : st.chosen) st.sizes.push_back(SizeOf(path));
    SetText(st.errorText, overflowed ? ui::kTransferTooManyFiles : "");
    ShowChosen(st);
    Refresh(st);
}

void SendChosen(State& st) {
    if (st.chosen.empty() || st.view.active) return;

    SetText(st.errorText, "");
    if (!st.hooks->begin || !st.hooks->begin(st.chosen)) {
        SetText(st.errorText, st.hooks->error ? st.hooks->error() : std::string());
        return;
    }

    st.settled = false;
    st.view = deskhub::ui::TransferView{};
    st.view.active = true;
    Refresh(st);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_COMMAND:
            if (!st) break;
            switch (LOWORD(wp)) {
                case kIdChoose: ChooseFiles(*st); return 0;
                case kIdSend: SendChosen(*st); return 0;
                case kIdStop:
                    if (st->hooks->cancel) st->hooks->cancel();
                    return 0;
            }
            break;
        case WM_TIMER:
            if (st && wp == kTimerPoll) Poll(*st);
            return 0;
        case WM_CLOSE:
            if (st) st->done = true;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}

void RunFileSendWindow(HWND owner, bool modal, const std::string& subtitle,
    const FileSendHooks& hooks) {
    INITCOMMONCONTROLSEX controls{sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    RECT anchor{};
    if (owner)
        GetWindowRect(owner, &anchor);
    else
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    const int x = anchor.left + ((anchor.right - anchor.left) - kW) / 2;
    const int y = anchor.top + ((anchor.bottom - anchor.top) - kH) / 2;

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT wr{0, 0, kW, kH};
    AdjustWindowRect(&wr, style, FALSE);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kWndClass,
        FromUtf8(ui::kTransferSendHeading).c_str(), style, x, y, wr.right - wr.left,
        wr.bottom - wr.top, modal ? owner : nullptr, nullptr, wc.hInstance, nullptr);
    if (!dlg) return;

    State st;
    st.hwnd = dlg;
    st.hooks = &hooks;
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    const ChildControlFactory mk(dlg, wc.hInstance);
    const int inner = kW - 2 * kPad;
    int row = kPad;

    mk(L"STATIC", FromUtf8(subtitle).c_str(), 0, kPad, row, inner, 18, kIdSubtitle);
    row += 26;
    st.chosenList = mk(L"LISTBOX", nullptr, LBS_NOSEL | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER,
        kPad, row, inner, kListH, kIdChosen);
    row += kListH + 8;
    st.errorText = mk(L"STATIC", nullptr, 0, kPad, row, inner, 34, kIdError);
    row += 40;
    st.progress = mk(PROGRESS_CLASSW, nullptr, 0, kPad, row, inner, 14, kIdProgress);
    row += 20;
    st.statusText = mk(L"STATIC", nullptr, SS_PATHELLIPSIS, kPad, row, inner - 90, 18, kIdStatus);
    st.stepText = mk(L"STATIC", nullptr, SS_RIGHT, kPad + inner - 84, row, 84, 18, kIdStep);
    row += 26;
    mk(L"STATIC", FromUtf8(ui::kTransferSentHeading).c_str(), 0, kPad, row, inner, 18,
        kIdSentHeading);
    row += 22;
    st.historyList = mk(L"LISTBOX", nullptr,
        LBS_NOSEL | LBS_HASSTRINGS | WS_VSCROLL | WS_BORDER, kPad, row, inner, kListH,
        kIdHistory);

    const int buttonY = kH - 46;
    st.chooseButton = mk(L"BUTTON", FromUtf8(ui::kTransferChooseButton).c_str(), 0, kPad,
        buttonY, 130, 26, kIdChoose);
    st.sendButton = mk(L"BUTTON", L"Send", BS_DEFPUSHBUTTON, kW - kPad - 90, buttonY, 90, 26,
        kIdSend);
    st.stopButton = mk(L"BUTTON", FromUtf8(ui::kTransferCancelButton).c_str(), 0,
        kW - kPad - 210, buttonY, 114, 26, kIdStop);

    SendMessageW(st.progress, PBM_SETRANGE32, 0, 100);
    ShowChosen(st);
    Refresh(st);

    if (modal && owner) EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    SetForegroundWindow(dlg);
    SetTimer(dlg, kTimerPoll, kPollMs, nullptr);

    if (PumpMessagesUntil(dlg, [&st] { return !st.done; })) PostQuitMessage(0);

    KillTimer(dlg, kTimerPoll);
    if (modal && owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    DestroyWindow(dlg);
}

void RunStandaloneFileSend(const FileSendLaunch& launch) {
    NetAddr server{};
    if (!ParseNetAddr(launch.address, server)) return;

    deskhubp::FileTransferClient client;
    std::string error;

    FileSendHooks hooks;
    hooks.begin = [&](const std::vector<std::filesystem::path>& files) {
        client.Stop();
        error.clear();

        const deskhubp::FileBatch batch = deskhubp::InspectFiles(files);
        if (!batch.Ok()) {
            error = batch.error;
            return false;
        }

        deskhubp::FileTransferClientConfig config;
        config.host = server;
        config.hostLabel = launch.address;
        config.passcode = launch.passcode;
        config.clientName = launch.clientName;
        config.files = files;
        if (!client.Start(config, deskhubp::FileTransferClientCallbacks{})) {
            error = ui::CouldNotConnectTo(launch.address);
            return false;
        }
        return true;
    };
    hooks.cancel = [&client] { client.Cancel(); };
    hooks.view = [&client] { return client.View(); };
    hooks.error = [&error] { return error; };
    hooks.acceptKey = [&client] { return client.AcceptKeyAndRetry(); };

    RunFileSendWindow(nullptr, false, launch.address, hooks);
    client.Stop();
}
