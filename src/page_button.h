#pragma once
#include <stdint.h>

// Poll an active-low button as a logical "pressed" value. Emit once after a
// stable press, never repeat while held, and ignore a button held at startup.
class PageButton {
public:
    bool update(bool pressed, uint32_t now) {
        if (!initialized_) {
            initialized_ = true;
            raw_ = stable_ = pressed;
            armed_ = !pressed;
            changedMs_ = now;
            return false;
        }
        if (pressed != raw_) { raw_ = pressed; changedMs_ = now; }
        if (raw_ != stable_ && static_cast<uint32_t>(now - changedMs_) >= 40) {
            stable_ = raw_;
            if (!stable_) armed_ = true;
            else if (armed_) { armed_ = false; return true; }
        }
        return false;
    }
private:
    uint32_t changedMs_ = 0;
    bool initialized_ = false;
    bool raw_ = false;
    bool stable_ = false;
    bool armed_ = true;
};
