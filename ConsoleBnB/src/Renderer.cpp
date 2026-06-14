#include "../include/Renderer.h"

#include <algorithm>
#include <gdiplus.h>
#include <iostream>
#include <memory>
#include <windowsx.h>

#pragma comment(lib, "gdiplus.lib")

namespace
{
    constexpr COLORREF Ink = RGB(73, 58, 58);
    constexpr COLORREF Outline = RGB(91, 70, 74);
    constexpr COLORREF Sky = RGB(142, 211, 232);
    constexpr COLORREF Cream = RGB(255, 242, 214);
    constexpr COLORREF MintA = RGB(167, 223, 166);
    constexpr COLORREF MintB = RGB(190, 235, 184);
    constexpr COLORREF TileDot = RGB(238, 248, 205);
    constexpr COLORREF WallBlue = RGB(79, 173, 232);
    constexpr COLORREF WallYellow = RGB(246, 211, 101);
    constexpr COLORREF WallRed = RGB(239, 111, 108);
    constexpr COLORREF WallShadow = RGB(107, 128, 158);
    constexpr COLORREF Crate = RGB(184, 120, 62);
    constexpr COLORREF CrateLight = RGB(232, 166, 90);
    constexpr COLORREF CrateDark = RGB(121, 79, 53);
    constexpr COLORREF Water = RGB(47, 155, 255);
    constexpr COLORREF WaterLight = RGB(110, 220, 255);
    constexpr COLORREF White = RGB(247, 255, 255);
    constexpr COLORREF Black = RGB(33, 37, 45);
    constexpr COLORREF Pink = RGB(239, 116, 210);
    constexpr COLORREF Yellow = RGB(246, 211, 101);
    constexpr COLORREF Red = RGB(239, 111, 108);
    constexpr COLORREF Panel = RGB(255, 242, 214);
    constexpr COLORREF SoftBlue = RGB(190, 236, 247);
    constexpr COLORREF Lavender = RGB(201, 167, 255);
    constexpr int Cell = 34;
    constexpr int BoardX = 211;
    constexpr int BoardY = 172;
    constexpr int BoardW = MAP_W * Cell;
    constexpr int BoardH = MAP_H * Cell;

    Renderer* g_renderer = nullptr;

    std::wstring directoryOf(const std::wstring& path)
    {
        const size_t slash = path.find_last_of(L"\\/");
        return slash == std::wstring::npos ? L"." : path.substr(0, slash);
    }

    bool fileExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    class BitmapFontAtlas
    {
    public:
        bool load()
        {
            if (ready_)
            {
                return atlas_ != nullptr;
            }
            ready_ = true;

            Gdiplus::GdiplusStartupInput input{};
            if (Gdiplus::GdiplusStartup(&token_, &input, nullptr) != Gdiplus::Ok)
            {
                token_ = 0;
                return false;
            }

            // 실행파일 옆에 font_korean.png 로드
            wchar_t exePath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring dir = directoryOf(exePath);
            std::wstring pngPath = dir + L"\\font_korean.png";

            atlas_.reset(Gdiplus::Bitmap::FromFile(pngPath.c_str()));
            if (!atlas_ || atlas_->GetLastStatus() != Gdiplus::Ok)
            {
                atlas_.reset();
                return false;
            }

            // PNG 실제 크기로 rows 계산
            rows_ = static_cast<int>(atlas_->GetHeight()) / glyphHeight_;
            return true;
        }

        SIZE measure(const std::wstring& value, int size)
        {
            int lineWidth = 0;
            int maxWidth = 0;
            int lines = 1;

            for (wchar_t ch : value)
            {
                if (ch == L'\n')
                {
                    maxWidth = std::max(maxWidth, lineWidth);
                    lineWidth = 0;
                    ++lines;
                    continue;
                }
                lineWidth += advanceFor(ch, size);
            }

            maxWidth = std::max(maxWidth, lineWidth);
            return SIZE{ maxWidth, lines * lineHeightFor(size) };
        }

        bool draw(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color)
        {
            if (!load() || atlas_ == nullptr || size <= 0)
            {
                return false;
            }

            Gdiplus::Graphics graphics(dc);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            const float red = GetRValue(color) / 255.0f;
            const float green = GetGValue(color) / 255.0f;
            const float blue = GetBValue(color) / 255.0f;

            Gdiplus::ColorMatrix tint =
            {
                red,  0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, green,0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, blue, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };

            Gdiplus::ImageAttributes attributes;
            attributes.SetColorMatrix(&tint, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

            int cursorX = x;
            int cursorY = y;

            for (wchar_t ch : value)
            {
                if (ch == L'\n')
                {
                    cursorX = x;
                    cursorY += lineHeightFor(size);
                    continue;
                }
                if (ch == L' ')
                {
                    cursorX += advanceFor(ch, size);
                    continue;
                }

                const int glyphIndex = glyphIndexFor(ch);
                const int cellCount = columns_ * rows_;
                if (glyphIndex < 0 || glyphIndex >= cellCount)
                {
                    cursorX += advanceFor(ch, size);
                    continue;
                }

                const int drawWidth = drawWidthFor(ch, size);
                const int sourceX = (glyphIndex % columns_) * glyphWidth_;
                const int sourceY = (glyphIndex / columns_) * glyphHeight_;

                const Gdiplus::RectF dest(
                    static_cast<Gdiplus::REAL>(cursorX),
                    static_cast<Gdiplus::REAL>(cursorY),
                    static_cast<Gdiplus::REAL>(drawWidth),
                    static_cast<Gdiplus::REAL>(size));

                graphics.DrawImage(
                    atlas_.get(),
                    dest,
                    static_cast<Gdiplus::REAL>(sourceX),
                    static_cast<Gdiplus::REAL>(sourceY),
                    static_cast<Gdiplus::REAL>(glyphWidth_),
                    static_cast<Gdiplus::REAL>(glyphHeight_),
                    Gdiplus::UnitPixel,
                    &attributes);

                cursorX += advanceFor(ch, size);
            }

            return true;
        }

    private:
        int glyphIndexFor(wchar_t ch) const
        {
            // ASCII: 32~126 → index 0~94
            if (ch >= 32 && ch <= 126)
            {
                return static_cast<int>(ch) - 32;
            }

            // 한글
            const size_t found = koreanGlyphs_.find(ch);
            if (found != std::wstring::npos)
            {
                return asciiGlyphCount_ + static_cast<int>(found);
            }

            // 자모
            const size_t jamoFound = jamoGlyphs_.find(ch);
            if (jamoFound != std::wstring::npos)
            {
                return asciiGlyphCount_ + static_cast<int>(koreanGlyphs_.size()) + static_cast<int>(jamoFound);
            }

            // 없는 글자 → '?' 출력
            return static_cast<int>(L'?') - 32;
        }

        static int advanceFor(wchar_t ch, int size)
        {
            if (ch == L' ')
            {
                return std::max(1, size / 2);
            }
            if (ch >= 32 && ch <= 126)
            {
                return std::max(1, (size * 6) / 10);
            }
            return size;
        }

        static int drawWidthFor(wchar_t ch, int size)
        {
            (void)ch;
            return size;
        }

        static int lineHeightFor(int size)
        {
            return size + std::max(3, size / 5);
        }

        ULONG_PTR token_ = 0;
        bool ready_ = false;
        std::unique_ptr<Gdiplus::Bitmap> atlas_;

        // font_korean.png 기준: CELL=36, COLS=16
        int glyphWidth_ = 36;
        int glyphHeight_ = 36;
        int columns_ = 16;
        int rows_ = 21; // PNG 로드 후 자동 갱신됨

        static constexpr int asciiGlyphCount_ = 95; // 32~126

        // PNG 생성 시 사용한 것과 완전히 동일한 순서여야 함!
        const std::wstring koreanGlyphs_ =
            L"가각갇개거검게겨경고공과광구귀그기나난날내너노누늘니"
            L"다대도돌동두드든디라락랑래러려력로록료루르른를리마막말"
            L"맵면명모무물미바박반발방배번법보복본부분불블비빠사상색"
            L"서선설세속수순쉬스승시십아안알애야어언얼업에엔여연영오"
            L"요우운움워원위유으은을음의이인일자작잠장저전정제조종좌"
            L"주줄중즈증지진쪽차착참창처천체초최추출치카커컨코크키타"
            L"탄탈터템토통트틱파판팁폭풍프플피하한할합해험화후히힘"
            L"갑뉴는또란릭메별생셀양와용임져존준집침클퍼픽향홍";

        const std::wstring jamoGlyphs_ = L"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ";
    };

    BitmapFontAtlas g_fontAtlas;

    bool pointInRect(POINT p, RECT r)
    {
        return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
    }
}

LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_renderer != nullptr)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN:
            g_renderer->hasClick_ = true;
            g_renderer->click_ = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_F10)
            {
                g_renderer->toggleFullscreen();
                return 0;
            }
            break;
        case WM_CLOSE:
            g_renderer->closed_ = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_renderer->closed_ = true;
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

Renderer::Renderer()
    : out_(GetStdHandle(STD_OUTPUT_HANDLE))
{
}

void Renderer::setupConsole()
{
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    SetConsoleTitleW(L"픽셀 물풍선 대전");
    hideCursor();
    std::wcout << L"[LOG] The game runs in the graphic window.\n";

    g_renderer = this;
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW wc{};
    wc.lpfnWndProc = GameWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"ConsoleBnBGameWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&wc);

    RECT rect{ 0, 0, width_, height_ };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    hwnd_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"픽셀 물풍선 대전",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr, nullptr, instance, nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    windowStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
}

void Renderer::hideCursor()
{
    CONSOLE_CURSOR_INFO info{};
    info.dwSize = 1;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(out_, &info);
}

void Renderer::clearScreen()
{
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
    fillRect(dc, 0, 0, width_, height_, Black);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

bool Renderer::pumpMessages()
{
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !closed_;
}

bool Renderer::windowClosed() const
{
    return closed_;
}

bool Renderer::consumeMenuClick(MenuChoice& choice)
{
    if (!hasClick_)
    {
        return false;
    }

    const POINT p = click_;
    hasClick_ = false;

    if (pointInRect(p, menuButtonRect(MenuChoice::Start)))
    {
        choice = MenuChoice::Start;
        return true;
    }
    if (pointInRect(p, menuButtonRect(MenuChoice::Help)))
    {
        choice = MenuChoice::Help;
        return true;
    }
    if (pointInRect(p, menuButtonRect(MenuChoice::Exit)))
    {
        choice = MenuChoice::Exit;
        return true;
    }
    return false;
}

void Renderer::drawMainMenu(MenuChoice selected)
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, SoftBlue);
    drawGrass(dc, 0, 0, width_, height_);
    roundRect(dc, 104, 50, 792, 648, 10, Cream, Outline);

    fillRect(dc, 132, 78, 736, 142, MintB);
    fillRect(dc, 132, 220, 736, 8, Outline);
    drawUncleFace(dc, 162, 112, 54);
    drawSimpleFace(dc, 780, 110, 58, Red);
    drawBomb(dc, 252, 120, 42);
    drawBomb(dc, 706, 122, 42);

    centered(dc, 92, L"픽셀", 38, WallBlue, true);
    centered(dc, 134, L"물풍선 대전", 50, Red, true);
    centered(dc, 192, L"귀여운 폭탄 게임", 22, Ink, true);

    roundRect(dc, 242, 246, 516, 98, 8, RGB(255, 248, 232), Outline);
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 11; ++col)
        {
            const int tx = 274 + col * 42;
            const int ty = 260 + row * 24;
            if ((row + col) % 3 == 0)       drawWall(dc, tx, ty, 24);
            else if ((row + col) % 3 == 1)  drawCrate(dc, tx, ty, 24);
            else                             drawGrass(dc, tx, ty, 24, 24);
        }
    }
    drawUncleFace(dc, 302, 284, 34);
    drawSimpleFace(dc, 654, 282, 34, Red);

    drawButton(dc, MenuChoice::Start, L"시작", selected == MenuChoice::Start);
    drawButton(dc, MenuChoice::Help, L"도움말", selected == MenuChoice::Help);
    drawButton(dc, MenuChoice::Exit, L"나가기", selected == MenuChoice::Exit);

    centered(dc, 595, L"클릭 또는 방향키와 엔터", 24, Ink, true);
    centered(dc, 630, L"1P 이동: WASD (폭탄: Shift/Ctrl)  |  2P 이동: 방향키 (폭탄: Space)", 20, Ink, true);
    centered(dc, 660, L"F10: 전체화면  |  ESC: 게임 종료", 20, Ink, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

void Renderer::drawHelp()
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, Sky);
    drawGrass(dc, 0, 0, width_, height_);
    roundRect(dc, 76, 54, 848, 642, 12, Panel, Outline);
    centered(dc, 86, L"게임 방법", 42, Ink, true);

    text(dc, 112, 150, L"1. 물풍선은 잠시 후 터집니다.", 24, Red, true);
    text(dc, 112, 188, L"물줄기는 십자 모양으로 퍼져요.", 21, Ink);
    text(dc, 112, 220, L"갇히면 바늘로 탈출하세요.", 21, Ink);

    text(dc, 112, 280, L"2. 조작법", 24, WallBlue, true);
    text(dc, 112, 318, L"1P 이동: W A S D        폭탄: Shift 또는 Ctrl", 21, Ink);
    text(dc, 112, 350, L"2P 이동: 방향키          폭탄: Space", 21, Ink);
    text(dc, 112, 382, L"N: 바늘 사용     F10: 전체화면     ESC: 종료", 21, Ink);

    text(dc, 112, 438, L"3. 아이템", 24, Pink, true);
    text(dc, 112, 476, L"파란별: 폭탄 개수 증가     분홍 플러스: 사거리 증가", 21, Ink);
    text(dc, 112, 508, L"초록번개: 속도 증가        노란바늘: 바늘 보유 증가", 21, Ink);

    centered(dc, 636, L"엔터 또는 ESC를 누르면 돌아갑니다", 24, Red, true);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

void Renderer::drawCountdown(int count)
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, Sky);
    drawGrass(dc, 0, 0, width_, height_);
    centered(dc, 212, L"준비", 58, Ink, true);
    centered(dc, 310, count > 0 ? std::to_wstring(count) : L"시작", 108, Yellow, true);
    centered(dc, 506, L"물풍선 대전 시작", 32, Ink, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

void Renderer::draw(
    const GameMap& map,
    const Player& p1,
    const Player& p2,
    const std::vector<Bomb>& bombs,
    const std::vector<Flame>& flames,
    GameResult result)
{
    (void)result;
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, SoftBlue);
    drawGrass(dc, 0, 0, width_, height_);

    roundRect(dc, 118, 34, 764, 94, 8, Panel, Outline);
    fillRect(dc, 145, 58, 56, 48, WallBlue);
    drawUncleFace(dc, 153, 62, 40);
    text(dc, 214, 56, L"1P", 24, WallBlue, true);
    text(dc, 214, 86,
        L"폭탄 " + std::to_wstring(p1.maxBombs) +
        L"  사거리 " + std::to_wstring(p1.range) +
        L"  바늘 " + std::to_wstring(p1.needles), 18, Ink, true);

    roundRect(dc, 423, 54, 154, 56, 8, White, Outline);
    centered(dc, 66, L"대전", 28, Red, true);

    drawSimpleFace(dc, 800, 58, 48, Red);
    text(dc, 710, 56, L"2P", 24, Red, true);
    text(dc, 590, 86,
        L"폭탄 " + std::to_wstring(p2.maxBombs) +
        L"  사거리 " + std::to_wstring(p2.range) +
        L"  바늘 " + std::to_wstring(p2.needles), 18, Ink, true);

    roundRect(dc, BoardX - 18, BoardY - 18, BoardW + 36, BoardH + 36, 8, Cream, Outline);
    fillRect(dc, BoardX - 8, BoardY - 8, BoardW + 16, BoardH + 16, Outline);
    fillRect(dc, BoardX, BoardY, BoardW, BoardH, MintA);

    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            const Vec2 pos{ x, y };
            const int sx = BoardX + x * Cell;
            const int sy = BoardY + y * Cell;
            drawGrass(dc, sx, sy, Cell, Cell);

            if (hasFlameAt(pos, flames))
            {
                drawFlame(dc, sx, sy, Cell);
            }
            else
            {
                switch (map.tileAt(pos))
                {
                case Tile::Wall:  drawWall(dc, sx, sy, Cell);  break;
                case Tile::Block: drawCrate(dc, sx, sy, Cell); break;
                case Tile::Empty: drawItem(dc, sx, sy, Cell, map.itemAt(pos)); break;
                }
            }

            if (hasBombAt(pos, bombs))
            {
                drawBomb(dc, sx, sy, Cell);
            }
        }
    }

    if (p1.alive) drawPlayer(dc, BoardX + p1.pos.x * Cell, BoardY + p1.pos.y * Cell, Cell, p1, RGB(79, 173, 232));
    if (p2.alive) drawPlayer(dc, BoardX + p2.pos.x * Cell, BoardY + p2.pos.y * Cell, Cell, p2, Red);

    const int uiY = BoardY + BoardH + 42;
    roundRect(dc, 174, uiY, 652, 54, 8, Panel, Outline);
    text(dc, 204, uiY + 15, L"1P " + std::wstring(p1.alive ? (p1.trapped ? L"갇힘" : L"생존") : L"탈락"), 19, WallBlue, true);
    text(dc, 342, uiY + 15, L"WASD / Shift", 18, Ink, true);
    text(dc, 508, uiY + 15, L"방향키 / Space", 18, Ink, true);
    text(dc, 690, uiY + 15, L"2P " + std::wstring(p2.alive ? (p2.trapped ? L"갇힘" : L"생존") : L"탈락"), 19, Red, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

void Renderer::drawGameOver(GameResult result)
{
    drawResultScreen(result);
}

void Renderer::drawResultScreen(GameResult result)
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, Sky);
    drawGrass(dc, 0, 0, width_, height_);
    roundRect(dc, 120, 166, 760, 390, 12, Cream, Outline);

    std::wstring message = L"무승부";
    if (result == GameResult::P1Win) message = L"1P 승리";
    else if (result == GameResult::P2Win) message = L"2P 승리";
    else if (result == GameResult::Quit)  message = L"게임 종료";

    centered(dc, 252, message, 78, Yellow, true);
    centered(dc, 370, L"엔터: 메인 메뉴", 32, Ink, true);
    centered(dc, 426, L"ESC: 종료", 26, Red, true);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

HDC Renderer::beginPaintFrame(HDC& windowDc, HDC& memoryDc, HBITMAP& bitmap, HBITMAP& oldBitmap)
{
    windowDc = GetDC(hwnd_);
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int clientW = client.right - client.left;
    const int clientH = client.bottom - client.top;
    memoryDc = CreateCompatibleDC(windowDc);
    bitmap = CreateCompatibleBitmap(windowDc, clientW, clientH);
    oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));
    SetBkMode(memoryDc, TRANSPARENT);
    fillRect(memoryDc, 0, 0, clientW, clientH, Black);

    const int offsetX = std::max(0, (clientW - width_) / 2);
    const int offsetY = std::max(0, (clientH - height_) / 2);
    SetViewportOrgEx(memoryDc, offsetX, offsetY, nullptr);
    return memoryDc;
}

void Renderer::endPaintFrame(HDC windowDc, HDC memoryDc, HBITMAP bitmap, HBITMAP oldBitmap)
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    BitBlt(windowDc, 0, 0, client.right - client.left, client.bottom - client.top, memoryDc, 0, 0, SRCCOPY);
    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd_, windowDc);
}

void Renderer::toggleFullscreen()
{
    if (hwnd_ == nullptr) return;

    if (!fullscreen_)
    {
        windowStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
        GetWindowPlacement(hwnd_, &windowPlacement_);

        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
        GetMonitorInfoW(monitor, &monitorInfo);

        SetWindowLongPtrW(hwnd_, GWL_STYLE, windowStyle_ & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd_, HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen_ = true;
    }
    else
    {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, windowStyle_);
        SetWindowPlacement(hwnd_, &windowPlacement_);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        fullscreen_ = false;
    }
}

void Renderer::fillRect(HDC dc, int x, int y, int w, int h, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    RECT r{ x, y, x + w, y + h };
    FillRect(dc, &r, brush);
    DeleteObject(brush);
}

void Renderer::roundRect(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN   pen = CreatePen(PS_SOLID, 4, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

bool Renderer::bitmapText(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color)
{
    return g_fontAtlas.draw(dc, x, y, value, size, color);
}

SIZE Renderer::bitmapTextSize(const std::wstring& value, int size)
{
    return g_fontAtlas.measure(value, size);
}

void Renderer::text(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        bitmapText(dc, x, y, value, size, color);
        return;
    }

    // PNG 로드 실패 시 폴백: GDI 폰트
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, value.c_str(), static_cast<int>(value.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void Renderer::centered(HDC dc, int y, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        const SIZE s = bitmapTextSize(value, size);
        const int x = (width_ - s.cx) / 2;
        bitmapText(dc, x, y, value, size, color);
        return;
    }

    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE s{};
    GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &s);
    SetTextColor(dc, color);
    TextOutW(dc, (width_ - s.cx) / 2, y, value.c_str(), static_cast<int>(value.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void Renderer::centeredInRect(HDC dc, RECT rect, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        const SIZE s = bitmapTextSize(value, size);
        const int x = rect.left + ((rect.right - rect.left) - s.cx) / 2;
        const int y = rect.top + ((rect.bottom - rect.top) - s.cy) / 2;
        bitmapText(dc, x, y, value, size, color);
        return;
    }

    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE s{};
    GetTextExtentPoint32W(dc, value.c_str(), static_cast<int>(value.size()), &s);
    SetTextColor(dc, color);
    TextOutW(dc,
        rect.left + ((rect.right - rect.left) - s.cx) / 2,
        rect.top + ((rect.bottom - rect.top) - s.cy) / 2,
        value.c_str(), static_cast<int>(value.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

RECT Renderer::menuButtonRect(MenuChoice choice) const
{
    int y = 378;
    if (choice == MenuChoice::Help) y = 458;
    else if (choice == MenuChoice::Exit) y = 538;
    return RECT{ 342, y, 658, y + 58 };
}

void Renderer::drawButton(HDC dc, MenuChoice choice, const std::wstring& label, bool selected)
{
    RECT r = menuButtonRect(choice);
    const COLORREF fill = selected ? Yellow : White;
    const COLORREF border = selected ? Red : Outline;
    roundRect(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, 8, fill, border);
    fillRect(dc, r.left + 10, r.top + 10, 14, 14, selected ? Red : WallBlue);
    fillRect(dc, r.left + 26, r.top + 10, 14, 14, selected ? White : Yellow);
    fillRect(dc, r.right - 40, r.bottom - 24, 14, 14, selected ? White : Yellow);
    fillRect(dc, r.right - 24, r.bottom - 24, 14, 14, selected ? Red : WallBlue);
    centeredInRect(dc, r, label, 34, Ink, true);
}

void Renderer::drawGrass(HDC dc, int x, int y, int w, int h)
{
    fillRect(dc, x, y, w, h, MintA);
    const int tile = Cell;
    for (int yy = y; yy < y + h; yy += tile)
    {
        for (int xx = x; xx < x + w; xx += tile)
        {
            fillRect(dc, xx, yy, tile, tile,
                (((xx / tile) + (yy / tile)) % 2 == 0) ? MintA : MintB);
            fillRect(dc, xx + 5, yy + 5, 3, 3, TileDot);
            fillRect(dc, xx + tile - 9, yy + tile - 9, 3, 3, TileDot);
        }
    }
    if (w == Cell && h == Cell)
    {
        fillRect(dc, x, y, w, 1, RGB(145, 202, 159));
        fillRect(dc, x, y, 1, h, RGB(145, 202, 159));
    }
}

void Renderer::drawWall(HDC dc, int x, int y, int size)
{
    const COLORREF fill =
        (((x / Cell) + (y / Cell)) % 3 == 0) ? WallBlue :
        (((x / Cell) + (y / Cell)) % 3 == 1) ? WallYellow :
        WallRed;
    fillRect(dc, x + 3, y + 4, size - 6, size - 7, Outline);
    fillRect(dc, x + 5, y + 2, size - 10, size - 8, Outline);
    fillRect(dc, x + 6, y + 5, size - 12, size - 13, fill);
    fillRect(dc, x + 9, y + 8, 6, 5, White);
    fillRect(dc, x + size - 14, y + 8, 5, 5, White);
    fillRect(dc, x + 9, y + size - 13, size - 18, 5, WallShadow);
    fillRect(dc, x + 14, y + 16, 5, 5, Outline);
    fillRect(dc, x + size - 19, y + 16, 5, 5, Outline);
}

void Renderer::drawCrate(HDC dc, int x, int y, int size)
{
    fillRect(dc, x + 4, y + 4, size - 8, size - 8, CrateDark);
    fillRect(dc, x + 7, y + 7, size - 14, size - 14, Crate);
    fillRect(dc, x + 10, y + 10, size - 20, 4, CrateLight);
    fillRect(dc, x + 10, y + size - 14, size - 20, 4, CrateDark);
    fillRect(dc, x + 9, y + 9, 4, size - 18, CrateDark);
    fillRect(dc, x + size - 13, y + 9, 4, size - 18, CrateDark);
    for (int i = 0; i < size - 18; i += 5)
    {
        fillRect(dc, x + 10 + i, y + 10 + i, 4, 4, CrateDark);
        fillRect(dc, x + size - 14 - i, y + 10 + i, 4, 4, CrateDark);
    }
}

void Renderer::drawBomb(HDC dc, int x, int y, int size)
{
    fillRect(dc, x + 11, y + 11, size - 22, size - 16, Outline);
    fillRect(dc, x + 8, y + 15, size - 16, size - 21, Outline);
    fillRect(dc, x + 12, y + 13, size - 24, size - 19, Water);
    fillRect(dc, x + 10, y + 17, size - 20, size - 25, Water);
    fillRect(dc, x + 15, y + 15, 7, 6, WaterLight);
    fillRect(dc, x + 23, y + 9, 6, 5, Outline);
    fillRect(dc, x + 27, y + 7, 4, 4, Yellow);
}

void Renderer::drawFlame(HDC dc, int x, int y, int size)
{
    fillRect(dc, x + 2, y + 13, size - 4, 10, Outline);
    fillRect(dc, x + 13, y + 2, 10, size - 4, Outline);
    fillRect(dc, x + 3, y + 14, size - 6, 8, Water);
    fillRect(dc, x + 14, y + 3, 8, size - 6, Water);
    fillRect(dc, x + 7, y + 16, size - 14, 4, WaterLight);
    fillRect(dc, x + 16, y + 7, 4, size - 14, WaterLight);
    fillRect(dc, x + 14, y + 14, 8, 8, White);
}

void Renderer::drawPlayer(HDC dc, int x, int y, int size, const Player& player, COLORREF color)
{
    if (player.trapped)
    {
        drawBomb(dc, x, y, size);
    }

    if (player.id == 1)
    {
        drawUncleFace(dc, x + 2, y + 1, size - 4);
    }
    else
    {
        drawSimpleFace(dc, x + 1, y + 1, size - 2, color);
    }
}

void Renderer::drawUncleFace(HDC dc, int x, int y, int size)
{
    const COLORREF skin = RGB(248, 202, 158);
    const COLORREF hair = RGB(18, 18, 16);
    const COLORREF shadow = RGB(221, 164, 123);
    const int u = std::max(2, size / 16);

    const int faceX = x + 3 * u;
    const int faceY = y + 7 * u;
    const int faceW = 10 * u;
    const int faceH = 7 * u;

    fillRect(dc, faceX, faceY, faceW, faceH, skin);
    fillRect(dc, faceX, faceY + faceH, faceW, u, shadow);

    fillRect(dc, x + 2 * u, y + 5 * u, 12 * u, 3 * u, hair);
    fillRect(dc, x + 1 * u, y + 6 * u, 3 * u, 2 * u, hair);
    fillRect(dc, x + 12 * u, y + 6 * u, 3 * u, 2 * u, hair);
    fillRect(dc, x + 4 * u, y + 3 * u, 8 * u, 3 * u, hair);
    fillRect(dc, x + 6 * u, y + 2 * u, 4 * u, 2 * u, hair);
    fillRect(dc, x + 8 * u, y + 4 * u, u, 4 * u, skin);

    fillRect(dc, x + 3 * u, y + 8 * u, 4 * u, 3 * u, Black);
    fillRect(dc, x + 9 * u, y + 8 * u, 4 * u, 3 * u, Black);
    fillRect(dc, x + 4 * u, y + 9 * u, 2 * u, u, White);
    fillRect(dc, x + 10 * u, y + 9 * u, 2 * u, u, White);
    fillRect(dc, x + 7 * u, y + 9 * u, 2 * u, u, Black);

    fillRect(dc, faceX - u, faceY + faceH, faceW + 2 * u, u, Outline);
}

void Renderer::drawSimpleFace(HDC dc, int x, int y, int size, COLORREF frameColor)
{
    const int pad = std::max(4, size / 6);
    const int eye = std::max(3, size / 11);
    const int inner = size - 2 * pad;

    fillRect(dc, x, y, size, size, frameColor);
    fillRect(dc, x + pad, y + pad, inner, inner, RGB(255, 241, 222));

    fillRect(dc, x + pad + inner / 3 - eye / 2, y + pad + inner / 2 - eye / 2, eye, eye, Black);
    fillRect(dc, x + pad + (inner * 2) / 3 - eye / 2, y + pad + inner / 2 - eye / 2, eye, eye, Black);
}

void Renderer::drawItem(HDC dc, int x, int y, int size, ItemType item)
{
    if (item == ItemType::None) return;

    const int cx = x + size / 2;
    const int cy = y + size / 2;

    fillRect(dc, x + 8, y + 8, size - 16, size - 16, White);
    fillRect(dc, x + 7, y + 9, 2, size - 18, Outline);
    fillRect(dc, x + size - 9, y + 9, 2, size - 18, Outline);
    fillRect(dc, x + 9, y + 7, size - 18, 2, Outline);
    fillRect(dc, x + 9, y + size - 9, size - 18, 2, Outline);

    if (item == ItemType::BombCount)
    {
        const COLORREF blue = RGB(47, 155, 255);
        fillRect(dc, cx - 2, cy - 12, 4, 5, blue);
        fillRect(dc, cx - 4, cy - 7, 8, 4, blue);
        fillRect(dc, cx - 12, cy - 4, 24, 5, blue);
        fillRect(dc, cx - 9, cy + 1, 18, 4, blue);
        fillRect(dc, cx - 7, cy + 5, 5, 7, blue);
        fillRect(dc, cx + 2, cy + 5, 5, 7, blue);
        fillRect(dc, cx - 14, cy - 2, 5, 5, blue);
        fillRect(dc, cx + 9, cy - 2, 5, 5, blue);
        fillRect(dc, cx - 2, cy - 5, 4, 4, WaterLight);
    }
    else if (item == ItemType::Range)
    {
        fillRect(dc, cx - 4, cy - 12, 8, 24, Pink);
        fillRect(dc, cx - 12, cy - 4, 24, 8, Pink);
        fillRect(dc, cx - 2, cy - 10, 4, 20, RGB(255, 190, 232));
        fillRect(dc, cx - 10, cy - 2, 20, 4, RGB(255, 190, 232));
    }
    else if (item == ItemType::Speed)
    {
        const COLORREF green = RGB(88, 222, 95);
        fillRect(dc, cx - 1, cy - 13, 9, 5, green);
        fillRect(dc, cx - 5, cy - 8, 11, 5, green);
        fillRect(dc, cx - 9, cy - 3, 13, 5, green);
        fillRect(dc, cx - 4, cy + 2, 12, 5, green);
        fillRect(dc, cx - 8, cy + 7, 8, 5, green);
        fillRect(dc, cx - 11, cy + 12, 5, 3, green);
        fillRect(dc, cx + 2, cy - 10, 4, 4, RGB(207, 255, 196));
    }
    else if (item == ItemType::Needle)
    {
        fillRect(dc, x + 8, y + 8, size - 16, size - 16, Yellow);
        fillRect(dc, x + 7, y + 9, 2, size - 18, Outline);
        fillRect(dc, x + size - 9, y + 9, 2, size - 18, Outline);
        fillRect(dc, x + 9, y + 7, size - 18, 2, Outline);
        fillRect(dc, x + 9, y + size - 9, size - 18, 2, Outline);
        text(dc, x + 8, y + 8, L"침", 20, Ink, true);
    }
}

bool Renderer::hasBombAt(Vec2 p, const std::vector<Bomb>& bombs) const
{
    for (const Bomb& bomb : bombs)
    {
        if (!bomb.exploded && bomb.pos == p) return true;
    }
    return false;
}

bool Renderer::hasFlameAt(Vec2 p, const std::vector<Flame>& flames) const
{
    for (const Flame& flame : flames)
    {
        for (Vec2 cell : flame.cells)
        {
            if (cell == p) return true;
        }
    }
    return false;
}
