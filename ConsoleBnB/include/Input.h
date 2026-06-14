#pragma once

struct InputState
{
    bool quit = false;

    bool p1Up = false;
    bool p1Down = false;
    bool p1Left = false;
    bool p1Right = false;
    bool p1Bomb = false;
    bool p1Needle = false;

    bool p2Up = false;
    bool p2Down = false;
    bool p2Left = false;
    bool p2Right = false;
    bool p2Bomb = false;
    bool p2Needle = false;
};

class Input
{
public:
    InputState poll();

private:
    bool prevSpace_ = false;
    bool prevEnter_ = false;
    bool prevNeedle_ = false;
};
