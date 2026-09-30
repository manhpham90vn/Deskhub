#include "Commands.h"

#include <string>
#include <vector>

#include "Output.h"
#include "ViewerRun.h"

#include "deskhub/cli/Command.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace deskhubcli {

ExitCode RunConnect(const Command& command) {
    NetAddr server{};
    if (!ParseNetAddr(command.address, server)) {
        PrintError(deskhub::ui::InvalidAddressLine(command.address));
        PrintError(deskhub::ui::InvalidAddressHint());
        return ExitCode::Usage;
    }

    if (!deskhubp::QuicAvailable()) {
        PrintError(deskhub::ui::kShareNoQuicLibrary);
        return ExitCode::Unsupported;
    }

    SourceQueryReply reply;
    if (!QuerySources(server, reply, QueryRequestOf(command))) return ReportQueryFailure(reply);
    const std::vector<deskhub::SourceInfo>& offered = reply.sources;
    if (offered.empty()) {
        PrintError(deskhub::ui::SourceQueryEmpty(command.address));
        return ExitCode::NothingToShare;
    }

    std::vector<std::string> names;
    names.reserve(offered.size());
    for (const deskhub::SourceInfo& source : offered)
        names.push_back(deskhub::media::SourceName(source.name, source.sourceId));

    std::vector<std::string> wanted = command.connect.sources;
    if (wanted.empty()) wanted.push_back("0");

    const deskhub::cli::DisplayPick pick = deskhub::cli::PickByName(wanted, names);
    if (!pick.error.empty()) {
        PrintError(pick.error);
        return ExitCode::Usage;
    }

    const deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();

    ViewRequest request;
    request.server = server;
    request.hostLabel = command.address;
    request.displayName =
        command.deviceName ? *command.deviceName : deskhubp::SessionDeviceName();
    request.control = command.connect.control && reply.caps.acceptsInput;
    request.audio = command.connect.audio.value_or(settings.playAudio) && reply.caps.audio;
    for (size_t index : pick.indices) request.sources.push_back(offered[index]);

    if (command.connect.control && !reply.caps.acceptsInput && !command.quiet)
        PrintError(deskhub::ui::kViewOnlyNote);

    return RunViewers(request);
}

}
