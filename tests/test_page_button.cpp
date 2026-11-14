#include "page_button.h"
#include <cassert>
#include <iostream>

int main() {
    PageButton button;
    assert(!button.update(false, 0));
    assert(!button.update(true, 10));
    assert(!button.update(false, 15)); // Contact bounce.
    assert(!button.update(true, 20));
    assert(!button.update(true, 59));
    assert(button.update(true, 60));
    assert(!button.update(true, 10000)); // A hold never repeats.
    assert(!button.update(false, 10010));
    assert(!button.update(true, 10015)); // A bouncing release must not re-arm.
    assert(!button.update(true, 10100));
    assert(!button.update(false, 10200));
    assert(!button.update(false, 10240));
    assert(!button.update(true, 10300));
    assert(button.update(true, 10340));

    PageButton heldAtStartup;
    assert(!heldAtStartup.update(true, 0));
    assert(!heldAtStartup.update(true, 1000));
    assert(!heldAtStartup.update(false, 1010));
    assert(!heldAtStartup.update(false, 1050));
    assert(!heldAtStartup.update(true, 1060));
    assert(heldAtStartup.update(true, 1100));

    PageButton rollover;
    assert(!rollover.update(false, UINT32_MAX - 100));
    assert(!rollover.update(true, UINT32_MAX - 20));
    assert(!rollover.update(true, 18)); // 39 ms across rollover.
    assert(rollover.update(true, 19));
    std::cout << "PASS: button bounce, holds, startup state and timer rollover\n";
}
