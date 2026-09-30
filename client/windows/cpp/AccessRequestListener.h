#pragma once
#include <functional>

void SetAccessRequestedListener(std::function<void()> listener);
