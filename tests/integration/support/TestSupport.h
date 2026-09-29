#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>

extern int g_failures;
void Check(bool ok, const char* what);

bool WaitFor(const std::function<bool()>& done, uint32_t timeoutMs);

std::string Hex(std::span<const uint8_t> bytes);

uint16_t NextTestPort();
