#include "../STM32CubeIDE/ProjectCode/ButtonPressFilter.hpp"
#include <assert.h>

int main() {
    ButtonPressFilter start;
    ButtonPressFilter stop;
    assert(start.Update(true, 0));
    assert(!start.Update(true, 3000));
    assert(!start.Update(false, 3100));
    assert(!start.Update(true, 4999));
    assert(!start.Update(true, 5000)); // Held ignored press does not become an event.
    assert(!start.Update(false, 5001));
    assert(start.Update(true, 5002)); // Ignored press did not extend lockout.
    assert(stop.Update(true, 5003)); // STOP has an independent interval.
    assert(!stop.Update(true, 11000)); // Holding never repeats.
    assert(!stop.Update(false, 11001));
    assert(stop.Update(true, 11002));

    ButtonPressFilter boundary;
    assert(boundary.Update(true, 100));
    assert(!boundary.Update(false, 101));
    assert(boundary.Update(true, 5100));

    ButtonPressFilter rollover;
    assert(rollover.Update(true, UINT32_MAX - 999u));
    assert(!rollover.Update(false, UINT32_MAX));
    assert(!rollover.Update(true, 3999));
    assert(!rollover.Update(false, 3999));
    assert(rollover.Update(true, 4000));
}
