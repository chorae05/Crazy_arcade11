#include "../include/Input.h"

#include <Windows.h>
#include <conio.h>

namespace
{
bool isDown(int virtualKey)
{
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}
}

InputState Input::poll()
{
    while (_kbhit())
    {
        (void)_getch();
    }

    InputState state;
    state.quit = isDown(VK_ESCAPE);

    state.p1Up = isDown('W');
    state.p1Down = isDown('S');
    state.p1Left = isDown('A');
    state.p1Right = isDown('D');

    state.p2Up = isDown(VK_UP);
    state.p2Down = isDown(VK_DOWN);
    state.p2Left = isDown(VK_LEFT);
    state.p2Right = isDown(VK_RIGHT);

    const bool space = isDown(VK_SPACE);
    const bool enter = isDown(VK_SHIFT) || isDown(VK_CONTROL);
    const bool needle = isDown('N');
    state.p1Bomb = enter && !prevEnter_;
    state.p2Bomb = space && !prevSpace_;
    state.p1Needle = needle && !prevNeedle_;
    state.p2Needle = needle && !prevNeedle_;
    prevSpace_ = space;
    prevEnter_ = enter;
    prevNeedle_ = needle;

    return state;
}
