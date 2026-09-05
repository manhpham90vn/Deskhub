#include "ViewerRun.h"

#include "Output.h"
#include "Signals.h"

#include "Viewer.h"

namespace deskhubcli {

namespace {

const char* UnreachableFlag(const ViewRequest& request) {
    if (!request.fecScheme.empty()) return "--fec";
    if (!request.videoPath.empty()) return "--video-path";
    if (request.nackGiven) return "--nack";
    if (request.holdFrames) return "--hold";
    if (request.audioDelayMs) return "--audio-delay";
    if (request.audioAdaptiveGiven) return "--audio-adaptive";
    return nullptr;
}

}

ExitCode RunViewers(const ViewRequest& request) {
    if (const char* flag = UnreachableFlag(request)) {
        PrintError(std::string(flag) +
                   " cannot reach the viewer on Windows: this window layer drives the session "
                   "through the dh_screen FFI, which carries none of the measurement settings, "
                   "so the flag would be accepted and then ignored. Measure the receiving end "
                   "on a build whose viewer runs in-process.");
        return ExitCode::Unsupported;
    }
    WatchForInterrupt();
    RunViewer(request.hostLabel, request.sources, request.control, request.passcode);
    return ExitCode::Ok;
}

}
