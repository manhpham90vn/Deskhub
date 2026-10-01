#pragma once
#include <cstdint>

#include "deskhub/input/InputApplier.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/input/LocalInputGate.h"

class InputInjector : public deskhub::InputApplier<InputInjector, uint16_t>,
                      public deskhubp::LocalInputGate<InputInjector> {
public:
    static constexpr const char* kKeyboardName = "Deskhub Keyboard";
    static constexpr const char* kPointerName = "Deskhub Mouse";
    static constexpr const char* kAbsPointerName = "Deskhub Absolute Mouse";

    InputInjector();
    ~InputInjector();
    InputInjector(const InputInjector&) = delete;
    InputInjector& operator=(const InputInjector&) = delete;

    bool Init(int32_t srcX, int32_t srcY, uint32_t srcW, uint32_t srcH, int32_t deskX,
        int32_t deskY, uint32_t deskW, uint32_t deskH);

    void Apply(const deskhub::InputEvent& e);

    void ReleaseAll();
    void ReleaseKey(int32_t vk, uint16_t native);

    void SendKey(int32_t vk, int32_t scan, bool down);
    void SendButton(deskhub::MouseButton btn, bool down);
    void SendMoveAbsolute(int32_t nx, int32_t ny);
    void SendMoveRelative(int32_t dx, int32_t dy);
    void SendWheel(int32_t delta);
    void OnLocalUserTookOver();
    void OnLocalUserIdle();

private:
    int kbdFd_ = -1;
    int mouseFd_ = -1;
    int absFd_ = -1;

    int32_t srcX_ = 0, srcY_ = 0;
    uint32_t srcW_ = 0, srcH_ = 0;
    int32_t deskX_ = 0, deskY_ = 0;
    uint32_t deskW_ = 0, deskH_ = 0;
    int32_t wheelCarry_ = 0;
};
