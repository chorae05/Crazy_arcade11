#pragma once

#include "Input.h"
#include "Map.h"
#include "Renderer.h"
#include "Types.h"
#include <random>
#include <vector>

class Game
{
public:
    Game();
    int run();

private:
    GameMap map_;
    Renderer renderer_;
    Input input_;
    Player p1_;
    Player p2_;
    std::vector<Bomb> bombs_;
    std::vector<Flame> flames_;
    std::mt19937 rng_;
    GameResult result_ = GameResult::Running;
    MenuChoice menuChoice_ = MenuChoice::Start;

    void reset();
    bool runMenu();
    void showHelp();
    void playCountdown();
    bool showResultScreen();
    void update(const InputState& input);
    void updatePlayer(Player& player, const InputState& input);
    void tryMove(Player& player, Vec2 delta);
    void tryPlaceBomb(Player& player);
    void updateBombs();
    void explodeBomb(size_t index);
    void updateFlames();
    void applyItems(Player& player);
    void checkDeaths();
    void checkResult();
    void soundMenu() const;
    void soundStart() const;
    void soundBomb() const;
    void soundExplosion() const;
    void soundItem() const;
    void soundDeath() const;
    bool blockedByBomb(Vec2 p) const;
    bool isOnFlame(Vec2 p) const;
};
