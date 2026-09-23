#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "net/Firewall.h"

#include <windows.h>
#include <netfw.h>

#include <cstdio>
#include <string>
#include <vector>

#include "deskhubp/diag/Log.h"
#include "WinPaths.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace {

constexpr wchar_t kRuleName[] = L"Deskhub (host)";

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

int InspectOwnRule(INetFwRules* rules, const std::wstring& exe) {
    BSTR name = SysAllocString(kRuleName);
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

void RemoveOwnRule(INetFwRules* rules) {
    if (BSTR name = SysAllocString(kRuleName)) {
        rules->Remove(name);
        SysFreeString(name);
    }
}

void RemoveConflictingBlockRules(INetFwRules* rules, const std::wstring& exe) {
    IUnknown* unk = nullptr;
    if (FAILED(rules->get__NewEnum(&unk)) || !unk) return;
    IEnumVARIANT* en = nullptr;
    if (SUCCEEDED(unk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&en)) && en) {
        std::vector<std::wstring> victims;
        VARIANT v;
        VariantInit(&v);
        ULONG got = 0;
        while (en->Next(1, &v, &got) == S_OK && got) {
            if (v.vt == VT_DISPATCH && v.pdispVal) {
                INetFwRule* r = nullptr;
                if (SUCCEEDED(v.pdispVal->QueryInterface(__uuidof(INetFwRule), (void**)&r)) && r) {
                    NET_FW_RULE_DIRECTION dir = NET_FW_RULE_DIR_IN;
                    NET_FW_ACTION act = NET_FW_ACTION_ALLOW;
                    BSTR app = nullptr, nm = nullptr;
                    r->get_Direction(&dir);
                    r->get_Action(&act);
                    r->get_ApplicationName(&app);
                    r->get_Name(&nm);
                    if (dir == NET_FW_RULE_DIR_IN && act == NET_FW_ACTION_BLOCK &&
                        PathEq(app, exe) && nm)
                        victims.emplace_back(nm);
                    if (app) SysFreeString(app);
                    if (nm) SysFreeString(nm);
                    r->Release();
                }
            }
            VariantClear(&v);
        }
        en->Release();
        for (const auto& n : victims)
            if (BSTR b = SysAllocString(n.c_str())) {
                rules->Remove(b);
                SysFreeString(b);
            }
    }
    unk->Release();
}

bool AddOwnRule(INetFwRules* rules, const std::wstring& exe) {
    INetFwRule* rule = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(INetFwRule), (void**)&rule)) ||
        !rule)
        return false;

    bool ok = false;
    BSTR name = SysAllocString(kRuleName);
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

bool EnsureHostFirewallRule() {
    const std::wstring exe = SelfExePath();
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

    if (InspectOwnRule(rules, exe) == 2) {
        ReleaseRules(rules, policy);
        return true;
    }

    RemoveOwnRule(rules);
    RemoveConflictingBlockRules(rules, exe);
    AddOwnRule(rules, exe);

    const bool ok = InspectOwnRule(rules, exe) == 2;
    ReleaseRules(rules, policy);
    return ok;
}
