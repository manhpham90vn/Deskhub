#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "deskhub/protocol/Wire.h"

struct DHScreen;

class ViewerInput {
public:
    bool Attach(HWND hwnd, DHScreen* session);
    void Detach();

    bool OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    void ReleaseHeldInput();
    void OnRawInput(LPARAM lp);
    void EmitButton(deskhub::MouseButton button, bool down);

    HWND hwnd_ = nullptr;
    DHScreen* session_ = nullptr;
    bool attached_ = false;
    int buttonsDown_ = 0;
};
