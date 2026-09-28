#include "Commands.h"

#include <string>
#include <vector>

#include "Output.h"

#include "deskhub/cli/Json.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/system/HostIdentity.h"

namespace deskhubcli {

namespace {

bool ResolveTarget(const Command& command, NetAddr& out) {
    if (ParseNetAddr(command.address, out)) return true;
    PrintError(deskhub::ui::InvalidAddressLine(command.address));
    PrintError(deskhub::ui::InvalidAddressHint());
    return false;
}

}

ExitCode RunDisplays(const Command& command) {
    if (command.forget) {
        deskhubp::ForgetDisplaySelection();
        if (!command.quiet) PrintLine("Forgot the saved screen choice - the next listing asks again.");
        return ExitCode::Ok;
    }

    const std::vector<deskhub::media::ShareSource> displays = deskhubp::ListDisplays();
    if (displays.empty()) {
        const std::string reason = deskhubp::ListDisplaysError();
        PrintError(reason.empty() ? std::string(deskhub::ui::kNoDisplayFound)
                                  : std::string(deskhub::ui::kNoDisplayFound) + " " + reason);
        deskhubp::ReleaseDisplays();
        return ExitCode::NothingToShare;
    }

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ArrayBegin();
        for (size_t i = 0; i < displays.size(); ++i) {
            const deskhub::media::ShareSource& display = displays[i];
            json.ObjectBegin();
            json.Field("id", i);
            json.Field("name", deskhub::media::SourceName(display.name, uint8_t(i)));
            json.Field("width", display.width);
            json.Field("height", display.height);
            json.Field("x", display.x);
            json.Field("y", display.y);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        PrintLine(json.Text());
    } else {
        Table table;
        table.Row({"ID", "NAME", "SIZE", "POSITION"});
        for (size_t i = 0; i < displays.size(); ++i) {
            const deskhub::media::ShareSource& display = displays[i];
            table.Row({std::to_string(i), deskhub::media::SourceName(display.name, uint8_t(i)),
                deskhub::media::SourceSizeLabel(display.width, display.height),
                std::to_string(display.x) + "," + std::to_string(display.y)});
        }
        table.Print();
    }

    deskhubp::ReleaseDisplays();
    return ExitCode::Ok;
}

ExitCode RunSources(const Command& command) {
    NetAddr server{};
    if (!ResolveTarget(command, server)) return ExitCode::Usage;

    if (!deskhubp::QuicAvailable()) {
        PrintError(deskhub::ui::kShareNoQuicLibrary);
        return ExitCode::Unsupported;
    }

    std::vector<deskhub::SourceInfo> sources;
    deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
    deskhub::HostCaps caps{};
    if (!QuerySources(server, sources, {}, &code, &caps)) {
        PrintError(deskhub::ui::AuthRefusalText(code));
        return ExitCode::Refused;
    }
    if (sources.empty() && !caps.terminal && !caps.files) {
        PrintError(deskhub::ui::SourceQueryEmpty(command.address));
        return ExitCode::Unreachable;
    }

    if (command.json) {
        deskhub::cli::JsonWriter json;
        json.ObjectBegin();
        json.Field("address", command.address);
        json.FieldBegin("sources");
        json.ArrayBegin();
        for (const deskhub::SourceInfo& source : sources) {
            json.ObjectBegin();
            json.Field("id", source.sourceId);
            json.Field("name", deskhub::media::SourceName(source.name, source.sourceId));
            json.Field("width", source.width);
            json.Field("height", source.height);
            json.ObjectEnd();
        }
        json.ArrayEnd();
        json.FieldBegin("caps");
        json.ObjectBegin();
        json.Field("acceptsInput", caps.acceptsInput);
        json.Field("terminal", caps.terminal);
        json.Field("audio", caps.audio);
        json.Field("files", caps.files);
        json.ObjectEnd();
        json.ObjectEnd();
        PrintLine(json.Text());
        return ExitCode::Ok;
    }

    if (sources.empty()) {
        PrintLine(deskhub::ui::NoDisplaySharedNote(caps.terminal, caps.files));
    } else {
        Table table;
        table.Row({"ID", "NAME", "SIZE"});
        for (const deskhub::SourceInfo& source : sources)
            table.Row({std::to_string(unsigned(source.sourceId)),
                deskhub::media::SourceName(source.name, source.sourceId),
                deskhub::media::SourceSizeLabel(source.width, source.height)});
        table.Print();
    }

    if (!command.quiet) {
        const auto yesNo = [](bool value) { return value ? "yes" : "no"; };
        PrintLine("");
        PrintLine(std::string("accepts input: ") + yesNo(caps.acceptsInput));
        PrintLine(std::string("remote shell:  ") + yesNo(caps.terminal));
        PrintLine(std::string("audio:         ") + yesNo(caps.audio));
        PrintLine(std::string("file transfer: ") + yesNo(caps.files));
    }
    return ExitCode::Ok;
}

}
