#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/cli/Command.h"
#include "deskhub/media/EncoderBackend.h"
#include "deskhub/media/ShareTypes.h"
#include "deskhub/control/CongestionControl.h"
#include "deskhub/transport/FecScheme.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

using namespace deskhub;

namespace {

cli::Command Parse(std::vector<const char*> args) {
    args.insert(args.begin(), "deskhub-cli");
    return cli::ParseCommand(int(args.size()), args.data());
}

bool Ok(const cli::Command& command, cli::Verb verb) {
    return command.error.empty() && command.verb == verb;
}

void TestNoArgumentsPrintsHelp() {
    std::printf("[cli] a bare invocation asks for help instead of guessing...\n");
    const cli::Command command = Parse({});
    Check(Ok(command, cli::Verb::Help), "no arguments means help");
    Check(command.helpFor == cli::Verb::None, "help with no topic");
}

void TestHelpAndVersionFlags() {
    std::printf("[cli] --help and --version win over everything else...\n");
    Check(Ok(Parse({"--help"}), cli::Verb::Help), "--help");
    Check(Ok(Parse({"-h"}), cli::Verb::Help), "-h");
    Check(Ok(Parse({"--version"}), cli::Verb::Version), "--version");
    Check(Ok(Parse({"-V"}), cli::Verb::Version), "-V");

    const cli::Command scoped = Parse({"scan", "--help"});
    Check(Ok(scoped, cli::Verb::Help), "a flag after a command still asks for help");
    Check(scoped.helpFor == cli::Verb::Scan, "help is scoped to that command");

    const cli::Command topic = Parse({"help", "sources"});
    Check(Ok(topic, cli::Verb::Help), "help takes a topic");
    Check(topic.helpFor == cli::Verb::Sources, "the topic is remembered");
    Check(!Parse({"help", "nonsense"}).error.empty(), "an unknown topic is rejected");
}

void TestUnknownInput() {
    std::printf("[cli] unknown commands and options are refused, not ignored...\n");
    Check(!Parse({"shrare"}).error.empty(), "an unknown command is an error");
    Check(!Parse({"--nope"}).error.empty(), "an unknown global option is an error");
    Check(!Parse({"scan", "--nope"}).error.empty(), "an unknown command option is an error");
    Check(!Parse({"displays", "extra"}).error.empty(), "a stray argument is an error");
}

void TestGlobalFlags() {
    std::printf("[cli] --json, --quiet and --verbose are accepted on either side...\n");
    Check(Parse({"--json", "scan"}).json, "before the command");
    Check(Parse({"scan", "--json"}).json, "after the command");
    Check(Parse({"scan", "--quiet"}).quiet, "--quiet");
    Check(Parse({"scan", "-q"}).quiet, "-q");
    Check(Parse({"scan", "--verbose"}).verbose, "--verbose");
    Check(Parse({"scan", "-v"}).verbose, "-v");
    Check(!Parse({"scan", "--json=1"}).error.empty(), "a switch takes no value");
}

void TestScanFlags() {
    std::printf("[cli] scan takes the port to look on, and nothing else...\n");
    const cli::Command defaults = Parse({"scan"});
    Check(Ok(defaults, cli::Verb::Scan), "scan parses");
    Check(defaults.port == kDeskhubPort, "the default port is the Deskhub port");

    Check(Parse({"scan", "--port", "50000"}).port == 50000, "a separate value");
    Check(Parse({"scan", "--port=50000"}).port == 50000, "an inline value");

    Check(!Parse({"scan", "--port"}).error.empty(), "a missing value is an error");
    Check(!Parse({"scan", "--port="}).error.empty(), "an empty inline value is an error");
    Check(!Parse({"scan", "--port", "0"}).error.empty(), "port 0 is refused");
    Check(!Parse({"scan", "--port", "65536"}).error.empty(), "a port past the range is refused");
    Check(!Parse({"scan", "--port", "http"}).error.empty(), "a non-numeric port is refused");
    Check(!Parse({"scan", "127.0.0.1"}).error.empty(), "scan takes no address");
    Check(!Parse({"scan", "--timeout", "900"}).error.empty(),
        "scan paces itself, so it takes no timeout");
}

void TestAddressParsing() {
    std::printf("[cli] an address carries its own port, or borrows the default...\n");
    const cli::Command bare = Parse({"sources", "192.168.1.10"});
    Check(Ok(bare, cli::Verb::Sources), "sources parses");
    Check(bare.port == kDeskhubPort, "the default port fills in");
    Check(bare.address == "192.168.1.10:" + std::to_string(kDeskhubPort), "the address carries the port");

    const cli::Command explicitPort = Parse({"sources", "192.168.1.10:50000"});
    Check(explicitPort.port == 50000, "a port in the address wins");
    Check(explicitPort.address == "192.168.1.10:50000", "and is kept in the address");

    Check(!Parse({"sources"}).error.empty(), "a missing address is an error");
    Check(!Parse({"sources", "1.2.3.4", "5.6.7.8"}).error.empty(), "two addresses are an error");
    Check(!Parse({"sources", "1.2.3.4:"}).error.empty(), "a trailing colon is an error");
    Check(!Parse({"sources", "1.2.3.4:0"}).error.empty(), "port 0 in an address is an error");
    Check(!Parse({"sources", "1.2.3.4:nope"}).error.empty(), "a non-numeric port is an error");
}

void TestPasscodeSources() {
    std::printf("[cli] a passcode can come from the flag, stdin or a file...\n");
    const cli::Command literal = Parse({"sources", "1.2.3.4", "--passcode", "0417"});
    Check(literal.passcodeSource == cli::PasscodeSource::Literal, "a literal passcode");
    Check(literal.passcode == "0417", "kept as given");

    const cli::Command stdinPasscode = Parse({"sources", "1.2.3.4", "--passcode", "-"});
    Check(stdinPasscode.passcodeSource == cli::PasscodeSource::Stdin, "'-' means stdin");
    Check(stdinPasscode.passcode.empty(), "nothing is read at parse time");

    const cli::Command filePasscode = Parse({"sources", "1.2.3.4", "--passcode", "@/run/secret"});
    Check(filePasscode.passcodeSource == cli::PasscodeSource::File, "'@' means a file");
    Check(filePasscode.passcode == "/run/secret", "the path is kept, not the passcode");

    Check(Parse({"sources", "1.2.3.4"}).passcodeSource == cli::PasscodeSource::Absent,
        "no flag means no passcode yet");
    Check(!Parse({"sources", "1.2.3.4", "--passcode", "12"}).error.empty(),
        "a short passcode is refused");
    Check(!Parse({"sources", "1.2.3.4", "--passcode", "abcd"}).error.empty(),
        "a non-numeric passcode is refused");
    Check(!Parse({"sources", "1.2.3.4", "--passcode", "@"}).error.empty(),
        "'@' with no path is refused");
    Check(!Parse({"sources", "1.2.3.4", "--passcode"}).error.empty(),
        "a missing passcode value is refused");
    Check(!Parse({"probe", "1.2.3.4", "--passcode", "0417"}).error.empty(),
        "probe asks nothing that needs a passcode");
}

void TestProbe() {
    std::printf("[cli] probe wants one address, and is the only one that waits...\n");
    const cli::Command command = Parse({"probe", "10.0.0.5:47000", "--timeout", "900"});
    Check(Ok(command, cli::Verb::Probe), "probe parses");
    Check(command.port == 47000, "the port comes from the address");
    Check(command.timeoutMs == 900, "the timeout is taken");
    Check(Parse({"probe", "1.2.3.4"}).timeoutMs == cli::kDefaultTimeoutMs, "the default timeout");
    Check(!Parse({"probe"}).error.empty(), "probe needs an address");
    Check(!Parse({"probe", "1.2.3.4", "--timeout", "0"}).error.empty(), "a zero timeout is refused");
    Check(!Parse({"probe", "1.2.3.4", "--timeout", "600000"}).error.empty(),
        "an absurd timeout is refused");
    Check(!Parse({"probe", "1.2.3.4", "--timeout"}).error.empty(), "a missing timeout is refused");
    Check(!Parse({"probe", "1.2.3.4", "--timeout", "soon"}).error.empty(),
        "a non-numeric timeout is refused");
    Check(!Parse({"sources", "1.2.3.4", "--timeout", "900"}).error.empty(),
        "sources waits on the host's own reply, so it takes no timeout");
}

void TestDevicesAndTrust() {
    std::printf("[cli] devices and trust are two separate stores, same shape...\n");
    Check(Parse({"devices"}).devices == cli::DevicesAction::List, "devices lists by default");
    Check(Parse({"devices", "list"}).devices == cli::DevicesAction::List, "devices list");
    Check(Parse({"devices", "--json"}).json, "devices takes global flags with no action");

    const cli::Command forget = Parse({"devices", "forget", "SHA256:abc"});
    Check(forget.devices == cli::DevicesAction::Forget, "devices forget");
    Check(forget.target == "SHA256:abc", "the fingerprint is kept");
    Check(Parse({"devices", "forget", "all"}).devices == cli::DevicesAction::ForgetAll,
        "devices forget all");

    Check(Parse({"trust"}).trust == cli::TrustAction::List, "trust lists by default");
    Check(Parse({"trust", "forget", "1.2.3.4"}).trust == cli::TrustAction::Forget, "trust forget");
    Check(Parse({"trust", "forget", "all"}).trust == cli::TrustAction::ForgetAll, "trust forget all");

    Check(!Parse({"devices", "wipe"}).error.empty(), "an unknown action is refused");
    Check(!Parse({"devices", "forget"}).error.empty(), "forget needs a target");
    Check(!Parse({"trust", "forget", "--json"}).error.empty(), "a flag is not a target");
}

void TestSettings() {
    std::printf("[cli] settings reads and writes the file the desktop app uses...\n");
    Check(Parse({"settings"}).settings == cli::SettingsAction::List, "settings lists by default");

    const cli::Command get = Parse({"settings", "get", "fps"});
    Check(get.settings == cli::SettingsAction::Get, "settings get");
    Check(get.key == "fps", "the key is kept");

    const cli::Command set = Parse({"settings", "set", "bind_ip=192.168.1.10"});
    Check(set.settings == cli::SettingsAction::Set, "settings set");
    Check(set.key == "bind_ip", "the key is split off");
    Check(set.value == "192.168.1.10", "the value keeps its dots");

    Check(!Parse({"settings", "get"}).error.empty(), "get needs a key");
    Check(!Parse({"settings", "set"}).error.empty(), "set needs a pair");
    Check(!Parse({"settings", "set", "fps"}).error.empty(), "set needs an equals sign");
    Check(!Parse({"settings", "set", "=90"}).error.empty(), "set needs a key before the equals");
    Check(!Parse({"settings", "set", "fps="}).error.empty(), "set needs a value after the equals");
    Check(!Parse({"settings", "reset"}).error.empty(), "an unknown action is refused");
}

deskhub::media::ShareSource Display(const char* name, uint32_t width, uint32_t height) {
    deskhub::media::ShareSource source;
    source.name = name;
    source.width = width;
    source.height = height;
    return source;
}

void TestShareFlags() {
    std::printf("[cli] share takes the whole host role on the command line...\n");
    const cli::Command bare = Parse({"share"});
    Check(Ok(bare, cli::Verb::Share), "share parses with nothing else");
    Check(bare.share.screen && !bare.share.terminal, "the screen goes out, the shell does not");
    Check(bare.share.displays.empty(), "no --display means every display");
    Check(bare.share.pairing == cli::PairingPolicy::Deny, "a new machine is turned away by default");
    Check(bare.share.statusIntervalMs == cli::kDefaultStatusIntervalMs, "the default status pace");
    Check(!bare.share.fps && !bare.share.bitrateMbps && !bare.share.audio,
        "what is not named is left to the settings file");

    const cli::Command picked =
        Parse({"share", "--display", "0", "--display", "HDMI", "--terminal", "--no-input"});
    Check(picked.share.displays.size() == 2, "--display can be given more than once");
    Check(picked.share.terminal, "--terminal");
    Check(picked.share.allowInput && *picked.share.allowInput == false, "--no-input");

    Check(Parse({"share", "--audio"}).share.audio.value_or(false), "--audio turns sound on");
    Check(Parse({"share", "--no-audio"}).share.audio.has_value() &&
              !*Parse({"share", "--no-audio"}).share.audio,
        "--no-audio turns it off");
    Check(Parse({"share", "--fps", "30"}).share.fps.value_or(0) == 30, "--fps");
    Check(Parse({"share", "--bitrate=8"}).share.bitrateMbps.value_or(0) == 8, "--bitrate");
    Check(Parse({"share", "--max-dim", "1280"}).share.maxDim.value_or(0) == 1280, "--max-dim");
    Check(Parse({"share", "--pairing", "allow"}).share.pairing == cli::PairingPolicy::Allow,
        "--pairing allow");
    Check(Parse({"share", "--pairing", "ask"}).share.pairing == cli::PairingPolicy::Ask,
        "--pairing ask");
    Check(!Parse({"share", "--no-status"}).share.status, "--no-status");
    Check(Parse({"share", "--bind", "192.168.1.10"}).share.bindIp.value_or("") == "192.168.1.10",
        "--bind takes an address");
    Check(Parse({"share", "--name", "study pc"}).deviceName.value_or("") == "study pc", "--name");
    Check(Parse({"share", "--port", "47999"}).portGiven, "--port is remembered as chosen");
    Check(!Parse({"share"}).portGiven, "and without it the settings file decides");

    Check(!Parse({"share", "--fps", "0"}).error.empty(), "zero frames a second is refused");
    Check(!Parse({"share", "--fps", "1000"}).error.empty(), "an impossible frame rate is refused");
    Check(!Parse({"share", "--pairing", "maybe"}).error.empty(), "an unknown pairing answer");
    Check(!Parse({"share", "--bind", "not-an-ip"}).error.empty(), "--bind wants a real address");
    Check(!Parse({"share", "--no-screen"}).error.empty(), "--no-screen alone shares nothing");
    Check(Ok(Parse({"share", "--no-screen", "--terminal"}), cli::Verb::Share),
        "--no-screen with --terminal shares the shell");
    Check(!Parse({"share", "--no-screen", "--terminal", "--display", "0"}).error.empty(),
        "--display contradicts --no-screen");
    Check(!Parse({"share", "0"}).error.empty(), "a display is named with --display, not loose");
    Check(!Parse({"share", "--status-interval", "10"}).error.empty(), "too fast a status pace");
}

void TestShell() {
    std::printf("[cli] shell wants a host, a passcode and a name...\n");
    const cli::Command command =
        Parse({"shell", "10.0.0.5", "--passcode", "0417", "--name", "laptop"});
    Check(Ok(command, cli::Verb::Shell), "shell parses");
    Check(command.address == "10.0.0.5:" + std::to_string(kDeskhubPort), "the address");
    Check(command.passcodeSource == cli::PasscodeSource::Literal, "the passcode");
    Check(command.deviceName.value_or("") == "laptop", "the name");
    Check(!Parse({"shell"}).error.empty(), "shell needs an address");
    Check(!Parse({"shell", "1.2.3.4", "--fps", "30"}).error.empty(), "a shell has no frame rate");
}

void TestConnect() {
    std::printf("[cli] connect names the host, the screen and who may type...\n");
    const cli::Command plain = Parse({"connect", "192.168.1.10"});
    Check(Ok(plain, cli::Verb::Connect), "connect parses");
    Check(plain.connect.control, "the pointer and keyboard go across by default");
    Check(plain.connect.sources.empty(), "no --source means the host's first screen");
    Check(!plain.connect.audio, "sound is left to the settings file");

    const cli::Command picked = Parse({"connect", "192.168.1.10:47999", "--source", "1",
        "--source", "HDMI", "--view-only", "--no-audio", "--name", "couch"});
    Check(picked.port == 47999, "the port comes from the address");
    Check(picked.connect.sources.size() == 2, "--source can be given more than once");
    Check(!picked.connect.control, "--view-only");
    Check(picked.connect.audio.has_value() && !*picked.connect.audio, "--no-audio");
    Check(picked.deviceName.value_or("") == "couch", "--name");
    Check(Parse({"connect", "1.2.3.4", "--audio"}).connect.audio.value_or(false), "--audio");

    Check(!Parse({"connect"}).error.empty(), "connect needs an address");
    Check(!Parse({"connect", "1.2.3.4", "2.3.4.5"}).error.empty(), "one address, not two");
    Check(!Parse({"connect", "1.2.3.4", "--fps", "30"}).error.empty(), "a viewer sets no frame rate");
    Check(!Parse({"connect", "1.2.3.4", "--source"}).error.empty(), "--source needs a value");
}

void TestDisplaysForget() {
    std::printf("[cli] displays can drop the screen choice this machine saved...\n");
    Check(Parse({"displays", "--forget"}).forget, "--forget is taken");
    Check(!Parse({"displays"}).forget, "and is off otherwise");
    Check(!Parse({"scan", "--forget"}).error.empty(), "only displays takes it");
}

void TestApplyShareOptions() {
    std::printf("[cli] a flag beats the settings file, and silence leaves it alone...\n");
    deskhub::ui::UiSettings stored;
    stored.fps = 60;
    stored.bitrateMbps = 20;
    stored.port = 47777;
    stored.deviceName = "stored name";
    stored.shareAudio = true;
    stored.allowInput = true;

    const deskhub::ui::UiSettings untouched =
        cli::ApplyShareOptions(Parse({"share"}), stored);
    Check(untouched == stored, "share with no flags changes nothing");

    const deskhub::ui::UiSettings changed = cli::ApplyShareOptions(
        Parse({"share", "--fps", "30", "--no-audio", "--no-input", "--port", "47999", "--name",
            "cli name"}),
        stored);
    Check(changed.fps == 30, "--fps wins");
    Check(!changed.shareAudio, "--no-audio wins");
    Check(!changed.allowInput, "--no-input wins");
    Check(changed.port == 47999, "--port wins");
    Check(changed.deviceName == "cli name", "--name wins");
    Check(changed.bitrateMbps == stored.bitrateMbps, "what was not named is left alone");
}

void TestPickDisplays() {
    std::printf("[cli] naming a display by id, by name, or not at all...\n");
    const std::vector<deskhub::media::ShareSource> screens = {
        Display("Built-in Retina", 2560, 1600), Display("HDMI-1", 1920, 1080),
        Display("HDMI-2", 1920, 1080)};

    Check(cli::PickDisplays({}, screens).indices.size() == 3, "no choice means all of them");
    Check(cli::PickDisplays({"all"}, screens).indices.size() == 3, "'all' means all of them");

    const cli::DisplayPick byId = cli::PickDisplays({"2", "0"}, screens);
    Check(byId.error.empty() && byId.indices.size() == 2, "two ids pick two displays");
    Check(byId.indices[0] == 0 && byId.indices[1] == 2, "and they come back in screen order");

    const cli::DisplayPick byName = cli::PickDisplays({"retina"}, screens);
    Check(byName.error.empty() && byName.indices.size() == 1, "a name matches, ignoring case");
    Check(byName.indices[0] == 0, "the right one");

    Check(cli::PickDisplays({"0", "0"}, screens).indices.size() == 1, "asking twice is asking once");
    Check(!cli::PickDisplays({"7"}, screens).error.empty(), "there is no display 7");
    Check(!cli::PickDisplays({"projector"}, screens).error.empty(), "no display is called that");
    Check(!cli::PickDisplays({"HDMI"}, screens).error.empty(), "an ambiguous name is refused");
    Check(!cli::PickDisplays({"any"}, {}).error.empty(), "with no displays there is nothing to pick");
}

void TestSend() {
    std::printf("[cli] send names a host and the files to put on it...\n");
    const cli::Command one = Parse({"send", "10.0.0.4", "report.pdf"});
    Check(Ok(one, cli::Verb::Send), "a host and one file parse");
    Check(one.address == "10.0.0.4:" + std::to_string(kDeskhubPort) && one.port == kDeskhubPort,
        "the address is the first word, with the default port filled in");
    Check(one.send.files.size() == 1 && one.send.files[0] == "report.pdf",
        "and the rest are files");

    const cli::Command many = Parse({"send", "host:47800", "a.txt", "b/c.txt", "../d.bin"});
    Check(Ok(many, cli::Verb::Send), "several files parse");
    Check(many.port == 47800, "the address carries its own port");
    Check(many.send.files.size() == 3 && many.send.files[2] == "../d.bin",
        "paths are taken as written, and reduced later");

    Check(!Parse({"send"}).error.empty(), "send without a host is refused");
    Check(!Parse({"send", "10.0.0.4"}).error.empty(), "send without a file is refused");
    Check(!Parse({"send", "10.0.0.4", "a.txt", "--nope"}).error.empty(),
        "an unknown flag is refused");

    const cli::Command named = Parse({"send", "10.0.0.4", "a.txt", "--passcode", "0417",
        "--name", "laptop"});
    Check(Ok(named, cli::Verb::Send), "a passcode and a name are accepted");
    Check(named.passcode == "0417" && named.deviceName && *named.deviceName == "laptop",
        "and are carried through");

    std::vector<const char*> flood{"send", "10.0.0.4"};
    for (size_t i = 0; i <= kMaxTransferFiles; ++i) flood.push_back("f.bin");
    Check(!Parse(flood).error.empty(), "more files than one batch carries is refused");

    const cli::Command scoped = Parse({"send", "--help"});
    Check(Ok(scoped, cli::Verb::Help) && scoped.helpFor == cli::Verb::Send,
        "send explains itself");
}

void TestShareFileFlags() {
    std::printf("[cli] taking files is asked for on the command line, like a shell...\n");
    const cli::Command off = Parse({"share"});
    Check(Ok(off, cli::Verb::Share), "a bare share parses");
    Check(!off.share.files, "and takes no files unless it is asked to");
    Check(!off.share.terminal, "exactly as it shares no shell unless it is asked to");

    const cli::Command on = Parse({"share", "--files"});
    Check(Ok(on, cli::Verb::Share) && on.share.files, "--files turns it on");

    const cli::Command only = Parse({"share", "--no-screen", "--files"});
    Check(Ok(only, cli::Verb::Share) && only.share.files && !only.share.screen,
        "a machine can share nothing but a place to put files");
    Check(!Parse({"share", "--no-screen"}).error.empty(),
        "while --no-screen on its own still leaves nothing to share");

    const cli::Command where = Parse({"share", "--files", "--files-dir", "/srv/incoming"});
    Check(Ok(where, cli::Verb::Share), "a folder can be named");
    Check(where.share.filesDir && *where.share.filesDir == "/srv/incoming",
        "and is carried through");
    Check(!Parse({"share", "--files-dir"}).error.empty(), "an empty --files-dir is refused");

    ui::UiSettings settings;
    Check(settings.transferDir.empty(), "a fresh machine has no folder of its own");
    const ui::UiSettings applied = cli::ApplyShareOptions(where, settings);
    Check(applied.transferDir == "/srv/incoming", "the folder is a saved preference");
    Check(ui::ParseUiSettings(ui::SerializeUiSettings(applied)).transferDir == "/srv/incoming",
        "and survives a round trip");
    Check(ui::SerializeUiSettings(applied).find("accept_files") == std::string::npos,
        "while the tick itself is not saved: it is a source, like the terminal");
}

void TestFecFlag() {
    std::printf("[cli] --fec names a scheme both ends must be told about...\n");

    Check(!Parse({"share"}).fecScheme.has_value(), "no --fec leaves the built-in default alone");
    Check(!Parse({"connect", "10.0.0.4"}).fecScheme.has_value(), "the same on the viewer side");

    const cli::Command shared = Parse({"share", "--fec", "xor"});
    Check(Ok(shared, cli::Verb::Share) && shared.fecScheme.value_or("") == "xor",
        "share takes --fec");
    const cli::Command viewed = Parse({"connect", "10.0.0.4", "--fec=xor"});
    Check(Ok(viewed, cli::Verb::Connect) && viewed.fecScheme.value_or("") == "xor",
        "connect takes --fec too, since parity has to be read with the scheme that wrote it");

    const cli::Command wrong = Parse({"share", "--fec", "reed-solomon"});
    Check(!wrong.error.empty(), "a scheme that is not built in is refused, not silently ignored");
    Check(wrong.error.find("xor") != std::string::npos,
        "and the refusal lists what this build actually has");

    for (std::string_view name : deskhub::FecSchemeNames()) {
        const std::string text(name);
        const cli::Command every = Parse({"share", "--fec", text.c_str()});
        Check(every.error.empty(), "every registered scheme name is accepted on the command line");
    }
}

void TestAudioBufferFlags() {
    std::printf("[cli] --audio-delay and --audio-adaptive reach the jitter buffer...\n");

    const cli::Command bare = Parse({"connect", "10.0.0.4"});
    Check(!bare.audioDelayMs && !bare.audioAdaptive,
        "naming neither leaves the shipping fixed target alone");

    const cli::Command set = Parse({"connect", "10.0.0.4", "--audio-delay", "120"});
    Check(Ok(set, cli::Verb::Connect) && set.audioDelayMs.value_or(0) == 120,
        "a target delay in milliseconds is taken");
    Check(!Parse({"connect", "10.0.0.4", "--audio-delay", "5"}).error.empty(),
        "a target under one audio frame is refused, because it cannot hold even one");
    Check(!Parse({"connect", "10.0.0.4", "--audio-delay", "5000"}).error.empty(),
        "and one past what the buffer can prefill is refused rather than silently clamped");

    const cli::Command on = Parse({"connect", "10.0.0.4", "--audio-adaptive"});
    Check(Ok(on, cli::Verb::Connect) && on.audioAdaptive.value_or(false),
        "the adaptive target can be asked for, which is how its curve was measured");
    const cli::Command off = Parse({"connect", "10.0.0.4", "--no-audio-adaptive"});
    Check(Ok(off, cli::Verb::Connect) && off.audioAdaptive.has_value() && !*off.audioAdaptive,
        "and asked against explicitly, which is not the same as leaving it off");

    Check(!Parse({"share", "--audio-delay", "120"}).error.empty(),
        "the host plays no audio, so this is a viewer flag only");
}

void TestCongestionControlFlag() {
    std::printf("[cli] --cc names which control loop decides the bitrate...\n");

    Check(!Parse({"share"}).share.congestionControl,
        "no --cc leaves the host on its built-in default");

    for (std::string_view name : deskhub::CongestionControlNames()) {
        const std::string text(name);
        const cli::Command every = Parse({"share", "--cc", text.c_str()});
        Check(every.error.empty() && every.share.congestionControl.value_or("") == text,
            "every registered control name is accepted on the command line");
    }

    const cli::Command wrong = Parse({"share", "--cc", "bbr"});
    Check(!wrong.error.empty(), "a control that is not built in is refused");
    Check(wrong.error.find("aimd") != std::string::npos,
        "and the refusal lists what this build actually has");

    Check(!Parse({"connect", "10.0.0.4", "--cc", "aimd"}).error.empty(),
        "the viewer has no say in it, so --cc is not a connect flag");
}

void TestEncoderFlag() {
    std::printf("[cli] --encoder names the backend to measure, not the first one that starts...\n");

    Check(!Parse({"share"}).share.encoder,
        "no --encoder leaves the factory picking whichever backend initialises first");

    for (std::string_view name : deskhub::media::EncoderBackendNames()) {
        const std::string text(name);
        const cli::Command every = Parse({"share", "--encoder", text.c_str()});
        Check(every.error.empty() && every.share.encoder.value_or("") == text,
            "every backend a host can be asked for is accepted on the command line");
    }

    const cli::Command wrong = Parse({"share", "--encoder", "qsv"});
    Check(!wrong.error.empty(),
        "a backend no client has is refused here rather than at the far end of a measurement");
    Check(wrong.error.find("nvenc") != std::string::npos,
        "and the refusal names the backends that exist");

    Check(!Parse({"connect", "10.0.0.4", "--encoder", "nvenc"}).error.empty(),
        "the encoder runs on the host, so this is not a connect flag");
}

void TestFecSweepFlags() {
    std::printf("[cli] --fec-parity, --fec-depth and --fec-arm reach the two axes that matter...\n");

    const cli::Command bare = Parse({"share"});
    Check(!bare.share.fecParity && !bare.share.fecDepth && !bare.share.fecArm,
        "naming none of them leaves the shipping behaviour untouched");

    const cli::Command pinned = Parse({"share", "--fec-parity", "3", "--fec-depth=8"});
    Check(Ok(pinned, cli::Verb::Share) && pinned.share.fecParity.value_or(0) == 3 &&
              pinned.share.fecDepth.value_or(0) == 8,
        "both take a value, in either spelling");

    Check(!Parse({"share", "--fec-parity", "0"}).error.empty(),
        "zero parity is refused rather than quietly meaning no FEC");
    Check(!Parse({"share", "--fec-depth", "0"}).error.empty(),
        "and zero depth is refused rather than quietly meaning the derived count");
    Check(!Parse({"share", "--fec-depth", "999"}).error.empty(),
        "a depth past what one wire byte can signal is refused at the command line");

    const cli::Command always = Parse({"share", "--fec-arm", "always"});
    Check(Ok(always, cli::Verb::Share) && always.share.fecArm.value_or("") == "always",
        "--fec-arm always holds parity on the wire for the length of a measurement");
    const cli::Command policy = Parse({"share", "--fec-arm=policy"});
    Check(Ok(policy, cli::Verb::Share) && policy.share.fecArm.value_or("") == "policy",
        "and policy asks for the shipping behaviour explicitly, which is not the same as "
        "leaving the flag off");
    const cli::Command never = Parse({"share", "--fec-arm=never"});
    Check(Ok(never, cli::Verb::Share) && never.share.fecArm.value_or("") == "never",
        "never turns FEC off, which is the only way to measure NACK on its own");

    const cli::Command wrong = Parse({"share", "--fec-arm", "on"});
    Check(!wrong.error.empty(), "any other word is refused");
    Check(wrong.error.find("never") != std::string::npos,
        "and the refusal names all three modes");
}

void TestVideoPathFlag() {
    std::printf("[cli] --video-path picks the leg the video rides on...\n");

    Check(!Parse({"share"}).videoPath.has_value(), "no --video-path leaves the default alone");

    const cli::Command raw = Parse({"share", "--video-path", "raw-udp"});
    Check(Ok(raw, cli::Verb::Share) && raw.videoPath.value_or("") == "raw-udp",
        "share takes --video-path raw-udp");
    const cli::Command quic = Parse({"connect", "10.0.0.4", "--video-path=quic-datagram"});
    Check(Ok(quic, cli::Verb::Connect) && quic.videoPath.value_or("") == "quic-datagram",
        "connect takes it too - the receiver only accepts raw video when it expects raw video");

    const cli::Command wrong = Parse({"share", "--video-path", "tcp"});
    Check(!wrong.error.empty(), "a path that does not exist is refused");
    Check(wrong.error.find("raw-udp") != std::string::npos,
        "and the refusal names the two that do");

    Check(deskhub::media::IsVideoPathName(deskhub::media::kVideoPathRawUdp) &&
              deskhub::media::IsVideoPathName(deskhub::media::kVideoPathQuicDatagram),
        "both names the CLI advertises are the ones the transport maps");
}

void TestUsageText() {
    std::printf("[cli] every command can explain itself...\n");
    Check(cli::UsageText().find("deskhub-cli") != std::string::npos, "the summary names the program");
    Check(cli::UsageText().find("sources") != std::string::npos, "the summary lists the commands");

    const cli::Verb verbs[] = {cli::Verb::Displays, cli::Verb::Scan, cli::Verb::Sources,
        cli::Verb::Probe, cli::Verb::Devices, cli::Verb::Trust, cli::Verb::Settings,
        cli::Verb::Version, cli::Verb::Share, cli::Verb::Shell, cli::Verb::Connect,
        cli::Verb::Send};
    for (cli::Verb verb : verbs) {
        const std::string name = cli::VerbName(verb);
        Check(!name.empty(), "the command has a name");
        Check(cli::UsageText(verb).find(name) != std::string::npos,
            "its usage text names it");
    }
    Check(cli::UsageText(cli::Verb::Help) == cli::UsageText(), "help falls back to the summary");
    Check(cli::UsageText(cli::Verb::None) == cli::UsageText(), "so does nothing in particular");
    Check(std::string(cli::VerbName(cli::Verb::None)).empty(), "no command has no name");
}

}

void RunCliCommandTests() {
    TestNoArgumentsPrintsHelp();
    TestHelpAndVersionFlags();
    TestUnknownInput();
    TestGlobalFlags();
    TestScanFlags();
    TestAddressParsing();
    TestPasscodeSources();
    TestProbe();
    TestDevicesAndTrust();
    TestSettings();
    TestShareFlags();
    TestShareFileFlags();
    TestSend();
    TestShell();
    TestConnect();
    TestDisplaysForget();
    TestApplyShareOptions();
    TestPickDisplays();
    TestFecFlag();
    TestAudioBufferFlags();
    TestCongestionControlFlag();
    TestEncoderFlag();
    TestFecSweepFlags();
    TestVideoPathFlag();
    TestUsageText();
}
