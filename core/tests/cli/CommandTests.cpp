#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/cli/Command.h"

#include <cstdio>
#include <string>
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

    const cli::Command scoped = Parse({"sources", "--help"});
    Check(Ok(scoped, cli::Verb::Help), "a flag after a command still asks for help");
    Check(scoped.helpFor == cli::Verb::Sources, "help is scoped to that command");

    const cli::Command topic = Parse({"help", "sources"});
    Check(Ok(topic, cli::Verb::Help), "help takes a topic");
    Check(topic.helpFor == cli::Verb::Sources, "the topic is remembered");
    Check(!Parse({"help", "nonsense"}).error.empty(), "an unknown topic is rejected");
}

void TestUnknownInput() {
    std::printf("[cli] unknown commands and options are refused, not ignored...\n");
    Check(!Parse({"shrare"}).error.empty(), "an unknown command is an error");
    Check(!Parse({"--nope"}).error.empty(), "an unknown global option is an error");
    Check(!Parse({"sources", "1.2.3.4", "--nope"}).error.empty(),
        "an unknown command option is an error");
    Check(!Parse({"displays", "extra"}).error.empty(), "a stray argument is an error");
}

void TestGlobalFlags() {
    std::printf("[cli] --json, --quiet and --verbose are accepted on either side...\n");
    Check(Parse({"--json", "displays"}).json, "before the command");
    Check(Parse({"displays", "--json"}).json, "after the command");
    Check(Parse({"displays", "--quiet"}).quiet, "--quiet");
    Check(Parse({"displays", "-q"}).quiet, "-q");
    Check(Parse({"displays", "--verbose"}).verbose, "--verbose");
    Check(Parse({"displays", "-v"}).verbose, "-v");
    Check(!Parse({"displays", "--json=1"}).error.empty(), "a switch takes no value");
}

void TestScanIsUnavailable() {
    std::printf("[cli] LAN scan is no longer a command...\n");
    Check(!Parse({"scan"}).error.empty(), "LAN scan is rejected");
    Check(cli::UsageText().find("  scan ") == std::string::npos,
        "the help page does not advertise LAN scanning");
}

void TestAddressParsing() {
    std::printf("[cli] an address carries its own port, or borrows the default...\n");
    const cli::Command bare = Parse({"sources", "192.168.1.10"});
    Check(Ok(bare, cli::Verb::Sources), "sources parses");
    Check(bare.port == kDeskhubPort, "the default port fills in");
    Check(bare.address == "192.168.1.10:" + std::to_string(kDeskhubPort), "the address carries the port");
    Check(!Parse({"sources", "192.168.1.10", "--identity", "phone"}).error.empty(),
        "a client no longer picks between keys: the machine has one");

    const cli::Command explicitPort = Parse({"sources", "192.168.1.10:50000"});
    Check(explicitPort.port == 50000, "a port in the address wins");
    Check(explicitPort.address == "192.168.1.10:50000", "and is kept in the address");

    Check(!Parse({"sources"}).error.empty(), "a missing address is an error");
    Check(!Parse({"sources", "1.2.3.4", "5.6.7.8"}).error.empty(), "two addresses are an error");
    Check(!Parse({"sources", "1.2.3.4:"}).error.empty(), "a trailing colon is an error");
    Check(!Parse({"sources", "1.2.3.4:0"}).error.empty(), "port 0 in an address is an error");
    Check(!Parse({"sources", "1.2.3.4:nope"}).error.empty(), "a non-numeric port is an error");
}

void TestLegacyAuthFlagsRejected() {
    std::printf("[cli] obsolete passcode and pairing options are rejected...\n");
    Check(!Parse({"sources", "1.2.3.4", "--passcode", "0417"}).error.empty(),
        "sources rejects a passcode");
    Check(!Parse({"connect", "1.2.3.4", "--passcode", "0417"}).error.empty(),
        "connect rejects a passcode");
    Check(!Parse({"shell", "1.2.3.4", "--passcode", "0417"}).error.empty(),
        "shell rejects a passcode");
    Check(!Parse({"send", "1.2.3.4", "a.txt", "--passcode", "0417"}).error.empty(),
        "send rejects a passcode");
    Check(!Parse({"share", "--passcode", "0417"}).error.empty(),
        "share rejects a passcode");
    Check(!Parse({"share", "--pairing", "allow"}).error.empty(),
        "share rejects pairing approval");
    Check(!Parse({"share", "--no-new-pairings"}).error.empty(),
        "share rejects the retired pairing switch");
    Check(cli::UsageText().find("--passcode") == std::string::npos,
        "general help has no passcode option");
    Check(cli::UsageText(cli::Verb::Share).find("--pairing") == std::string::npos,
        "share help has no pairing option");
}

void TestProbeIsUnavailable() {
    std::printf("[cli] probe is no longer a command...\n");
    Check(!Parse({"probe", "10.0.0.5"}).error.empty(), "probe is rejected");
}

void TestDevicesAndTrust() {
    std::printf("[cli] devices and trust are two separate stores, same shape...\n");
    Check(Parse({"devices"}).devices == cli::DevicesAction::List, "devices lists by default");
    Check(Parse({"devices", "list"}).devices == cli::DevicesAction::List, "devices list");
    Check(Parse({"devices", "--json"}).json, "devices takes global flags with no action");
    Check(Parse({"devices", "public"}).devices == cli::DevicesAction::Public,
        "devices public requests this machine's shareable key");
    Check(!Parse({"devices", "public", "laptop"}).error.empty(),
        "devices public takes no key name: the machine has one key");
    Check(!Parse({"devices", "identities"}).error.empty(),
        "there is no separate list of client identities any more");
    Check(!Parse({"devices", "generate", "laptop"}).error.empty(),
        "nor can extra client keys be generated");
    const cli::Command add = Parse({"devices", "add", "-"});
    Check(add.devices == cli::DevicesAction::Add && add.target == "-",
        "devices add reads public key text from stdin");
    Check(!Parse({"devices", "import", "client.pem"}).error.empty(),
        "private keys are no longer imported");

    const cli::Command forget = Parse({"devices", "forget", "SHA256:abc"});
    Check(forget.devices == cli::DevicesAction::Forget, "devices forget");
    Check(forget.target == "SHA256:abc", "the fingerprint is kept");
    Check(Parse({"devices", "forget", "all"}).devices == cli::DevicesAction::ForgetAll,
        "devices forget all");

    Check(Parse({"trust"}).trust == cli::TrustAction::List, "trust lists by default");
    Check(Parse({"trust", "public"}).trust == cli::TrustAction::Public,
        "a host can display its key for manual pinning");
    const cli::Command trustAdd = Parse({"trust", "add", "1.2.3.4:47777", "SHA256:abc"});
    Check(trustAdd.trust == cli::TrustAction::Add && trustAdd.target == "1.2.3.4:47777" &&
              trustAdd.value == "SHA256:abc",
        "trust add takes the address and host fingerprint");
    const cli::Command profile = Parse({"trust", "add", "1.2.3.4:47777", "SHA256:abc",
        "--name", "Office"});
    Check(profile.error.empty() && profile.deviceName == "Office",
        "trust add can save a separate alias");
    Check(!Parse({"trust", "add", "1.2.3.4:47777", "SHA256:abc", "--identity", "phone"})
              .error.empty(),
        "and no longer picks a client key per host");
    Check(Parse({"trust", "forget", "1.2.3.4"}).trust == cli::TrustAction::Forget, "trust forget");
    Check(Parse({"trust", "forget", "all"}).trust == cli::TrustAction::ForgetAll, "trust forget all");

    Check(!Parse({"devices", "wipe"}).error.empty(), "an unknown action is refused");
    Check(!Parse({"devices", "forget"}).error.empty(), "forget needs a target");
    Check(!Parse({"devices", "add"}).error.empty(), "add needs a public key");
    Check(!Parse({"trust", "add", "1.2.3.4"}).error.empty(),
        "trust add needs a fingerprint");
    Check(!Parse({"trust", "forget", "--json"}).error.empty(), "a flag is not a target");
}

void TestProfileCommands() {
    const cli::Command publicKey = Parse({"key", "public"});
    Check(Ok(publicKey, cli::Verb::Devices) && publicKey.devices == cli::DevicesAction::Public,
        "key public prints this machine's one key");
    Check(!Parse({"key", "generate", "--name", "laptop-a"}).error.empty(),
        "key generate is gone with named identities");
    Check(!Parse({"key", "import", "--name", "k", "--file", "private.pem"}).error.empty(),
        "so is key import");
    Check(!Parse({"key", "public", "--name", "laptop-a"}).error.empty(),
        "and key public takes no name");
    Check(Parse({"access", "add", "--stdin"}).target == "-", "access add reads stdin");
    Check(Parse({"access", "list", "--json"}).json, "access list supports JSON");
    Check(Parse({"access", "remove", "--fingerprint", "SHA256:abc"}).target ==
              "SHA256:abc",
        "access remove selects a fingerprint");
    Check(!Parse({"access", "remove"}).error.empty(), "access remove needs a fingerprint");
    const cli::Command requests = Parse({"access", "requests", "--json"});
    Check(Ok(requests, cli::Verb::Devices) && requests.devices == cli::DevicesAction::Requests &&
              requests.json,
        "access requests lists the devices waiting for approval");
    const cli::Command approve = Parse({"access", "approve", "--fingerprint", "SHA256:abc"});
    Check(Ok(approve, cli::Verb::Devices) && approve.devices == cli::DevicesAction::Approve &&
              approve.target == "SHA256:abc",
        "access approve names the device to let in");
    const cli::Command deny = Parse({"access", "deny", "--fingerprint", "SHA256:abc"});
    Check(Ok(deny, cli::Verb::Devices) && deny.devices == cli::DevicesAction::Deny &&
              deny.target == "SHA256:abc",
        "access deny names the device to turn away");
    Check(!Parse({"access", "approve"}).error.empty(), "access approve needs a fingerprint");
    Check(!Parse({"access", "deny"}).error.empty(), "so does access deny");
    const cli::Command added = Parse({"host", "add", "office", "--address",
        "192.168.1.10:47777", "--host-key-stdin"});
    Check(added.error.empty() && added.verb == cli::Verb::Host &&
              added.trust == cli::TrustAction::Add && added.profileAlias == "office" &&
              added.target == "192.168.1.10:47777" && added.value == "-",
        "host add stores an alias, endpoint and key source");
    Check(!Parse({"host", "add", "office", "--address", "192.168.1.10", "--identity", "k",
                     "--host-key-stdin"})
              .error.empty(),
        "host add no longer takes a client key");
    Check(Parse({"host", "list", "--json"}).json, "host list supports JSON");
    Check(Parse({"host", "update", "office", "--address", "10.0.0.9"}).trust ==
              cli::TrustAction::Update,
        "host update can change the address");
    Check(Parse({"host", "remove", "office"}).trust == cli::TrustAction::Forget,
        "host remove selects an alias");
    Check(Parse({"host-key", "public"}).trust == cli::TrustAction::Public,
        "host-key public exports the TLS host key");
    Check(!Parse({"host", "add", "office"}).error.empty(), "host add needs required fields");
    Check(Ok(Parse({"host", "--help"}), cli::Verb::Help), "host help works without an action");
    Check(Ok(Parse({"host", "add", "--help"}), cli::Verb::Help),
        "host add can request help without an alias");
    Check(Ok(Parse({"access", "list", "--help"}), cli::Verb::Help),
        "access list can request help");
    const cli::Command connected = Parse({"--config-dir", "/tmp/deskhub-test",
        "connect", "office"});
    Check(connected.error.empty() && connected.profileAlias == "office" &&
              connected.configDir == "/tmp/deskhub-test",
        "connect accepts an alias and config dir");
    Check(Parse({"sources", "office"}).profileAlias == "office",
        "sources accepts a saved host alias");
    Check(Parse({"shell", "office"}).profileAlias == "office",
        "shell accepts a saved host alias");
    Check(Parse({"send", "office", "file.txt"}).profileAlias == "office",
        "send accepts a saved host alias");
    Check(Parse({"host", "list", "--config-dir", "/tmp/deskhub-test"}).configDir ==
              "/tmp/deskhub-test",
        "config dir also works after a command");
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
    Check(!Parse({"share", "--no-status"}).share.status, "--no-status");
    Check(Parse({"share", "--bind", "192.168.1.10"}).share.bindIp.value_or("") == "192.168.1.10",
        "--bind takes an address");
    Check(Parse({"share", "--name", "study pc"}).deviceName.value_or("") == "study pc", "--name");
    Check(Parse({"share", "--port", "47999"}).portGiven, "--port is remembered as chosen");
    Check(!Parse({"share"}).portGiven, "and without it the settings file decides");

    Check(!Parse({"share", "--fps", "0"}).error.empty(), "zero frames a second is refused");
    Check(!Parse({"share", "--fps", "1000"}).error.empty(), "an impossible frame rate is refused");
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
    std::printf("[cli] shell wants a host and optional name...\n");
    const cli::Command command = Parse({"shell", "10.0.0.5", "--name", "laptop"});
    Check(Ok(command, cli::Verb::Shell), "shell parses");
    Check(command.address == "10.0.0.5:" + std::to_string(kDeskhubPort), "the address");
    Check(command.deviceName.value_or("") == "laptop", "the name");
    Check(!Parse({"shell", "10.0.0.5", "--identity", "phone"}).error.empty(),
        "shell no longer chooses between client keys");
    Check(!Parse({"shell"}).error.empty(), "shell needs an address");
    Check(!Parse({"shell", "1.2.3.4", "--fps", "30"}).error.empty(), "a shell has no frame rate");
    const cli::Command resumed = Parse({"shell", "10.0.0.5", "--resume", "3"});
    Check(Ok(resumed, cli::Verb::Shell) && resumed.shell.resumeId == 3, "--resume names the shell to pick back up");
    Check(!resumed.shell.list, "without asking for the list");
    const cli::Command listed = Parse({"shell", "10.0.0.5", "--list"});
    Check(Ok(listed, cli::Verb::Shell) && listed.shell.list, "--list asks what the host is keeping");
    Check(listed.shell.resumeId == 0, "without naming one");
    Check(!Parse({"shell", "1.2.3.4", "--resume", "0"}).error.empty(), "zero is no session id");
    Check(!Parse({"shell", "1.2.3.4", "--resume", "abc"}).error.empty(), "nor is text");
    Check(!Parse({"shell", "1.2.3.4", "--resume"}).error.empty(), "a missing id is usage");
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
    Check(!Parse({"connect", "1.2.3.4", "--identity", "laptop-a"}).error.empty(),
        "connect no longer chooses between client keys");
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
    Check(!Parse({"send", "10.0.0.4", "report.pdf", "--identity", "phone"}).error.empty(),
        "send no longer chooses between client keys");

    const cli::Command many = Parse({"send", "host:47800", "a.txt", "b/c.txt", "../d.bin"});
    Check(Ok(many, cli::Verb::Send), "several files parse");
    Check(many.port == 47800, "the address carries its own port");
    Check(many.send.files.size() == 3 && many.send.files[2] == "../d.bin",
        "paths are taken as written, and reduced later");

    Check(!Parse({"send"}).error.empty(), "send without a host is refused");
    Check(!Parse({"send", "10.0.0.4"}).error.empty(), "send without a file is refused");
    Check(!Parse({"send", "10.0.0.4", "a.txt", "--nope"}).error.empty(),
        "an unknown flag is refused");

    const cli::Command named = Parse({"send", "10.0.0.4", "a.txt", "--name", "laptop"});
    Check(Ok(named, cli::Verb::Send), "a name is accepted");
    Check(named.deviceName && *named.deviceName == "laptop", "and carried through");

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

void TestAcceptNewHostKey() {
    std::printf("[cli] a new host key is only saved when the flag asks for it...\n");
    Check(!Parse({"sources", "192.168.1.10"}).acceptNewHostKey, "the default refuses unknown hosts");
    Check(Parse({"sources", "192.168.1.10", "--accept-new-host-key"}).acceptNewHostKey,
        "sources can accept a host seen for the first time");
    Check(Parse({"connect", "192.168.1.10", "--accept-new-host-key"}).acceptNewHostKey,
        "so can connect");
    Check(Parse({"shell", "192.168.1.10", "--accept-new-host-key"}).acceptNewHostKey,
        "and shell");
    Check(Parse({"send", "192.168.1.10", "a.txt", "--accept-new-host-key"}).acceptNewHostKey,
        "and send");
    Check(!Parse({"sources", "192.168.1.10", "--accept-new-host-key=yes"}).error.empty(),
        "the flag takes no value");
    Check(Parse({"sources", "192.168.1.10", "--approval-wait", "5"}).approvalWaitSeconds.value_or(0) == 5,
        "the approval wait can be shortened");
    Check(!Parse({"connect", "192.168.1.10", "--approval-wait", "0"}).error.empty(),
        "but not to nothing");
    Check(!Parse({"connect", "192.168.1.10", "--approval-wait", "601"}).error.empty(),
        "nor past ten minutes");
}

void TestKeyDeleteAndAccessClear() {
    std::printf("[cli] every client can be removed from the command line, keys cannot...\n");
    Check(!Parse({"key", "delete", "--name", "work"}).error.empty(),
        "the machine's one key cannot be deleted");
    const cli::Command cleared = Parse({"access", "clear"});
    Check(cleared.error.empty() && cleared.devices == cli::DevicesAction::ForgetAll,
        "access clear removes every allowed client");
}

void TestInvitesAndQr() {
    std::printf("[cli] a QR invite stands in for an address, and share can print one...\n");
    Check(Parse({"share", "--qr"}).share.qr, "share --qr asks for the QR code");
    Check(!Parse({"share"}).share.qr, "and it is off by default");
    const std::string invite =
        "deskhub://pair/AQHAqAEKuqEAAQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eH6ChoqOkpaanqKmqq6ytrq-w"
        "sbKztLW2t7i5uru8vb6_AA";
    const cli::Command connect = Parse({"connect", invite.c_str()});
    Check(Ok(connect, cli::Verb::Connect) && connect.pairingInvite == invite &&
              connect.address.empty(),
        "connect takes the invite instead of an address");
    Check(Ok(Parse({"sources", invite.c_str()}), cli::Verb::Sources), "so does sources");
    Check(!Parse({"connect", "deskhub://pair/notreal"}).error.empty(),
        "a damaged invite is refused with an explanation");
    Check(!Parse({"connect", invite.c_str(), "1.2.3.4"}).error.empty(),
        "an invite and an address together is one address too many");
}

void TestUsageText() {
    std::printf("[cli] every command can explain itself...\n");
    Check(cli::UsageText().find("deskhub-cli") != std::string::npos, "the summary names the program");
    Check(cli::UsageText().find("sources") != std::string::npos, "the summary lists the commands");

    const cli::Verb verbs[] = {cli::Verb::Displays, cli::Verb::Sources,
        cli::Verb::Devices, cli::Verb::Trust, cli::Verb::Settings,
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
    TestScanIsUnavailable();
    TestAddressParsing();
    TestLegacyAuthFlagsRejected();
    TestProbeIsUnavailable();
    TestDevicesAndTrust();
    TestProfileCommands();
    TestSettings();
    TestShareFlags();
    TestShareFileFlags();
    TestSend();
    TestShell();
    TestConnect();
    TestDisplaysForget();
    TestApplyShareOptions();
    TestPickDisplays();
    TestAcceptNewHostKey();
    TestKeyDeleteAndAccessClear();
    TestInvitesAndQr();
    TestUsageText();
}
