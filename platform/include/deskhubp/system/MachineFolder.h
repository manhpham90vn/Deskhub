#pragma once
#include <string>

namespace deskhubp {

bool EnterMachineFolder(const std::string& path);
bool UnattendedEnabled(const std::string& path);
bool WipeMachineFolder(const std::string& path);

}
