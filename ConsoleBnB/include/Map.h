#pragma once

#include "Types.h"
#include <array>
#include <random>

class GameMap
{
public:
    GameMap();

    void reset();
    Tile tileAt(Vec2 p) const;
    ItemType itemAt(Vec2 p) const;
    void setTile(Vec2 p, Tile tile);
    void setItem(Vec2 p, ItemType item);
    bool inBounds(Vec2 p) const;
    bool isWalkable(Vec2 p) const;
    bool breakBlock(Vec2 p, std::mt19937& rng);

private:
    std::array<std::array<Tile, MAP_W>, MAP_H> tiles_{};
    std::array<std::array<ItemType, MAP_W>, MAP_H> items_{};

    void clearStartArea(Vec2 start);
};
