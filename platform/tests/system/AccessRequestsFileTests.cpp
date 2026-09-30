#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"

#include <cstdio>
#include <string>

namespace {

void TestARequestIsRecordedApprovedAndDenied() {
    std::printf("[requests] an unknown device leaves a request the owner can approve or deny...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[requests] skipped: this build has no key support\n");
        return;
    }
    const IsolatedAppData data("deskhub-access-requests");
    const deskhubp::HostIdentity device = deskhubp::LoadOrCreateHostIdentity();
    Check(device.Valid(), "the requesting device has a key");

    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->empty(),
        "a fresh machine has no requests");
    const uint64_t before = deskhubp::AccessRequestsGeneration();
    Check(deskhubp::RememberAccessRequest(device.publicKey, "Manh phone", "10.0.0.2:47777"),
        "a request is written");
    Check(deskhubp::AccessRequestsGeneration() != before, "and the generation moves");
    auto listed = deskhubp::ListAccessRequests();
    Check(listed && listed->size() == 1 && listed->front().label == "Manh phone" &&
              listed->front().address == "10.0.0.2:47777" &&
              listed->front().fingerprint == device.fingerprint,
        "the request names the device, its address and its fingerprint");

    Check(deskhubp::RememberAccessRequest(device.publicKey, "Manh phone", "10.0.0.3:47777"),
        "the same device asking again is accepted");
    listed = deskhubp::ListAccessRequests();
    Check(listed && listed->size() == 1 && listed->front().address == "10.0.0.3:47777",
        "and updates its row instead of adding one");

    deskhub::Fingerprint stranger;
    stranger.bytes.fill(0x42);
    Check(!deskhubp::ApproveAccessRequest(stranger), "approving a fingerprint nobody asked with fails");
    Check(!deskhubp::IsClientKeyAuthorized(device.publicKey), "the device is not allowed in yet");
    Check(deskhubp::ApproveAccessRequest(device.fingerprint), "approving the request succeeds");
    Check(deskhubp::IsClientKeyAuthorized(device.publicKey), "and the device is now allowed in");
    const auto allowed = deskhubp::ListAuthorizedClients();
    Check(allowed && allowed->size() == 1 && allowed->front().label == "Manh phone",
        "under the name it gave");
    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->empty(),
        "and the request is gone");

    Check(deskhubp::RememberAccessRequest(device.publicKey, "", "10.0.0.2:47777"),
        "a device without a name can still ask");
    Check(deskhubp::DenyAccessRequest(device.fingerprint), "denying removes its request");
    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->empty(),
        "the list is empty after the denial");
    Check(!deskhubp::DenyAccessRequest(device.fingerprint), "denying twice reports nothing happened");

    Check(deskhubp::WriteAppDataFile(deskhubp::kAccessRequestsFileName, "damaged\n"),
        "the request file can be damaged for this test");
    Check(!deskhubp::ListAccessRequests().has_value(), "a damaged file lists nothing");
    Check(deskhubp::RememberAccessRequest(device.publicKey, "x", "10.0.0.2:47777"),
        "the next request replaces the damaged file");
    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->size() == 1,
        "with a readable one");
}

}

void RunAccessRequestsFileTests() {
    TestARequestIsRecordedApprovedAndDenied();
}
