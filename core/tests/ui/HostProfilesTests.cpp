#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/HostProfiles.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

Fingerprint KeyFor(uint8_t seed) {
    Fingerprint fp;
    for (size_t i = 0; i < kFingerprintBytes; ++i) fp.bytes[i] = uint8_t(seed + i + 1);
    return fp;
}

TrustStore StoreWithOffice() {
    TrustStore store;
    store.Insert(TrustedHost{"192.168.1.10:47777", "office", KeyFor(1), 100, 200, "laptop"});
    return store;
}

ui::HostProfileRequest Request(std::string alias, std::string address,
    std::optional<Fingerprint> hostKey = std::nullopt, std::string identity = {}) {
    return ui::HostProfileRequest{std::move(alias), std::move(address), hostKey,
        std::move(identity)};
}

void TestAliasRules() {
    std::printf("[hosts] a host name is short, plain ASCII and never empty...\n");
    Check(ui::IsValidHostAlias("office-pc_2"), "letters, digits, '-' and '_' are allowed");
    Check(!ui::IsValidHostAlias(""), "an empty name is refused");
    Check(!ui::IsValidHostAlias("office pc"), "a space is refused");
    Check(!ui::IsValidHostAlias("../etc"), "path characters are refused");
    Check(!ui::IsValidHostAlias(std::string(kMaxTrustLabelBytes + 1, 'a')),
        "a name longer than a label is refused");
}

void TestEndpointIsCanonical() {
    std::printf("[hosts] an address is stored with its port spelled out...\n");
    Check(ui::CanonicalHostEndpoint("192.168.1.10") == std::string("192.168.1.10:47777"),
        "a bare address gets the default port");
    Check(ui::CanonicalHostEndpoint(" 10.0.0.2:5000 ") == std::string("10.0.0.2:5000"),
        "surrounding spaces are dropped and a custom port is kept");
    Check(!ui::CanonicalHostEndpoint("office.local"), "a host name is not an address");
    Check(!ui::CanonicalHostEndpoint("10.0.0.2:0"), "port zero is refused");
    Check(!ui::CanonicalHostEndpoint(""), "an empty address is refused");
}

void TestAddNeedsEverything() {
    std::printf("[hosts] adding a host needs a name, an address and its key...\n");
    const TrustStore store = StoreWithOffice();
    const auto added = ui::PlanHostProfile(store, ui::HostProfileMode::Add,
        Request("home", "192.168.1.20", KeyFor(2)));
    Check(added.error == ui::HostProfileError::None && !added.existing,
        "a complete new host is accepted");
    Check(added.profile.endpoint == "192.168.1.20:47777" && added.profile.label == "home" &&
              added.profile.fingerprint == KeyFor(2),
        "and carries the canonical address, the name and the pinned key");
    Check(added.profile.identityName == ui::kDefaultIdentityName,
        "with no client key chosen the default one is used");

    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Add, Request("home", "192.168.1.20"))
                  .error == ui::HostProfileError::MissingHostKey,
        "a host without a pinned key is refused");
    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Add, Request("home", "", KeyFor(2)))
                  .error == ui::HostProfileError::InvalidAddress,
        "a host without an address is refused");
    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Add, Request("", "10.0.0.1", KeyFor(2)))
                  .error == ui::HostProfileError::InvalidAlias,
        "a host without a name is refused");
    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Add,
              Request("office", "10.0.0.1", KeyFor(2)))
                  .error == ui::HostProfileError::AliasExists,
        "a second host cannot take a name already in use");
    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Add,
              Request("home", "192.168.1.10:47777", KeyFor(2)))
                  .error == ui::HostProfileError::AddressInUse,
        "nor an address another saved host already uses");
}

void TestUpdateKeepsWhatIsNotChanged() {
    std::printf("[hosts] updating a host keeps every field the user left alone...\n");
    const TrustStore store = StoreWithOffice();
    const auto moved =
        ui::PlanHostProfile(store, ui::HostProfileMode::Update, Request("office", "10.0.0.9"));
    Check(moved.error == ui::HostProfileError::None && moved.existing,
        "a new address alone is enough for an update");
    Check(moved.profile.endpoint == "10.0.0.9:47777" && moved.profile.fingerprint == KeyFor(1),
        "the pinned key does not change with the address");
    Check(moved.profile.identityName == "laptop" && moved.profile.firstSeenUnix == 100 &&
              moved.profile.lastSeenUnix == 200,
        "nor does the client key or the history");

    const auto rekeyed = ui::PlanHostProfile(store, ui::HostProfileMode::Update,
        Request("office", "", KeyFor(9), "phone"));
    Check(rekeyed.profile.endpoint == "192.168.1.10:47777" &&
              rekeyed.profile.fingerprint == KeyFor(9) && rekeyed.profile.identityName == "phone",
        "a new key and client key keep the address");

    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Update, Request("missing", "10.0.0.1"))
                  .error == ui::HostProfileError::AliasMissing,
        "an unknown name cannot be updated");
}

void TestDuplicateNamesAreNotGuessed() {
    std::printf("[hosts] two hosts sharing a name are never picked between...\n");
    TrustStore store = StoreWithOffice();
    store.Insert(TrustedHost{"192.168.1.11:47777", "office", KeyFor(3), 0, 0, ""});
    Check(ui::FindHostProfile(store, "office").error == ui::HostProfileError::AliasAmbiguous,
        "the lookup reports the clash");
    Check(ui::PlanHostProfile(store, ui::HostProfileMode::Update, Request("office", "10.0.0.1"))
                  .error == ui::HostProfileError::AliasAmbiguous,
        "and an update refuses to choose one");
    Check(ui::ProfileIdentityName(store.Hosts().back()) == ui::kDefaultIdentityName,
        "a host saved without a client key uses the default one");
}

void TestFirstTrustGetsAUsableName() {
    std::printf("[hosts] a host trusted on first connection gets a name that can be edited...\n");
    TrustStore store;
    const std::string first = ui::SuggestHostAlias(store, "192.168.1.10:47777");
    Check(first == "192-168-1-10", "the default port is left out of the name");
    Check(ui::IsValidHostAlias(first), "and the name passes the same rules as a typed one");
    Check(ui::SuggestHostAlias(store, "10.0.0.2:5000") == "10-0-0-2-5000",
        "a custom port stays in the name");
    store.Insert(TrustedHost{"192.168.1.10:47777", first, KeyFor(1), 0, 0, ""});
    Check(ui::SuggestHostAlias(store, "192.168.1.10:47777") == "192-168-1-10-2",
        "a name already in use gets a number instead of a clash");
}

void TestEveryErrorHasText() {
    std::printf("[hosts] every refusal can be shown to the user...\n");
    for (uint8_t e = 1; e <= uint8_t(ui::HostProfileError::WriteFailed); ++e)
        Check(*ui::HostProfileErrorText(ui::HostProfileError(e)) != '\0',
            "each error has a sentence");
    Check(*ui::HostProfileErrorText(ui::HostProfileError::None) == '\0',
        "success says nothing");
}

}

void RunHostProfilesTests() {
    TestAliasRules();
    TestEndpointIsCanonical();
    TestAddNeedsEverything();
    TestUpdateKeepsWhatIsNotChanged();
    TestDuplicateNamesAreNotGuessed();
    TestFirstTrustGetsAUsableName();
    TestEveryErrorHasText();
}
