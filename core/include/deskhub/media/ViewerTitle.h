#pragma once
#include <string>
#include <string_view>

namespace deskhub {

inline constexpr std::string_view kViewerConnectingStatus = "connecting...";
inline constexpr std::string_view kViewerViewOnlyHint = "View only - input is not sent";

inline std::string ViewerBaseTitle(std::string_view sourceName) {
    std::string base = "Deskhub - viewing";
    if (!sourceName.empty()) {
        base += ": ";
        base += sourceName;
    }
    return base;
}

inline std::string ViewerStatusWith(std::string_view statusLine, std::string_view hint) {
    std::string s(statusLine.empty() ? kViewerConnectingStatus : statusLine);
    if (hint.empty()) return s;
    s += " \xC2\xB7 ";
    s += hint;
    return s;
}

inline std::string ViewerControlSubtitle(std::string_view statusLine) {
    return ViewerStatusWith(statusLine, {});
}

inline std::string ViewerViewOnlySubtitle(std::string_view statusLine) {
    return ViewerStatusWith(statusLine, kViewerViewOnlyHint);
}

inline std::string ComposeViewerTitle(std::string_view base, std::string_view statusLine,
    std::string_view hint = {}) {
    std::string title(base);
    title += " \xE2\x80\x94 ";
    title += ViewerStatusWith(statusLine, hint);
    return title;
}

}
