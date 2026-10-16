#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <functional>

#include "deskhub/input/InputApplier.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/input/LocalInputGate.h"

class InputInjector : public deskhub::InputApplier<InputInjector, int32_t>,
                      public deskhubp::LocalInputGate<InputInjector> {
public:
    using SecureRoute = std::function<bool(const INPUT&)>;

    bool Init(HMONITOR monitor);
    void SetSecureRoute(SecureRoute route);

    void Apply(const deskhub::InputEvent& e);

    void ReleaseAll();
    void ReleaseKey(int32_t vk, int32_t native);

    void SendKey(int32_t vk, int32_t scan, bool down);
    void SendButton(deskhub::MouseButton btn, bool down);
    void SendMoveAbsolute(int32_t nx, int32_t ny);
    void SendMoveRelative(int32_t dx, int32_t dy);
    void SendWheel(int32_t delta);
    void OnLocalUserTookOver();
    void OnLocalUserIdle();

private:
    void Deliver(const INPUT& in);

    HMONITOR monitor_ = nullptr;
    SecureRoute secureRoute_;
};
