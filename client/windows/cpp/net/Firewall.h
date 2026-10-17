#pragma once

#include <string>

inline constexpr wchar_t kHostFirewallRuleName[] = L"Deskhub (host)";
inline constexpr wchar_t kUnattendedFirewallRuleName[] = L"Deskhub Secure Desktop (host)";

bool EnsureFirewallRule(const wchar_t* ruleName, const std::wstring& exe);
bool EnsureHostFirewallRule();
