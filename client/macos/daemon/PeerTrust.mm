#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "PeerTrust.h"

#include <string>

#include "deskhubp/diag/Log.h"
#include "secure/UnattendedNames.h"

namespace {

std::string AppRequirement() {
    std::string requirement = std::string("identifier \"") + kUnattendedBundleId + "\"";
#ifndef DESKHUB_DEV_BUILD
    requirement += std::string(" and anchor apple generic and certificate leaf[subject.OU] = \"") +
                   kUnattendedTeamId + "\"";
#endif
    return requirement;
}

NSDictionary* GuestAttributes(const deskhubp::LocalPeer& peer) {
    if (!peer.auditToken.empty()) {
        NSData* token = [NSData dataWithBytes:peer.auditToken.data()
                                       length:peer.auditToken.size()];
        return @{(__bridge NSString*)kSecGuestAttributeAudit : token};
    }
    return @{(__bridge NSString*)kSecGuestAttributePid : @(peer.pid)};
}

}

bool TrustedAppPeer(const deskhubp::LocalPeer& peer) {
    SecCodeRef code = nullptr;
    OSStatus status = SecCodeCopyGuestWithAttributes(nullptr,
        (__bridge CFDictionaryRef)GuestAttributes(peer), kSecCSDefaultFlags, &code);
    if (status != errSecSuccess || !code) {
        LOGW("[Daemon] Could not look at the code of pid %d: %d", peer.pid, int(status));
        return false;
    }
    SecRequirementRef requirement = nullptr;
    const std::string text = AppRequirement();
    CFStringRef requirementText = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(),
        kCFStringEncodingUTF8);
    status = SecRequirementCreateWithString(requirementText, kSecCSDefaultFlags, &requirement);
    CFRelease(requirementText);
    if (status != errSecSuccess || !requirement) {
        CFRelease(code);
        LOGE("[Daemon] The app requirement did not compile: %d", int(status));
        return false;
    }
    status = SecCodeCheckValidity(code, kSecCSDefaultFlags, requirement);
    CFRelease(requirement);
    CFRelease(code);
    return status == errSecSuccess;
}
