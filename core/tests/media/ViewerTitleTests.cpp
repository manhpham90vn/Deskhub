#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/ViewerTitle.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

bool Contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

void TestBaseTitleNamesTheSource() {
    std::printf("[title] the window title says what is being viewed...\n");
    Check(ViewerBaseTitle("Display 1") == "Deskhub - viewing: Display 1",
        "a known source is named");
    Check(ViewerBaseTitle("") == "Deskhub - viewing",
        "before the host answers there is no dangling separator");
}

void TestStatusFallsBackToConnecting() {
    std::printf("[title] an empty status reads as connecting, never as a blank gap...\n");
    Check(ViewerControlSubtitle("") == kViewerConnectingStatus,
        "empty status is replaced by 'connecting...'");
    Check(ViewerControlSubtitle("60fps  12ms") == "60fps  12ms",
        "a real status line is passed through untouched, with no trailing separator");
}

void TestViewOnlySubtitleSaysSo() {
    std::printf("[title] a view-only session says input is not sent...\n");
    const std::string viewOnly = ViewerViewOnlySubtitle("60fps  12ms");
    Check(Contains(viewOnly, "60fps  12ms"), "the status line is kept");
    Check(Contains(viewOnly, kViewerViewOnlyHint), "the subtitle says the session is view-only");
    Check(Contains(ViewerViewOnlySubtitle(""), kViewerConnectingStatus),
        "an empty status still reads as connecting");
}

void TestComposedTitleKeepsEveryPart() {
    std::printf("[title] base, status and any hint all survive into the final title...\n");
    const std::string title = ComposeViewerTitle(ViewerBaseTitle("Display 1"), "60fps");
    Check(title == "Deskhub - viewing: Display 1 \xE2\x80\x94 60fps",
        "a controlling session shows base and status with nothing dangling");

    Check(Contains(ComposeViewerTitle("Deskhub", ""), kViewerConnectingStatus),
        "the composed title uses the same connecting placeholder");

    const std::string viewOnly = ComposeViewerTitle("Deskhub", "60fps", kViewerViewOnlyHint);
    Check(Contains(viewOnly, kViewerViewOnlyHint), "a view-only session says so");
    Check(viewOnly.find("60fps") < viewOnly.find(kViewerViewOnlyHint),
        "the hint follows the status, so the title does not reshuffle every second");
}

}

void RunViewerTitleTests() {
    TestBaseTitleNamesTheSource();
    TestStatusFallsBackToConnecting();
    TestViewOnlySubtitleSaysSo();
    TestComposedTitleKeepsEveryPart();
}
