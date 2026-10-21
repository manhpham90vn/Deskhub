#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "net/Firewall.h"

#include <windows.h>
#include <netfw.h>

#include <cstdio>
#include <string>

#include "deskhubp/diag/Log.h"
#include "WinPaths.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

constexpr long kWantProfiles =
    NET_FW_PROFILE2_DOMAIN | NET_FW_PROFILE2_PRIVATE | NET_FW_PROFILE2_PUBLIC;

struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
    bool ok() const {
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }
};

bool PathEq(const wchar_t* a, const std::wstring& b) {
    return a && CompareStringOrdinal(a, -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

INetFwRules* OpenRules(INetFwPolicy2** outPolicy) {
    *outPolicy = nullptr;
    INetFwPolicy2* policy = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(INetFwPolicy2), (void**)&policy)) ||
        !policy)
        return nullptr;
    INetFwRules* rules = nullptr;
    if (FAILED(policy->get_Rules(&rules)) || !rules) {
        policy->Release();
        return nullptr;
    }
    *outPolicy = policy;
    return rules;
}

void ReleaseRules(INetFwRules* rules, INetFwPolicy2* policy) {
    if (rules) rules->Release();
    if (policy) policy->Release();
}

int InspectOwnRule(INetFwRules* rules, const wchar_t* ruleName, const std::wstring& exe) {
    BSTR name = SysAllocString(ruleName);
    if (!name) return 0;
    INetFwRule* rule = nullptr;
    int state = 0;
    if (SUCCEEDED(rules->Item(name, &rule)) && rule) {
        BSTR app = nullptr;
        rule->get_ApplicationName(&app);
        const bool appOk = PathEq(app, exe);
        if (app) SysFreeString(app);
        long prof = 0;
        if (appOk && SUCCEEDED(rule->get_Profiles(&prof)) &&
            (prof & kWantProfiles) == kWantProfiles)
            state = 2;
        else
            state = 1;
        rule->Release();
    }
    SysFreeString(name);
    return state;
}

void RemoveOwnRule(INetFwRules* rules, const wchar_t* ruleName) {
    if (BSTR name = SysAllocString(ruleName)) {
        rules->Remove(name);
        SysFreeString(name);
    }
}

bool IsEnabledInboundBlockFor(INetFwRule* rule, const std::wstring& exe) {
    NET_FW_RULE_DIRECTION dir = NET_FW_RULE_DIR_OUT;
    NET_FW_ACTION act = NET_FW_ACTION_ALLOW;
    VARIANT_BOOL enabled = VARIANT_FALSE;
    if (FAILED(rule->get_Direction(&dir)) || dir != NET_FW_RULE_DIR_IN) return false;
    if (FAILED(rule->get_Action(&act)) || act != NET_FW_ACTION_BLOCK) return false;
    if (FAILED(rule->get_Enabled(&enabled)) || enabled != VARIANT_TRUE) return false;
    BSTR app = nullptr;
    rule->get_ApplicationName(&app);
    const bool match = PathEq(app, exe);
    if (app) SysFreeString(app);
    return match;
}

void DisableRule(INetFwRule* rule) {
    BSTR name = nullptr;
    rule->get_Name(&name);
    const HRESULT hr = rule->put_Enabled(VARIANT_FALSE);
    if (SUCCEEDED(hr))
        LOGW("[Firewall] Disabled inbound block rule \"%ls\" that targeted Deskhub.",
            name ? name : L"");
    else
        LOGE("[Firewall] Could not disable block rule \"%ls\": hr=0x%08lx", name ? name : L"",
            (unsigned long)hr);
    if (name) SysFreeString(name);
}

void DisableBlockRulesFor(INetFwRules* rules, const std::wstring& exe) {
    IUnknown* unk = nullptr;
    if (FAILED(rules->get__NewEnum(&unk)) || !unk) return;
    IEnumVARIANT* en = nullptr;
    if (SUCCEEDED(unk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&en)) && en) {
        VARIANT v;
        VariantInit(&v);
        ULONG got = 0;
        while (en->Next(1, &v, &got) == S_OK && got) {
            INetFwRule* rule = nullptr;
            if (v.vt == VT_DISPATCH && v.pdispVal &&
                SUCCEEDED(v.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&rule)) &&
                rule) {
                if (IsEnabledInboundBlockFor(rule, exe)) DisableRule(rule);
                rule->Release();
            }
            VariantClear(&v);
        }
        en->Release();
    }
    unk->Release();
}

bool AddOwnRule(INetFwRules* rules, const wchar_t* ruleName, const std::wstring& exe) {
    INetFwRule* rule = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(INetFwRule), (void**)&rule)) ||
        !rule)
        return false;

    bool ok = false;
    BSTR name = SysAllocString(ruleName);
    BSTR desc = SysAllocString(L"Allow incoming connections for Deskhub host.");
    BSTR app = SysAllocString(exe.c_str());
    if (name && app) {
        rule->put_Name(name);
        if (desc) rule->put_Description(desc);
        rule->put_ApplicationName(app);
        rule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
        rule->put_Direction(NET_FW_RULE_DIR_IN);
        rule->put_Action(NET_FW_ACTION_ALLOW);
        rule->put_Enabled(VARIANT_TRUE);
        rule->put_Profiles(kWantProfiles);
        const HRESULT hr = rules->Add(rule);
        ok = SUCCEEDED(hr);
        if (!ok)
            LOGE("[Firewall] Add() failed: hr=0x%08lx", (unsigned long)hr);
    }
    SysFreeString(name);
    SysFreeString(desc);
    SysFreeString(app);
    rule->Release();
    return ok;
}

}

bool EnsureFirewallRule(const wchar_t* ruleName, const std::wstring& exe) {
    if (exe.empty()) return false;

    ComScope com;
    if (!com.ok()) {
        LOGE("[Firewall] COM init failed.");
        return false;
    }
    INetFwPolicy2* policy = nullptr;
    INetFwRules* rules = OpenRules(&policy);
    if (!rules) {
        LOGE("[Firewall] Could not open the firewall policy.");
        return false;
    }

    DisableBlockRulesFor(rules, exe);

    if (InspectOwnRule(rules, ruleName, exe) == 2) {
        ReleaseRules(rules, policy);
        return true;
    }

    RemoveOwnRule(rules, ruleName);
    AddOwnRule(rules, ruleName, exe);

    const bool ok = InspectOwnRule(rules, ruleName, exe) == 2;
    ReleaseRules(rules, policy);
    return ok;
}

bool EnsureHostFirewallRule() {
    return EnsureFirewallRule(kHostFirewallRuleName, SelfExePath());
}
