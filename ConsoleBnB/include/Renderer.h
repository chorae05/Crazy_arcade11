#pragma once

#include "Map.h"
#include "Types.h"
#include <Windows.h>
#include <string>
#include <vector>

class Renderer
{
public:
    Renderer();

    void setupConsole();
    void hideCursor();
    void clearScreen();
    bool pumpMessages();
    bool windowClosed() const;
    bool consumeMenuClick(MenuChoice& choice);
    void drawMainMenu(MenuChoice selected);
    void drawHelp();
    void drawCountdown(int count);
    void draw(
        const GameMap& map,
        const Player& p1,
        const Player& p2,
        const std::vector<Bomb>& bombs,
        const std::vector<Flame>& flames,
        GameResult result);
    void drawGameOver(GameResult result);
    void drawResultScreen(GameResult result);

private:
    HANDLE out_;
    HWND hwnd_ = nullptr;
    int width_ = 1000;
    int height_ = 800;
    bool fullscreen_ = false;
    DWORD windowStyle_ = 0;
    WINDOWPLACEMENT windowPlacement_{ sizeof(WINDOWPLACEMENT) };
    bool closed_ = false;
    bool hasClick_ = false;
    POINT click_{};

    friend LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HDC beginPaintFrame(HDC& windowDc, HDC& memoryDc, HBITMAP& bitmap, HBITMAP& oldBitmap);
    void endPaintFrame(HDC windowDc, HDC memoryDc, HBITMAP bitmap, HBITMAP oldBitmap);
    void toggleFullscreen();
    void fillRect(HDC dc, int x, int y, int w, int h, COLORREF color);
    void roundRect(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border);
    bool bitmapText(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color);
    SIZE bitmapTextSize(const std::wstring& value, int size);
    void text(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color, bool bold = false);
    void centered(HDC dc, int y, const std::wstring& value, int size, COLORREF color, bool bold = false);
    void centeredInRect(HDC dc, RECT rect, const std::wstring& value, int size, COLORREF color, bool bold = false);
    RECT menuButtonRect(MenuChoice choice) const;
    void drawButton(HDC dc, MenuChoice choice, const std::wstring& label, bool selected);
    void drawGrass(HDC dc, int x, int y, int w, int h);
    void drawWall(HDC dc, int x, int y, int size);
    void drawCrate(HDC dc, int x, int y, int size);
    void drawBomb(HDC dc, int x, int y, int size);
    void drawFlame(HDC dc, int x, int y, int size);
    void drawPlayer(HDC dc, int x, int y, int size, const Player& player, COLORREF color);
    void drawUncleFace(HDC dc, int x, int y, int size);
    void drawSimpleFace(HDC dc, int x, int y, int size, COLORREF frameColor);
    void drawItem(HDC dc, int x, int y, int size, ItemType item);
    bool hasBombAt(Vec2 p, const std::vector<Bomb>& bombs) const;
    bool hasFlameAt(Vec2 p, const std::vector<Flame>& flames) const;
};
