#pragma once
#include <cstdint>

#include "deskhub/input/InputApplier.h"
#include "deskhub/protocol/Wire.h"
#include "deskhubp/input/LocalInputGate.h"

class InputInjector : public deskhub::InputApplier<InputInjector, uint16_t>,
                      public deskhubp::LocalInputGate<InputInjector> {
public:
    InputInjector();
    ~InputInjector();
    InputInjector(const InputInjector&) = delete;
    InputInjector& operator=(const InputInjector&) = delete;

    bool Init(uint32_t displayId);

    void Apply(const deskhub::InputEvent& e);

    void ReleaseAll();

    void SendKey(int32_t vk, int32_t scan, bool down);
    void SendButton(deskhub::MouseButton btn, bool down);
    void SendMoveAbsolute(int32_t nx, int32_t ny);
    void SendMoveRelative(int32_t dx, int32_t dy);
    void SendWheel(int32_t delta);
    void OnLocalUserTookOver();
    void OnLocalUserIdle();

private:
    bool SourceRect(double& x, double& y, double& w, double& h);
    void PostMouseAt(double x, double y, int32_t dx, int32_t dy);
    uint64_t CurrentFlags() const;

    uint32_t displayId_ = 0;
    void* source_ = nullptr;

    double rectX_ = 0, rectY_ = 0, rectW_ = 0, rectH_ = 0;
    uint64_t rectUs_ = 0;

    uint64_t lastClickUs_ = 0;
    double lastClickX_ = 0, lastClickY_ = 0;
    int64_t clickState_ = 1;
    deskhub::MouseButton lastClickBtn_ = deskhub::MouseButton::Left;
    int32_t wheelCarry_ = 0;
};
