#include "ViewerRun.h"

#include "Signals.h"

#include "Viewer.h"

namespace deskhubcli {

ExitCode RunViewers(const ViewRequest& request) {
    WatchForInterrupt();
    RunViewer(request.hostLabel, request.sources, request.control);
    return ExitCode::Ok;
}

}
