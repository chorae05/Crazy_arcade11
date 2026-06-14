#pragma once

#include <vector>

constexpr int MAP_W = 17;
constexpr int MAP_H = 13;
constexpr int CELL_W = 3;
constexpr int FPS = 20;
constexpr int BOMB_FRAMES = 60;
constexpr int FLAME_FRAMES = 8;
constexpr int SPEED_FRAMES = 100;
constexpr int TRAP_FRAMES = 100;

struct Vec2
{
    int x = 0;
    int y = 0;

    bool operator==(const Vec2& other) const
    {
        return x == other.x && y == other.y;
    }
};

enum class Tile
{
    Empty,
    Wall,
    Block
};

enum class ItemType
{
    None,
    Speed,
    Range,
    BombCount,
    Needle
};

enum class GameResult
{
    Running,
    P1Win,
    P2Win,
    Draw,
    Quit
};

enum class MenuChoice
{
    Start,
    Help,
    Exit
};

struct Player
{
    int id = 0;
    char symbol = 'P';
    Vec2 pos;
    bool alive = true;
    int range = 2;
    int maxBombs = 1;
    int activeBombs = 0;
    int moveCooldown = 0;
    int speedFrames = 0;
    int needles = 0;
    bool trapped = false;
    int trappedFrames = 0;
};

struct Bomb
{
    Vec2 pos;
    int ownerId = 0;
    int timer = BOMB_FRAMES;
    int range = 2;
    bool exploded = false;
};

struct Flame
{
    std::vector<Vec2> cells;
    int timer = FLAME_FRAMES;
};
