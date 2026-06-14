#include "../include/Game.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <thread>

namespace
{
int moveDelayFor(const Player& player)
{
    return player.speedFrames > 0 ? 4 : 7;
}

int keyMoveX(const InputState& input, int playerId)
{
    if (playerId == 1)
    {
        if (input.p1Left) return -1;
        if (input.p1Right) return 1;
    }
    else
    {
        if (input.p2Left) return -1;
        if (input.p2Right) return 1;
    }
    return 0;
}

int keyMoveY(const InputState& input, int playerId)
{
    if (playerId == 1)
    {
        if (input.p1Up) return -1;
        if (input.p1Down) return 1;
    }
    else
    {
        if (input.p2Up) return -1;
        if (input.p2Down) return 1;
    }
    return 0;
}
}

Game::Game()
    : rng_(std::random_device{}())
{
    reset();
}

int Game::run()
{
    renderer_.setupConsole();

    while (!renderer_.windowClosed())
    {
        menuChoice_ = MenuChoice::Start;
        if (!runMenu())
        {
            break;
        }

        reset();
        playCountdown();

        using clock = std::chrono::steady_clock;
        const auto frameTime = std::chrono::milliseconds(1000 / FPS);

        while (result_ == GameResult::Running)
        {
            const auto frameStart = clock::now();
            if (!renderer_.pumpMessages() || renderer_.windowClosed())
            {
                result_ = GameResult::Quit;
                break;
            }

            const InputState input = input_.poll();
            update(input);
            renderer_.draw(map_, p1_, p2_, bombs_, flames_, result_);

            const auto elapsed = clock::now() - frameStart;
            if (elapsed < frameTime)
            {
                std::this_thread::sleep_for(frameTime - elapsed);
            }
        }

        if (!showResultScreen())
        {
            break;
        }
    }

    return 0;
}

void Game::reset()
{
    map_.reset();
    bombs_.clear();
    flames_.clear();

    p1_ = Player{ 1, 'P', { 1, 1 }, true, 2, 3, 0, 0, 0, 0 };
    p2_ = Player{ 2, 'Q', { MAP_W - 2, MAP_H - 2 }, true, 2, 3, 0, 0, 0, 0 };
    result_ = GameResult::Running;
}

bool Game::runMenu()
{
    bool prevUp = (GetAsyncKeyState(VK_UP) & 0x8000) != 0 || (GetAsyncKeyState('W') & 0x8000) != 0;
    bool prevDown = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 || (GetAsyncKeyState('S') & 0x8000) != 0;
    bool prevEnter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    bool prevEsc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

    while (true)
    {
        if (!renderer_.pumpMessages() || renderer_.windowClosed())
        {
            return false;
        }

        renderer_.drawMainMenu(menuChoice_);
        MenuChoice clickedChoice = menuChoice_;
        const bool clicked = renderer_.consumeMenuClick(clickedChoice);
        if (clicked)
        {
            menuChoice_ = clickedChoice;
        }

        const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0 || (GetAsyncKeyState('W') & 0x8000) != 0;
        const bool down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 || (GetAsyncKeyState('S') & 0x8000) != 0;
        const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if (up && !prevUp)
        {
            menuChoice_ = menuChoice_ == MenuChoice::Start ? MenuChoice::Exit :
                (menuChoice_ == MenuChoice::Help ? MenuChoice::Start : MenuChoice::Help);
            soundMenu();
        }
        if (down && !prevDown)
        {
            menuChoice_ = menuChoice_ == MenuChoice::Start ? MenuChoice::Help :
                (menuChoice_ == MenuChoice::Help ? MenuChoice::Exit : MenuChoice::Start);
            soundMenu();
        }
        if ((enter && !prevEnter) || clicked)
        {
            soundStart();
            if (menuChoice_ == MenuChoice::Start)
            {
                return true;
            }
            if (menuChoice_ == MenuChoice::Help)
            {
                showHelp();
            }
            if (menuChoice_ == MenuChoice::Exit)
            {
                return false;
            }
        }
        if (esc && !prevEsc)
        {
            return false;
        }

        prevUp = up;
        prevDown = down;
        prevEnter = enter;
        prevEsc = esc;
        Sleep(70);
    }
}

void Game::showHelp()
{
    bool prevEnter = true;
    bool prevEsc = false;
    renderer_.drawHelp();

    while (true)
    {
        if (!renderer_.pumpMessages() || renderer_.windowClosed())
        {
            return;
        }

        const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if ((enter && !prevEnter) || (esc && !prevEsc))
        {
            soundMenu();
            return;
        }

        prevEnter = enter;
        prevEsc = esc;
        Sleep(70);
    }
}

void Game::playCountdown()
{
    for (int count = 3; count >= 0; --count)
    {
        renderer_.drawCountdown(count);
        Beep(count > 0 ? 620 : 920, count > 0 ? 140 : 230);
        Sleep(count > 0 ? 720 : 520);
    }
}

bool Game::showResultScreen()
{
    bool prevEnter = true;
    bool prevEsc = true;

    while (!renderer_.windowClosed())
    {
        renderer_.pumpMessages();
        renderer_.drawResultScreen(result_);

        const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if ((enter && !prevEnter) || (esc && !prevEsc))
        {
            return enter && !prevEnter;
        }

        prevEnter = enter;
        prevEsc = esc;
        Sleep(50);
    }

    return false;
}

void Game::update(const InputState& input)
{
    if (input.quit)
    {
        result_ = GameResult::Quit;
        return;
    }

    updatePlayer(p1_, input);
    updatePlayer(p2_, input);
    updateBombs();
    checkDeaths();
    updateFlames();
    checkResult();
}

void Game::updatePlayer(Player& player, const InputState& input)
{
    if (!player.alive)
    {
        return;
    }

    const bool placeBomb = (player.id == 1) ? input.p1Bomb : input.p2Bomb;
    const bool useNeedle = (player.id == 1) ? input.p1Needle : input.p2Needle;

    if (player.trapped)
    {
        if (useNeedle && player.needles > 0)
        {
            --player.needles;
            player.trapped = false;
            player.trappedFrames = 0;
            soundItem();
            return;
        }

        if (player.trappedFrames > 0)
        {
            --player.trappedFrames;
        }
        if (player.trappedFrames <= 0)
        {
            player.alive = false;
            soundDeath();
        }
        return;
    }

    if (player.moveCooldown > 0)
    {
        --player.moveCooldown;
    }
    if (player.speedFrames > 0)
    {
        --player.speedFrames;
    }

    if (placeBomb)
    {
        tryPlaceBomb(player);
    }

    if (player.moveCooldown == 0)
    {
        Vec2 delta{ keyMoveX(input, player.id), keyMoveY(input, player.id) };
        if (delta.x != 0)
        {
            delta.y = 0;
        }

        if (delta.x != 0 || delta.y != 0)
        {
            tryMove(player, delta);
            player.moveCooldown = moveDelayFor(player);
        }
    }
}

void Game::tryMove(Player& player, Vec2 delta)
{
    const Vec2 next{ player.pos.x + delta.x, player.pos.y + delta.y };
    Player& other = player.id == 1 ? p2_ : p1_;

    if (other.alive && other.pos == next)
    {
        if (other.trapped)
        {
            other.alive = false;
            other.trapped = false;
            soundDeath();
        }
        return;
    }

    if (!map_.isWalkable(next) || blockedByBomb(next))
    {
        return;
    }

    player.pos = next;
    applyItems(player);
}

void Game::tryPlaceBomb(Player& player)
{
    if (player.activeBombs >= player.maxBombs || blockedByBomb(player.pos))
    {
        return;
    }

    bombs_.push_back(Bomb{ player.pos, player.id, BOMB_FRAMES, player.range, false });
    ++player.activeBombs;
    soundBomb();
}

void Game::updateBombs()
{
    for (Bomb& bomb : bombs_)
    {
        if (!bomb.exploded)
        {
            --bomb.timer;
        }
    }

    bool explodedAny = true;
    while (explodedAny)
    {
        explodedAny = false;
        for (size_t i = 0; i < bombs_.size(); ++i)
        {
            if (!bombs_[i].exploded && bombs_[i].timer <= 0)
            {
                explodeBomb(i);
                explodedAny = true;
            }
        }
    }

    bombs_.erase(
        std::remove_if(
            bombs_.begin(),
            bombs_.end(),
            [](const Bomb& bomb) { return bomb.exploded; }),
        bombs_.end());
}

void Game::explodeBomb(size_t index)
{
    if (index >= bombs_.size() || bombs_[index].exploded)
    {
        return;
    }

    Bomb& source = bombs_[index];
    source.exploded = true;

    Player& owner = source.ownerId == 1 ? p1_ : p2_;
    owner.activeBombs = std::max(0, owner.activeBombs - 1);

    Flame flame;
    flame.cells.push_back(source.pos);

    const Vec2 directions[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (Vec2 dir : directions)
    {
        for (int step = 1; step <= source.range; ++step)
        {
            const Vec2 p{ source.pos.x + dir.x * step, source.pos.y + dir.y * step };
            const Tile tile = map_.tileAt(p);

            if (tile == Tile::Wall)
            {
                break;
            }

            flame.cells.push_back(p);

            for (Bomb& bomb : bombs_)
            {
                if (!bomb.exploded && bomb.pos == p)
                {
                    bomb.timer = 0;
                }
            }

            if (tile == Tile::Block)
            {
                map_.breakBlock(p, rng_);
                break;
            }
        }
    }

    flames_.push_back(flame);
    soundExplosion();
}

void Game::updateFlames()
{
    for (Flame& flame : flames_)
    {
        --flame.timer;
    }

    flames_.erase(
        std::remove_if(
            flames_.begin(),
            flames_.end(),
            [](const Flame& flame) { return flame.timer <= 0; }),
        flames_.end());
}

void Game::applyItems(Player& player)
{
    const ItemType item = map_.itemAt(player.pos);
    if (item == ItemType::None)
    {
        return;
    }

    switch (item)
    {
    case ItemType::Speed:
        player.speedFrames = SPEED_FRAMES;
        break;
    case ItemType::Range:
        player.range = std::min(8, player.range + 1);
        break;
    case ItemType::BombCount:
        player.maxBombs = std::min(8, player.maxBombs + 1);
        break;
    case ItemType::Needle:
        player.needles = std::min(3, player.needles + 1);
        break;
    case ItemType::None:
        break;
    }

    map_.setItem(player.pos, ItemType::None);
    soundItem();
}

void Game::checkDeaths()
{
    if (p1_.alive && !p1_.trapped && isOnFlame(p1_.pos))
    {
        p1_.trapped = true;
        p1_.trappedFrames = TRAP_FRAMES;
        soundDeath();
    }
    if (p2_.alive && !p2_.trapped && isOnFlame(p2_.pos))
    {
        p2_.trapped = true;
        p2_.trappedFrames = TRAP_FRAMES;
        soundDeath();
    }
}

void Game::checkResult()
{
    if (!p1_.alive && !p2_.alive)
    {
        result_ = GameResult::Draw;
    }
    else if (!p1_.alive)
    {
        result_ = GameResult::P2Win;
    }
    else if (!p2_.alive)
    {
        result_ = GameResult::P1Win;
    }
}

void Game::soundMenu() const
{
    Beep(520, 35);
}

void Game::soundStart() const
{
    Beep(740, 80);
}

void Game::soundBomb() const
{
    Beep(430, 55);
}

void Game::soundExplosion() const
{
    Beep(160, 90);
    Beep(220, 45);
}

void Game::soundItem() const
{
    Beep(880, 55);
}

void Game::soundDeath() const
{
    Beep(260, 120);
    Beep(180, 140);
}

bool Game::blockedByBomb(Vec2 p) const
{
    for (const Bomb& bomb : bombs_)
    {
        if (!bomb.exploded && bomb.pos == p)
        {
            return true;
        }
    }
    return false;
}

bool Game::isOnFlame(Vec2 p) const
{
    for (const Flame& flame : flames_)
    {
        for (Vec2 cell : flame.cells)
        {
            if (cell == p)
            {
                return true;
            }
        }
    }
    return false;
}
