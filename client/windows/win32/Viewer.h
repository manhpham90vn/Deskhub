#pragma once
#include <string>
#include <vector>

#include "deskhub/protocol/Wire.h"

void RunViewer(const std::string& addrUtf8, const std::vector<deskhub::SourceInfo>& sources,
    bool control);
