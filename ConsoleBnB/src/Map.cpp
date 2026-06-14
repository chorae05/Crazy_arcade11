#include "../include/Map.h"

GameMap::GameMap()
{
    reset();
}

void GameMap::reset()
{
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            items_[y][x] = ItemType::None;

            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1)
            {
                tiles_[y][x] = Tile::Wall;
            }
            else if (x % 2 == 0 && y % 2 == 0)
            {
                tiles_[y][x] = Tile::Wall;
            }
            else
            {
                tiles_[y][x] = ((x * 13 + y * 7 + x * y) % 5 == 0) ? Tile::Empty : Tile::Block;
            }
        }
    }

    clearStartArea({ 1, 1 });
    clearStartArea({ MAP_W - 2, MAP_H - 2 });
}

Tile GameMap::tileAt(Vec2 p) const
{
    return inBounds(p) ? tiles_[p.y][p.x] : Tile::Wall;
}

ItemType GameMap::itemAt(Vec2 p) const
{
    return inBounds(p) ? items_[p.y][p.x] : ItemType::None;
}

void GameMap::setTile(Vec2 p, Tile tile)
{
    if (inBounds(p))
    {
        tiles_[p.y][p.x] = tile;
    }
}

void GameMap::setItem(Vec2 p, ItemType item)
{
    if (inBounds(p))
    {
        items_[p.y][p.x] = item;
    }
}

bool GameMap::inBounds(Vec2 p) const
{
    return p.x >= 0 && p.x < MAP_W && p.y >= 0 && p.y < MAP_H;
}

bool GameMap::isWalkable(Vec2 p) const
{
    return inBounds(p) && tileAt(p) == Tile::Empty;
}

bool GameMap::breakBlock(Vec2 p, std::mt19937& rng)
{
    if (!inBounds(p) || tileAt(p) != Tile::Block)
    {
        return false;
    }

    setTile(p, Tile::Empty);

    std::uniform_int_distribution<int> dropRoll(1, 100);
    if (dropRoll(rng) <= 30)
    {
        std::uniform_int_distribution<int> itemRoll(0, 3);
        const int item = itemRoll(rng);
        setItem(p,
            item == 0 ? ItemType::Speed :
            item == 1 ? ItemType::Range :
            item == 2 ? ItemType::BombCount :
            ItemType::Needle);
    }

    return true;
}

void GameMap::clearStartArea(Vec2 start)
{
    const Vec2 safeCells[] = {
        start,
        { start.x + (start.x == 1 ? 1 : -1), start.y },
        { start.x + (start.x == 1 ? 2 : -2), start.y },
        { start.x, start.y + (start.y == 1 ? 1 : -1) },
        { start.x, start.y + (start.y == 1 ? 2 : -2) },
        { start.x + (start.x == 1 ? 1 : -1), start.y + (start.y == 1 ? 1 : -1) }
    };

    for (Vec2 p : safeCells)
    {
        if (inBounds(p) && tileAt(p) != Tile::Wall)
        {
            setTile(p, Tile::Empty);
            setItem(p, ItemType::None);
        }
    }
}
