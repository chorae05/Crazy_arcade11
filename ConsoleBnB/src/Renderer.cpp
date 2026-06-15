#include "../include/Renderer.h"

#include <algorithm>
#include <gdiplus.h>
#include <iostream>
#include <memory>
#include <windowsx.h>

// GDI+ 그래픽 라이브러리를 사용하기 위해 윈도우 정적 링커에게 라이브러리를 포함하라고 지시하는 명령어 규격이야.
#pragma comment(lib, "gdiplus.lib")

// 익명 네임스페이스: 이 파일(.cpp) 내부에서만 사용하는 비밀 도화지 설정값과 전용 폰트 부품을 정의하는 곳이야.
namespace
{
    // --- 레트로 감성 도트 디자인용 완벽 색상표 (물감 정의) ---
    constexpr COLORREF Ink = RGB(73, 58, 58);          // 메인 글씨 및 어두운 잉크색
    constexpr COLORREF Outline = RGB(91, 70, 74);      // 모든 오브젝트의 갈색 테두리선 선 두께용
    constexpr COLORREF Sky = RGB(142, 211, 232);          // 로비나 도움말의 시원한 하늘색 배경
    constexpr COLORREF Cream = RGB(255, 242, 214);        // 메인 패널의 부드러운 크림색 바닥
    constexpr COLORREF MintA = RGB(167, 223, 166);        // 체크무늬 잔디밭 격자 A타입 초록색
    constexpr COLORREF MintB = RGB(190, 235, 184);        // 체크무늬 잔디밭 격자 B타입 연초록색
    constexpr COLORREF TileDot = RGB(238, 248, 205);      // 잔디밭 위에 박히는 귀여운 도트 점 색상
    constexpr COLORREF WallBlue = RGB(79, 173, 232);      // 짝수 칸에 박히는 안 부서지는 기둥 벽 파란색
    constexpr COLORREF WallYellow = RGB(246, 211, 101);    // 기둥 벽 노란색 변체
    constexpr COLORREF WallRed = RGB(239, 111, 108);       // 기둥 벽 빨간색 변체
    constexpr COLORREF WallShadow = RGB(107, 128, 158);    // 벽 아래 깔리는 짙은 그림자 색
    constexpr COLORREF Crate = RGB(184, 120, 62);         // 부서지는 나무 상자의 메인 브라운 색상
    constexpr COLORREF CrateLight = RGB(232, 166, 90);    // 나무 상자 모서리의 밝은 하이라이트 색
    constexpr COLORREF CrateDark = RGB(121, 79, 53);      // 나무 상자의 깊은 음영 색
    constexpr COLORREF Water = RGB(47, 155, 255);         // 물풍선 및 물줄기 불꽃의 파란색 물줄기 기본 물감
    constexpr COLORREF WaterLight = RGB(110, 220, 255);    // 물줄기 정중앙의 번쩍이는 하늘색 코어 효과음
    constexpr COLORREF White = RGB(247, 255, 255);        // 쨍한 화이트 색상
    constexpr COLORREF Black = RGB(33, 37, 45);           // GDI 폴백 배경용 어두운 암전색
    constexpr COLORREF Pink = RGB(239, 116, 210);         // 물풍선 사거리 증가 아이템용 분홍색
    constexpr COLORREF Yellow = RGB(246, 211, 101);       // 카운트다운 숫자 및 승리 문구용 메인 골드 옐로우
    constexpr COLORREF Red = RGB(239, 111, 108);          // 2P 팀 컬러 및 경고 문구용 강렬한 레드
    constexpr COLORREF Panel = RGB(255, 242, 214);        // 상단 스탯 인터페이스 창의 패널 바탕색
    constexpr COLORREF SoftBlue = RGB(190, 236, 247);     // 전체 로비 월페이퍼 연하늘색
    constexpr COLORREF Lavender = RGB(201, 167, 255);     // 특수 이펙트용 연보라색

    // --- 전장 스케일 및 좌표 수학 상수 매칭 ---
    constexpr int Cell = 34;                           // 게임판 한 칸(타일)의 정밀 크기 (34x34 픽셀 도트)
    constexpr int BoardX = 211;                        // 1000x800 화면 내에서 인게임 바둑판이 시작되는 좌측 x 절대좌표
    constexpr int BoardY = 172;                        // 1000x800 화면 내에서 인게임 바둑판이 시작되는 상단 y 절대좌표
    constexpr int BoardW = MAP_W * Cell;               // 전장 바둑판의 총 가로 크기 (17칸 * 34픽셀 = 578 픽셀)
    constexpr int BoardH = MAP_H * Cell;               // 전장 바둑판의 총 세로 크기 (13칸 * 34픽셀 = 442 픽셀)

    Renderer* g_renderer = nullptr;                    // WndProc 글로벌 콜백 안테나가 조율할 수 있게 주소를 넘겨두는 전역 포인터야.

    // 파일 경로 문자열에서 실행파일이 위치한 "순수 디렉토리 폴더 주소"만 칼같이 오려내 주는 파싱 함수야.
    std::wstring directoryOf(const std::wstring& path)
    {
        const size_t slash = path.find_last_of(L"\\/");
        return slash == std::wstring::npos ? L"." : path.substr(0, slash);
    }

    // 지정한 에셋 경로에 진짜로 파일이 실존하는지 윈도우 파일 시스템에 물어보고 체크하는 안전 함수야.
    bool fileExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    // =================================================================
    // 폰트 에셋 통합 가위질 렌더링 서브시스템 (내부 클래스)
    // =================================================================
    // font_korean.png 글자 모음집 사진 한 장을 통째로 들고 있으면서,
    // 원하는 문자열이 들어오면 실시간으로 자소 주소를 연산해 낱글자로 오려 그리는 엔진의 핵심 눈이야!
    class BitmapFontAtlas
    {
    public:
        // [초기화 펑션] 실행파일(`.exe`) 바로 옆에 있는 'font_korean.png' 파일을 찾아 GDI+ 메모리에 안착시켜.
        bool load()
        {
            if (ready_)
            {
                return atlas_ != nullptr; // 이미 로드가 한 번 끝났다면 중복 작업을 피하기 위해 패스해.
            }
            ready_ = true;

            // GDI+ 그래픽 엔진 스타트업 초기화 과정 신호를 전송해.
            Gdiplus::GdiplusStartupInput input{};
            if (Gdiplus::GdiplusStartup(&token_, &input, nullptr) != Gdiplus::Ok)
            {
                token_ = 0;
                return false;
            }

            // 실행파일 절대경로 추출 연산
            wchar_t exePath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring dir = directoryOf(exePath);
            std::wstring pngPath = dir + L"\\font_korean.png"; // 최종 에셋 패스 결합

            // 파일로부터 비트맵 이미지를 복사해 메모리에 상주시켜.
            atlas_.reset(Gdiplus::Bitmap::FromFile(pngPath.c_str()));
            if (!atlas_ || atlas_->GetLastStatus() != Gdiplus::Ok)
            {
                atlas_.reset(); // 로드 실패 시 메모리 누수를 막기 위해 안전하게 소멸시켜버려.
                return false;
            }

            // 이미지 실제 세로 픽셀 크기를 한 글자 높이(36픽셀)로 나눠서 이 그림책이 총 몇 줄짜리인지 자동 연산해.
            rows_ = static_cast<int>(atlas_->GetHeight()) / glyphHeight_;
            return true;
        }

        // 특정 문자열을 출력하기 전, "그 글씨들이 가로·세로로 총 몇 픽셀을 차지하는지" 미리 길이를 재주는 자(Scale) 함수야.
        SIZE measure(const std::wstring& value, int size)
        {
            int lineWidth = 0;
            int maxWidth = 0;
            int lines = 1;

            for (wchar_t ch : value)
            {
                if (ch == L'\n') // 개행 문자를 만나면 다음 줄로 토스하고 가로 길이를 리셋해.
                {
                    maxWidth = std::max(maxWidth, lineWidth);
                    lineWidth = 0;
                    ++lines;
                    continue;
                }
                lineWidth += advanceFor(ch, size); // 자소별 폰트 가로 폭을 계속 더해 나가.
            }

            maxWidth = std::max(maxWidth, lineWidth);
            return SIZE{ maxWidth, lines * lineHeightFor(size) }; // 최종 픽셀 면적(가로, 세로)을 리턴해.
        }

        // [문자열 오려 그리기 마스터 함수] 글자 사진 첩에서 필요한 픽셀을 고속으로 오려 붙이고 원하는 색상으로 틴팅(염색)까지 정산해 줘.
        bool draw(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color)
        {
            if (!load() || atlas_ == nullptr || size <= 0)
            {
                return false; // 에셋 맵 로드에 실패했다면 버그 방지를 위해 즉시 드로잉을 거부해.
            }

            Gdiplus::Graphics graphics(dc);
            // 안티앨리어싱 필터를 끄고 '가장 가까운 이웃 보간법'을 적용해 도트 배율이 커져도 뿌옇게 번지지 않고 칼같이 픽셀 엣지가 살아나게 조율해!
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

            // COLORREF 물감 정보를 GDI+가 이해할 수 있는 0.0 ~ 1.0f 비율의 RGB 채널 데이터로 변환해.
            const float red = GetRValue(color) / 255.0f;
            const float green = GetGValue(color) / 255.0f;
            const float blue = GetBValue(color) / 255.0f;

            // [핵심 컬러 매트릭스 기술] 흰색/검은색 폰트 아틀라스 이미지에 내가 원하는 색상 물감을 픽셀별로 고속 곱셈 연산하여 실시간 염색(Tinting)을 먹이는 행렬 필터야!
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
                if (ch == L'\n') // 줄바꿈 문자 정산
                {
                    cursorX = x;
                    cursorY += lineHeightFor(size);
                    continue;
                }
                if (ch == L' ') // 공백 문자일 땐 그리기 점프하고 띄어쓰기 폭만큼 커서만 밀어줘.
                {
                    cursorX += advanceFor(ch, size);
                    continue;
                }

                // 현재 글자가 글자책 장부 이미지의 몇 번째 격자 인덱스 칸에 있는지 수식 주소를 가져와.
                const int glyphIndex = glyphIndexFor(ch);
                const int cellCount = columns_ * rows_;
                if (glyphIndex < 0 || glyphIndex >= cellCount)
                {
                    cursorX += advanceFor(ch, size); // 없는 글자 예외 처리
                    continue;
                }

                // 아틀라스 이미지 격자 상의 정밀 x, y 원본 픽셀 좌표를 수학적으로 오려내(Crop)는 가위질 좌표 연산이야!
                const int drawWidth = drawWidthFor(ch, size);
                const int sourceX = (glyphIndex % columns_) * glyphWidth_;
                const int sourceY = (glyphIndex / columns_) * glyphHeight_;

                // 화면에 그려질 목적지 픽셀 범위 사각형 설정
                const Gdiplus::RectF dest(
                    static_cast<Gdiplus::REAL>(cursorX),
                    static_cast<Gdiplus::REAL>(cursorY),
                    static_cast<Gdiplus::REAL>(drawWidth),
                    static_cast<Gdiplus::REAL>(size));

                // Gdiplus 고속 스프라이트 렌더링 실행! 원본 주소(sourceX, sourceY)에서 36x36만큼 오려서 화면 목적지(dest)에 쾅!
                graphics.DrawImage(
                    atlas_.get(),
                    dest,
                    static_cast<Gdiplus::REAL>(sourceX),
                    static_cast<Gdiplus::REAL>(sourceY),
                    static_cast<Gdiplus::REAL>(glyphWidth_),
                    static_cast<Gdiplus::REAL>(glyphHeight_),
                    Gdiplus::UnitPixel,
                    &attributes);

                // 글자 하나를 다 그렸으니 다음 글자를 배치하기 위해 문자 가로 폭만큼 옆으로 커서를 전진시켜.
                cursorX += advanceFor(ch, size);
            }

            return true;
        }

    private:
        // 들어온 유니코드 문자(한글, 알파벳)를 아틀라스 이미지 폰트책의 바둑판 '일련번호 격자 인덱스 번호'로 해시 변환하는 매퍼 함수야.
        int glyphIndexFor(wchar_t ch) const
        {
            // ASCII 영역: 키보드 특수문자 및 영문자(32번 스페이스부터 126번 ~까지) → 인덱스 0번~94번 매칭
            if (ch >= 32 && ch <= 126)
            {
                return static_cast<int>(ch) - 32;
            }

            // 완성형 한글 영역 장부 매칭 연산
            const size_t found = koreanGlyphs_.find(ch);
            if (found != std::wstring::npos)
            {
                return asciiGlyphCount_ + static_cast<int>(found);
            }

            // ㄱ, ㄴ, ㄷ 같은 단일 자모음 영역 매칭 연산
            const size_t jamoFound = jamoGlyphs_.find(ch);
            if (jamoFound != std::wstring::npos)
            {
                return asciiGlyphCount_ + static_cast<int>(koreanGlyphs_.size()) + static_cast<int>(jamoFound);
            }

            // 내 텍스트 지도책에 아예 존재하지 않는 글자(외계어)가 들어오면 기본 에러 대체 문자인 '?' 자소 위치를 리턴해버려.
            return static_cast<int>(L'?') - 32;
        }

        // 글자 크기에 맞춰 글자 간의 가로 자간 띄어쓰기 폭 폭(Advance)을 비례적으로 계산해 주는 폰트 정산 장치야.
        static int advanceFor(wchar_t ch, int size)
        {
            if (ch == L' ')
            {
                return std::max(1, size / 2); // 스페이스 바는 반 칸만 띄워.
            }
            if (ch >= 32 && ch <= 126)
            {
                return std::max(1, (size * 6) / 10); // 영문이나 특수문자는 가로 폭이 좁으므로 60% 슬림 자간 비율을 적용해.
            }
            return size; // 뚱뚱한 일반 한글은 정사각형 100% 비율 자간을 적용해.
        }

        static int drawWidthFor(wchar_t ch, int size)
        {
            (void)ch;
            return size; // 기본 텍스트 스케일 비율 동기화
        }

        // 줄바꿈 시 아래 줄과 겹치지 않게 가독성용 미세 여백(20%)을 추가한 행간 간격 산출 함수야.
        static int lineHeightFor(int size)
        {
            return size + std::max(3, size / 5);
        }

        ULONG_PTR token_ = 0;
        bool ready_ = false;
        std::unique_ptr<Gdiplus::Bitmap> atlas_;

        // 아틀라스 낱글자 격자 정보 규격
        int glyphWidth_ = 36;
        int glyphHeight_ = 36;
        int columns_ = 16;
        int rows_ = 21; // 이미지 로드 직후 세로줄 개수는 자동 재계산됨

        static constexpr int asciiGlyphCount_ = 95;

        // [폰트 맵 리스트 장부] png 파일 안에 그려진 글자 배치 도면 순서와 자 하나 안 틀리고 200% 일치해야 정밀 오려내기가 작동해!
        const std::wstring koreanGlyphs_ =
            L"가각갇개거검게겨경고공과광구귀그기나난날내너노누늘니"
            L"다대도돌동두드든디라락랑래러려력로록료루르른를리마막말"
            L"맵면명모무물미바박반발방배번법보복본부분불블비빠사상색"
            L"서선설세속수순쉬스승시십아안알애야어언얼업에엔여연영오"
            L"요우운움워원위유으은을음의이인일자작잠장저전정제조종좌"
            L"주줄중즈증지진쪽차착참창처천체초최추출치카커컨코크키타"
            L"탄탈터템토통트틱파판팁폭풍프플피하한할합해험화후히힘"
            L"갑뉴는또란릭메별생셀양와용임져존준집침클퍼픽향홍";

        const std::wstring jamoGlyphs_ = L"ㄱ%ㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ";
    };

    BitmapFontAtlas g_fontAtlas; // 글로벌 비트맵 폰트 매니저 싱글톤 인스턴스화

    // 마우스 마우스 클릭 좌표(p)가 특정 UI 버튼 사각형 영역 박스(r) 안쪽에 겹쳐 눌렸는지 검사하는 수학 충돌 센서 함수야.
    bool pointInRect(POINT p, RECT r)
    {
        return p.x >= r.left && p.x <= r.right && p.y >= r.top && p.y <= r.bottom;
    }
}

// =================================================================
// 2. Win32 OS 핵심 메시지 루프 안테나 (글로벌 콜백)
// =================================================================
// 윈도우 운영체제가 쏴주는 하드웨어 신호를 수신해 렌더러 내부 변수로 안전 전이시키는 인터럽트 다리 함수야.
LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_renderer != nullptr)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN: // 사용자가 마우스 왼쪽 버튼을 딸깍 클릭했다면!
            g_renderer->hasClick_ = true;
            // lParam 32비트 메모리 주소 공간에서 가위질 비트 연산 연산으로 마우스 x, y 마우스 픽셀 좌표를 정밀 파싱해 와!
            g_renderer->click_ = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            return 0;

        case WM_CLOSE: // 사용자가 우측 상단 윈도우 창 X 버튼을 눌렀다면!
            g_renderer->closed_ = true;
            DestroyWindow(hwnd); // 창 파괴 프로세스 촉발
            return 0;
        case WM_DESTROY: // 창 파괴가 완료되어 메모리 해제 단계라면!
            g_renderer->closed_ = true;
            PostQuitMessage(0); // 메인 프로세스 완전 종료 메시지를 대기열에 삽입해.
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam); // 내가 처리 안 한 기타 메시지는 윈도우 기본 디폴트 처리기에 일임해.
}

Renderer::Renderer()
    : out_(GetStdHandle(STD_OUTPUT_HANDLE)) // 표준 콘솔 핸들 바인딩
{
}

// [창 초기화 세팅 마스터 함수] 도트 대전용 윈도우 창 클래스를 OS에 등록하고 실물 가상 창을 띄워주는 초기 정산 함수야.
void Renderer::setupConsole()
{
    // 콘솔 문자 코드 페이지를 UTF-8 유니코드 코드 규격(65001)으로 세팅해 한글 깨짐을 방어해.
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    SetConsoleTitleW(L"픽셀 물풍선 대전");
    hideCursor();
    std::wcout << L"[LOG] The game runs in the graphic window.\n";

    g_renderer = this; // 안테나가 나를 조종할 수 있게 전역 포인터 변수에 내 주소 주입
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    // 윈도우 스타일 및 클래스 속성 메모리 조립
    WNDCLASSW wc{};
    wc.lpfnWndProc = GameWndProc; // 내 커스텀 안테나 연결!
    wc.hInstance = instance;
    wc.lpszClassName = L"ConsoleBnBGameWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); // 기본 배경 붓 블랙 지정
    RegisterClassW(&wc);

    // [중요 수학 공식] 우리가 원하는 순수 도화지 알맹이 크기가 1000x800이 되도록, 외곽 타이틀 바와 테두리 두께 픽셀을 추가 계산해 전체 창 외곽 스케일을 보정해 주는 Win32 필수 API야.
    RECT rect{ 0, 0, width_, height_ };
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    // 실물 GDI 그래픽 윈도우 창 생성 오더 발주!
    hwnd_ = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"픽셀 물풍선 대전",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr, nullptr, instance, nullptr);

    ShowWindow(hwnd_, SW_SHOW); // 화면에 보이게 활성화 투사
    UpdateWindow(hwnd_);
}

void Renderer::hideCursor()
{
    CONSOLE_CURSOR_INFO info{};
    info.dwSize = 1;
    info.bVisible = FALSE; // 깜빡이는 터미널 커서 투명화 숨김 처리
    SetConsoleCursorInfo(out_, &info);
}

void Renderer::clearScreen()
{
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    // 더블 버퍼링 백버퍼 캔버스를 쥐고 검은색 물감으로 화면을 싹 지워 리셋해.
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
    fillRect(dc, 0, 0, width_, height_, Black);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

// OS 윈도우 큐에 밀려 들어오는 마우스 클릭, 드래그 등의 가상 메시지 통신을 순회해 싹 비워내고 렌더링 동력원을 동기화하는 펌프 함수야.
bool Renderer::pumpMessages()
{
    MSG msg{};
    // 메시지 큐에 밀린 편지가 있다면 대기하지 말고 즉시 꺼내서(`PM_REMOVE`) 안테나(`WndProc`)로 초고속 배달 송신을 보낸단다.
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);  // 가상 키보드 메시지 조합 서포트
        DispatchMessageW(&msg);  // 안테나 함수로 최종 신호 슛!
    }
    return !closed_; // 창이 꺼졌다면 false를 뱉어 메인 고정 루프를 탈출시켜 줘.
}

bool Renderer::windowClosed() const
{
    return closed_;
}

// 메인 타이틀 창에서 마우스 클릭 신호가 들어왔을 때, 버튼 영역 픽셀과 충돌했는지 검사 후 어떤 메뉴를 골랐는지 리턴해 주는 스마트 소비 함수야.
bool Renderer::consumeMenuClick(MenuChoice& choice)
{
    if (!hasClick_)
    {
        return false; // 마우스 클릭 사건 자체가 없었다면 일찌감치 퇴근해.
    }

    const POINT p = click_;
    hasClick_ = false; // 클릭 사건 접수 완료했으니 중복 처리 방지를 위해 플래그 소거

    // 각 메뉴 버튼의 사각형 영역 범위(RECT) 안에 마우스 포인터 좌표(p)가 충돌했는지 정밀 판정해.
    if (pointInRect(p, menuButtonRect(MenuChoice::Start)))
    {
        choice = MenuChoice::Start; // 게임 시작 당첨!
        return true;
    }
    if (pointInRect(p, menuButtonRect(MenuChoice::Help)))
    {
        choice = MenuChoice::Help;  // 도움말 창 열기 당첨!
        return true;
    }
    if (pointInRect(p, menuButtonRect(MenuChoice::Exit)))
    {
        choice = MenuChoice::Exit;  // 나가기 종료 당첨!
        return true;
    }
    return false;
}

// =================================================================
// 3. 종합 UI 및 씬(Scene) 테마 렌더링 대장 함수들
// =================================================================

// [로비 렌더링 대장] 메인 타이틀 화면의 아기자기한 레트로 도트 배경과 메뉴 UI 단추들을 조각하는 함수야.
void Renderer::drawMainMenu(MenuChoice selected)
{
    pumpMessages(); // 밀린 클릭 신호 정산 상주
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap); // 비밀 백버퍼 가동!

    // 배경 작화 작업 시작
    fillRect(dc, 0, 0, width_, height_, SoftBlue);
    drawGrass(dc, 0, 0, width_, height_); // 체크무늬 잔디 바닥 도배
    roundRect(dc, 104, 50, 792, 648, 10, Cream, Outline); // 중앙 메인 보드판 사각형 거치

    // 타이틀 로고 박스 조각
    fillRect(dc, 132, 78, 736, 142, MintB);
    fillRect(dc, 132, 220, 736, 8, Outline); // 로고 아래 짙은 테두리 가로선 박기
    drawUncleFace(dc, 162, 112, 54);         // 장식용 1P 아저씨 얼굴 박기
    drawSimpleFace(dc, 780, 110, 58, Red);    // 장식용 2P 빨강이 얼굴 박기
    drawBomb(dc, 252, 120, 42);               // 장식용 물풍선 투척
    drawBomb(dc, 706, 122, 42);

    // 커스텀 비트맵 폰트 아틀라스를 이용해 웅장한 도트 타이틀 로고 글씨 출력!
    centered(dc, 92, L"픽셀", 38, WallBlue, true);
    centered(dc, 134, L"물풍선 대전", 50, Red, true);
    centered(dc, 192, L"귀여운 폭탄 게임", 22, Ink, true);

    // 중앙 로비 전용 미니 맵 디스플레이 장식장 조각
    roundRect(dc, 242, 246, 516, 98, 8, RGB(255, 248, 232), Outline);
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 11; ++col)
        {
            const int tx = 274 + col * 42;
            const int ty = 260 + row * 24;
            // 모듈러 공식을 응용해 미니 장식장 바닥에 벽 기둥과 상자, 잔디를 미니어처 스타일로 교차 스케치해!
            if ((row + col) % 3 == 0)       drawWall(dc, tx, ty, 24);
            else if ((row + col) % 3 == 1)  drawCrate(dc, tx, ty, 24);
            else                            drawGrass(dc, tx, ty, 24, 24);
        }
    }
    drawUncleFace(dc, 302, 284, 34);
    drawSimpleFace(dc, 654, 282, 34, Red);

    // 사용자의 메뉴 조작 상태(selected)에 맞춰 하이라이트 색상 물감을 묻혀 삼대 단추 렌더링!
    drawButton(dc, MenuChoice::Start, L"시작", selected == MenuChoice::Start);
    drawButton(dc, MenuChoice::Help, L"도움말", selected == MenuChoice::Help);
    drawButton(dc, MenuChoice::Exit, L"나가기", selected == MenuChoice::Exit);

    // 하단 조작 가이드 설명서 안내 텍스트 출력
    centered(dc, 610, L"클릭 또는 방향키와 엔터", 24, Ink, true);
    centered(dc, 650, L"1P 이동: WASD (폭탄: Shift/Ctrl)  |  2P 이동: 방향키 (폭탄: Space)", 20, Ink, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap); // 모니터에 완성품 긴급 송출!
}

// [도움말 렌더링 대장] 키보드 단축키 정보와 크레이지 아케이드 4대 보물 아이템 성능 요약본을 그려주는 설명서 창이야.
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

    // 조작법 텍스트 한 줄씩 지정 픽셀 좌표에 레이아웃 출력
    text(dc, 112, 150, L"1. 물풍선은 잠시 후 터집니다.", 24, Red, true);
    text(dc, 112, 188, L"물줄기는 십자 모양으로 퍼져요.", 21, Ink);
    text(dc, 112, 220, L"갇히면 바늘로 탈출하세요.", 21, Ink);

    text(dc, 112, 280, L"2. 조작법", 24, WallBlue, true);
    text(dc, 112, 318, L"1P 이동: W A S D       폭탄: Shift 또는 Ctrl", 21, Ink);
    text(dc, 112, 350, L"2P 이동: 방향키         폭탄: Space", 21, Ink);
    text(dc, 112, 382, L"N: 바늘 사용               ESC: 종료", 21, Ink);

    text(dc, 112, 438, L"3. 아이템", 24, Pink, true);
    text(dc, 112, 476, L"파란별: 폭탄 개수 증가   분홍 플러스: 사거리 증가", 21, Ink);
    text(dc, 112, 508, L"초록번개: 속도 증가        노란바늘: 바늘 보유 증가", 21, Ink);

    centered(dc, 636, L"엔터 또는 ESC를 누르면 돌아갑니다", 24, Red, true);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

// [카운트다운 렌더링 대장] 인게임 매치 진입 직전, 플레이어 긴장감을 조성하는 "준비! 3... 2... 1... 시작!" 전용 애니메이션 창이야.
void Renderer::drawCountdown(int count)
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, Sky);
    drawGrass(dc, 0, 0, width_, height_);
    centered(dc, 212, L"준비", 58, Ink, true);
    // 삼항 연산자를 이용해 카운트 타이머 변수가 0보다 크면 남은 초 숫자를 뿌리고, 0 이하가 되는 순간 "시작" 골드 문구로 치환해 줘.
    centered(dc, 310, count > 0 ? std::to_wstring(count) : L"시작", 108, Yellow, true);
    centered(dc, 506, L"물풍선 대전 시작", 32, Ink, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

// =================================================================
// 4. [★인게임 핵심 마스터 렌더러 구현부] (가장 최중요함)
// =================================================================
// 20FPS 루프가 돌 때마다 실시간 맵 구조체 배열, 1P/2P 실시간 좌표와 현재 깔린 폭탄/불꽃의 가변 vector 리스트를
// 아틀라스 핀셋 스캔 방식으로 한꺼번에 전달받아 도화지에 레이어 순서대로 중첩 드로잉하는 심장 마스터 함수야!
void Renderer::draw(
    const GameMap& map,
    const Player& p1,
    const Player& p2,
    const std::vector<Bomb>& bombs,
    const std::vector<Flame>& flames,
    GameResult result)
{
    (void)result;
    pumpMessages(); // 메시지 수신 버퍼 펌핑 가동
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap); // 백버퍼 메모리 오프닝!

    // 레이어 ①: 제일 밑바닥 로비 전체 월페이퍼 도배
    fillRect(dc, 0, 0, width_, height_, SoftBlue);
    drawGrass(dc, 0, 0, width_, height_);

    // 레이어 ②: 상단 UI 스탯 대시보드 스케치 (1P와 2P의 현재 아이템 보유 스탯 실시간 성적표 수치 갱신)
    roundRect(dc, 118, 34, 764, 94, 8, Panel, Outline);
    fillRect(dc, 145, 58, 56, 48, WallBlue);
    drawUncleFace(dc, 153, 62, 40); // 1P 프로필 사진 박기
    text(dc, 214, 56, L"1P", 24, WallBlue, true);
    // 1P가 실시간으로 먹은 별, 신발, 물약의 현재 최대 스탯 현황판 출력
    text(dc, 214, 86,
        L"폭탄 " + std::to_wstring(p1.maxBombs) +
        L"  사거리 " + std::to_wstring(p1.range) +
        L"  바늘 " + std::to_wstring(p1.needles), 18, Ink, true);

    roundRect(dc, 423, 54, 154, 56, 8, White, Outline);
    centered(dc, 66, L"대전", 28, Red, true); // 정중앙 매치 타이틀 칠판 박기

    // 2P 스탯 정보 우측 정렬 레이아웃 조각
    drawSimpleFace(dc, 800, 58, 48, Red); // 2P 프로필 사진 박기
    text(dc, 710, 56, L"2P", 24, Red, true);
    text(dc, 590, 86,
        L"폭탄 " + std::to_wstring(p2.maxBombs) +
        L"  사거리 " + std::to_wstring(p2.range) +
        L"  바늘 " + std::to_wstring(p2.needles), 18, Ink, true);

    // 레이어 ③: 실제 전투가 펼쳐지는 중앙 전투판 격자 테두리 프레임 생성
    roundRect(dc, BoardX - 18, BoardY - 18, BoardW + 36, BoardH + 36, 8, Cream, Outline);
    fillRect(dc, BoardX - 8, BoardY - 8, BoardW + 16, BoardH + 16, Outline);
    fillRect(dc, BoardX, BoardY, BoardW, BoardH, MintA); // 내부 바둑판 기본 판때기 거치

    // [★이중 루프 전수조사 스캐너 레이어] 17x13 모눈종이 좌표계를 돌며 타일 정보를 받아 한 땀 한 땀 조각해!
    for (int y = 0; y < MAP_H; ++y)
    {
        for (int x = 0; x < MAP_W; ++x)
        {
            const Vec2 pos{ x, y };
            // 모눈종이 주소에 격자 한 칸 크기(34픽셀)를 곱해 절대 화면상 픽셀 좌표(sx, sy)로 기하학적 매핑을 수행해!
            const int sx = BoardX + x * Cell;
            const int sy = BoardY + y * Cell;

            // 타일 기본 잔디 깔기
            drawGrass(dc, sx, sy, Cell, Cell);

            // 우선순위 분기점 1: 만약 현재 좌표가 물풍선이 펑! 터져서 불꽃이 지나가는 범위 목록(`flames`)에 겹친다면?
            if (hasFlameAt(pos, flames))
            {
                drawFlame(dc, sx, sy, Cell); // 십자 물줄기 스프라이트 우선 출력!
            }
            // 우선순위 분기점 2: 불꽃이 지나가는 자리가 아니라면 안전하게 고정 맵 장부 데이터를 조회해.
            else
            {
                switch (map.tileAt(pos))
                {
                case Tile::Wall:  drawWall(dc, sx, sy, Cell);  break; // 안 부서지는 콘크리트 벽 기둥 박기
                case Tile::Block: drawCrate(dc, sx, sy, Cell); break; // 부서지는 나무 상자 박기
                case Tile::Empty: // 빈길인 경우에는 혹시 상자가 부서져 바닥에 아이템 보물이 스폰되어 있는지 교차 확인 후 드로잉해!
                    drawItem(dc, sx, sy, Cell, map.itemAt(pos));
                    break;
                }
            }

            // 우선순위 분기점 3: 위의 배경/타일/물줄기 레이어를 그린 직후, 그 자리에 설치된 대기 중인 물풍선(`bombs`)이 있다면 그 위에 덮어 씌워 그려!
            if (hasBombAt(pos, bombs))
            {
                drawBomb(dc, sx, sy, Cell);
            }
        }
    }

    // 레이어 ④: 정적 타일과 이펙트 렌더링이 싹 끝난 최종 상단 위치에 '살아있는 주인공 캐릭터'들의 좌표를 산출해 얹어 덮어써!
    if (p1.alive) drawPlayer(dc, BoardX + p1.pos.x * Cell, BoardY + p1.pos.y * Cell, Cell, p1, RGB(79, 173, 232));
    if (p2.alive) drawPlayer(dc, BoardX + p2.pos.x * Cell, BoardY + p2.pos.y * Cell, Cell, p2, Red);

    // 레이어 ⑤: 화면 맨 아래쪽 실시간 캐릭터 스태이트 상황판(생존/갇힘/탈락 상태 칠판) 출력 UI 드로잉
    const int uiY = BoardY + BoardH + 42;
    roundRect(dc, 174, uiY, 652, 54, 8, Panel, Outline);
    // 삼항 연산자를 중첩해 생존 시에는 trapped 여부를 검사해 "갇힘" 또는 "생존" 상태 문구를 실시간 가변 텍스트로 전환 연산해 줘.
    text(dc, 204, uiY + 15, L"1P " + std::wstring(p1.alive ? (p1.trapped ? L"갇힘" : L"생존") : L"탈락"), 19, WallBlue, true);
    text(dc, 342, uiY + 15, L"WASD / Shift", 18, Ink, true);
    text(dc, 508, uiY + 15, L"방향키 / Space", 18, Ink, true);
    text(dc, 690, uiY + 15, L"2P " + std::wstring(p2.alive ? (p2.trapped ? L"갇힘" : L"생존") : L"탈락"), 19, Red, true);

    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap); // 비밀리에 완성된 오프스크린 종합 백버퍼 그림을 모니터 전면 전송!
}

void Renderer::drawGameOver(GameResult result)
{
    drawResultScreen(result); // 게임 오버 시 결과 종료 보드창으로 토스해 유연하게 연출 통합 수행
}

// [결과창 렌더링 대장] 경기가 종료(무승부, 1P승, 2P승)되었을 때 뜨는 최종 결과 스코어 패널 칠판창이야.
void Renderer::drawResultScreen(GameResult result)
{
    pumpMessages();
    HDC windowDc = nullptr, memoryDc = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr;
    HDC dc = beginPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);

    fillRect(dc, 0, 0, width_, height_, Sky);
    drawGrass(dc, 0, 0, width_, height_);
    roundRect(dc, 120, 166, 760, 390, 12, Cream, Outline);

    // 열거형 매치 결과값 조건 분기 정산 후 출력 문자열 바인딩
    std::wstring message = L"무승부";
    if (result == GameResult::P1Win)      message = L"1P 승리";
    else if (result == GameResult::P2Win) message = L"2P 승리";
    else if (result == GameResult::Quit)  message = L"게임 종료";

    centered(dc, 252, message, 78, Yellow, true); // 영광의 승리자 이름 빅 사이즈 도트 노출
    centered(dc, 370, L"엔터: 메인 메뉴", 32, Ink, true);
    centered(dc, 426, L"ESC: 종료", 26, Red, true);
    endPaintFrame(windowDc, memoryDc, bitmap, oldBitmap);
}

// =================================================================
// 5. 더블 버퍼링 (Double Buffering) 그래픽 보호 엔진 하위 코어 시스템 ⭐⭐⭐
// =================================================================
// 화면을 다 그리기 전까지 유저의 눈을 가려두고 가짜 도화지(메모리 캔버스)를 비밀리에 신설해 주는 핵심 오프스크린 기술 엔진이야.
HDC Renderer::beginPaintFrame(HDC& windowDc, HDC& memoryDc, HBITMAP& bitmap, HBITMAP& oldBitmap)
{
    windowDc = GetDC(hwnd_); // 1. 운영체제로부터 현재 실물 윈도우 창의 디바이스 콘텍스트(붓과 도화지 소유권)를 넘겨받아.
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int clientW = client.right - client.left;
    const int clientH = client.bottom - client.top;

    // 2. [가짜 비밀 도화지 생성] 진짜 모니터 도화지(windowDc)와 품질이 완벽히 호환되는 고속 연산용 메모리 캔버스 공간을 복제 생성해!
    memoryDc = CreateCompatibleDC(windowDc);
    // 3. 픽셀 색상 정보를 저장할 비트맵 도화지를 생성해 백버퍼 메모리 기계장치에 마운팅(SelectObject)해 줘.
    bitmap = CreateCompatibleBitmap(windowDc, clientW, clientH);
    oldBitmap = static_cast<HBITMAP>(SelectObject(memoryDc, bitmap));

    SetBkMode(memoryDc, TRANSPARENT); // 글자 배경을 투명하게 세팅해 도트 폰트 외곽에 흰 네모가 생기는 현상을 원천 차단해.
    fillRect(memoryDc, 0, 0, clientW, clientH, Black); // 일단 백버퍼를 암전색으로 깔끔하게 밀어 청소부 정산해.

    // [화면 해상도 비율 레터박스 연산 수식] 창 크기가 마구 늘어나거나 변해도 항상 1000x800 중심 비율을 유지하게 화면 원점을 계산해 여백 오프셋을 먹여줘!
    const int offsetX = std::max(0, (clientW - width_) / 2);
    const int offsetY = std::max(0, (clientH - height_) / 2);
    SetViewportOrgEx(memoryDc, offsetX, offsetY, nullptr); // 가짜 도화지의 가상 스케일 원점 이동 정산 완료
    return memoryDc; // 다른 드로잉 헬퍼 함수들이 이 비밀 도화지(memoryDc)에 그림을 가득 채우도록 전달해 줘.
}

// 비밀 메모리 도화지에서 백작업 작화가 완전히 끝나는 순간, 통째로 진짜 화면 비디오 메모리에 전송 소멸시키는 정산 함수야.
void Renderer::endPaintFrame(HDC windowDc, HDC memoryDc, HBITMAP bitmap, HBITMAP oldBitmap)
{
    RECT client{};
    GetClientRect(hwnd_, &client);
    // 1. [고속 비트 전송 기술: BitBlt] 비밀 도화지(memoryDc)에 완벽하게 완성이 끝난 그림을 진짜 모니터 픽셀 버퍼(windowDc)에다가 0.001초 만에 복사해 버려!
    // 이 처리가 기말 프로젝트 화면 번쩍임 깜빡임 결함을 완벽히 해결하는 치트키 메커니즘의 마침표란다.
    BitBlt(windowDc, 0, 0, client.right - client.left, client.bottom - client.top, memoryDc, 0, 0, SRCCOPY);

    // 2. 사용이 끝난 임시 가짜 도화지 장치 부품과 비트맵 사각형들을 메모리에서 소멸시켜 자원 해제(오버헤드 방지) 처리를 단행해.
    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(hwnd_, windowDc); // 운영체제에 그래픽 제어권을 반납하며 한 프레임 작화 정산 완료!
}

// =================================================================
// 6. GDI 기반 원초적 저수준 도형 렌더링 헬퍼 함수군
// =================================================================

void Renderer::fillRect(HDC dc, int x, int y, int w, int h, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color); // 단색 사각형 면을 채우는 붓 생성
    RECT r{ x, y, x + w, y + h };
    FillRect(dc, &r, brush);
    DeleteObject(brush); // 사용이 끝난 하드웨어 오브젝트 펜 자원은 반드시 소멸(GDI Leak 방지)!
}

void Renderer::roundRect(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH brush = CreateSolidBrush(fill);    // 안감 채우기 붓
    HPEN   pen = CreatePen(PS_SOLID, 4, border); // 외곽선 갈색 4픽셀 두께 테두리선 펜 생성
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, radius, radius); // Win32 GDI 전용 모서리가 둥근 세련된 패널 생성 API
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// 글로벌 비트맵 가위질 객체에게 대리 오더를 위임하여 글자를 출력하는 다리 함수야.
bool Renderer::bitmapText(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color)
{
    return g_fontAtlas.draw(dc, x, y, value, size, color);
}

SIZE Renderer::bitmapTextSize(const std::wstring& value, int size)
{
    return g_fontAtlas.measure(value, size);
}

// 만약 font_korean.png 파일이 누락되거나 에러가 터졌을 때 게임이 튕기지 않게 윈도우 내장 "맑은 고딕" 시스템 기본 폰트로 문자를 출력해 주는 방어형 폴백(Fallback) 함수야!
void Renderer::text(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        bitmapText(dc, x, y, value, size, color); // 에셋 정상 작동 시 우리 도트 폰트로 즉시 유턴 우회그리기 실행!
        return;
    }

    // PNG 로드 실패 시 가동되는 비상 발전기: 하드웨어 기본 래스터 폰트 시스템 발동
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, value.c_str(), static_cast<int>(value.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

// 특정 문구 문자열을 화면의 X축 정확한 가로 정중앙 위치를 계산해 예쁘게 정렬 가독성을 정산하는 정렬 함수야.
void Renderer::centered(HDC dc, int y, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        const SIZE s = bitmapTextSize(value, size);
        const int x = (width_ - s.cx) / 2; // 전체 해상도 폭(1000픽셀)에서 문자열 실제 가로 크기를 빼고 2로 나누어 센터 좌표를 산출해!
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

// 로비 버튼 사각형 RECT 박스 범위를 넘겨받아 버튼 상자 한가운데에 텍스트가 정확히 파고들게 계산하는 중앙 정렬 장치야.
void Renderer::centeredInRect(HDC dc, RECT rect, const std::wstring& value, int size, COLORREF color, bool bold)
{
    if (g_fontAtlas.load())
    {
        const SIZE s = bitmapTextSize(value, size);
        // 사각형 내부 전용 정중앙 스케일링 수식 연산
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

// 로비 UI 삼대 버튼(시작, 도움말, 종료) 각각이 차지하고 있는 정밀 마우스 조작 충돌 영역 RECT 공간 범위를 하드코딩 없이 리턴해 주는 좌표 산출기야.
RECT Renderer::menuButtonRect(MenuChoice choice) const
{
    int y = 378; // 시작 버튼 베이스 Y축 절대 픽셀 주소
    if (choice == MenuChoice::Help) y = 458;      // 80픽셀 단위로 정밀 아래 이격 간격 계산
    else if (choice == MenuChoice::Exit) y = 538;
    return RECT{ 342, y, 658, y + 58 }; // 가로 폭 316픽셀, 세로 폭 58픽셀 규격의 사각형 박스 리턴
}

// 로비 전용 UI 단추 그리기 렌더 함수야. 마우스 휠이 올라갔거나 키보드로 선택 중인 단추는 옐로우 틴팅 효과물감으로 예쁘게 반응 유도 연출을 수행해.
void Renderer::drawButton(HDC dc, MenuChoice choice, const std::wstring& label, bool selected)
{
    RECT r = menuButtonRect(choice);
    const COLORREF fill = selected ? Yellow : White; // 선택 시 노란색 단추로 변신
    const COLORREF border = selected ? Red : Outline; // 선택 시 빨간 테두리선 강조
    roundRect(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, 8, fill, border);

    // 아케이드 감성을 주기 위해 버튼 모서리 네 군데에 미니 픽셀 스티커 사각형 장식물을 중첩 드로잉해 줘.
    fillRect(dc, r.left + 10, r.top + 10, 14, 14, selected ? Red : WallBlue);
    fillRect(dc, r.left + 26, r.top + 10, 14, 14, selected ? White : Yellow);
    fillRect(dc, r.right - 40, r.bottom - 24, 14, 14, selected ? White : Yellow);
    fillRect(dc, r.right - 24, r.bottom - 24, 14, 14, selected ? Red : WallBlue);

    centeredInRect(dc, r, label, 34, Ink, true); // 버튼 정중앙 텍스트 최종 도장 쾅!
}

// =================================================================
// 7. 인게임 순수 스프라이트(도장) 세부 픽셀 조각 함수군
// =================================================================

// [잔디밭 렌더 함수] 체크무늬 미로 바닥을 생성해 주고 미세 도트 점을 찍어 고전 패미컴 느낌의 잔디 텍스처를 묘사해.
void Renderer::drawGrass(HDC dc, int x, int y, int w, int h)
{
    fillRect(dc, x, y, w, h, MintA);
    const int tile = Cell;
    // 인자로 넘어온 영역 크기만큼 이중 포문을 돌려 짝수/홀수 타일 위치마다 연초록과 초록을 교차 배열하여 체크무늬 필드를 조각해.
    for (int yy = y; yy < y + h; yy += tile)
    {
        for (int xx = x; xx < x + w; xx += tile)
        {
            fillRect(dc, xx, yy, tile, tile,
                (((xx / tile) + (yy / tile)) % 2 == 0) ? MintA : MintB);
            // 디테일: 잔디밭 픽셀이 심심하지 않게 좌하단/우상단 구석에 3x3 픽셀짜리 미니 풀잎 점(TileDot)을 두 개씩 박아준다!
            fillRect(dc, xx + 5, yy + 5, 3, 3, TileDot);
            fillRect(dc, xx + tile - 9, yy + tile - 9, 3, 3, TileDot);
        }
    }
    // 인게임 단일 낱칸(34픽셀) 잔디 블록을 그릴 때는 타일 경계선 구분을 위해 상단과 좌측 모서리에 1픽셀 두께의 잔디 경계 외곽 구분 음영선을 둘러쳐 줘.
    if (w == Cell && h == Cell)
    {
        fillRect(dc, x, y, w, 1, RGB(145, 202, 159));
        fillRect(dc, x, y, 1, h, RGB(145, 202, 159));
    }
}

// [단단한 벽 기둥 렌더 함수] 절대 안 부서지는 고정 벽돌 기둥을 그려. 지루함을 덜기 위해 좌표 모듈러 수식으로 파랑, 노랑, 빨강 벽돌이 골고루 섞여 나오게 조율했어!
void Renderer::drawWall(HDC dc, int x, int y, int size)
{
    const COLORREF fill =
        (((x / Cell) + (y / Cell)) % 3 == 0) ? WallBlue :
        (((x / Cell) + (y / Cell)) % 3 == 1) ? WallYellow :
        WallRed;

    // 픽셀 사각형들을 안쪽으로 좁혀가며 중첩 레이어로 쌓아 3D 입체 블록처럼 계단식 도트 쉐이딩 연출을 준 기막힌 코드란다.
    fillRect(dc, x + 3, y + 4, size - 6, size - 7, Outline);      // 베이스 갈색 외곽 프레임
    fillRect(dc, x + 5, y + 2, size - 10, size - 8, Outline);
    fillRect(dc, x + 6, y + 5, size - 12, size - 13, fill);       // 실세 메인 기둥 컬러색 투척
    fillRect(dc, x + 9, y + 8, 6, 5, White);                      // 좌측 상단 번쩍이는 반사광 화이트 도트 박기
    fillRect(dc, x + size - 14, y + 8, 5, 5, White);               // 우측 상단 반사광 도트 박기
    fillRect(dc, x + 9, y + size - 13, size - 18, 5, WallShadow);  // 하단 짙은 바닥면 그림자 음영 블록 생성
    fillRect(dc, x + 14, y + 16, 5, 5, Outline);                  // 기둥 정면 벽돌 무늬 도트 장식 나사 박기
    fillRect(dc, x + size - 19, y + 16, 5, 5, Outline);
}

// [나무 상자 렌더 함수] 물풍선에 맞으면 박살 나서 아이템을 뱉어내는 나무 크레이트 오브젝트를 픽셀로 조각해.
void Renderer::drawCrate(HDC dc, int x, int y, int size)
{
    // 갈색, 진갈색, 연갈색 물감 사각형들을 안쪽으로 겹쳐 쌓아 입체 나무 질감을 구현해.
    fillRect(dc, x + 4, y + 4, size - 8, size - 8, CrateDark);
    fillRect(dc, x + 7, y + 7, size - 14, size - 14, Crate);
    fillRect(dc, x + 10, y + 10, size - 20, 4, CrateLight);      // 상자 윗면 밝은 나무 판자 무늬선
    fillRect(dc, x + 10, y + size - 14, size - 20, 4, CrateDark); // 상자 아랫면 어두운 나무 판자 선
    fillRect(dc, x + 9, y + 9, 4, size - 18, CrateDark);
    fillRect(dc, x + size - 13, y + 9, 4, size - 18, CrateDark);

    // 포문을 이용해 사선 방향으로 나무 상자 특유의 X자형 결속 나무 판자 띠 무늬를 5픽셀 단위 도트 간격으로 쾅쾅 찍어 완성해 줘!
    for (int i = 0; i < size - 18; i += 5)
    {
        fillRect(dc, x + 10 + i, y + 10 + i, 4, 4, CrateDark);       // \ 방향 사선 줄무늬 도트
        fillRect(dc, x + size - 14 - i, y + 10 + i, 4, 4, CrateDark); // / 방향 사선 줄무늬 도트
    }
}

// [물풍선 렌더 함수] 바닥에 놓여 출렁거리는 동그란 물방울 폭탄 형상을 조각하는 함수야.
void Renderer::drawBomb(HDC dc, int x, int y, int size)
{
    // 원형을 GDI 사각형 픽셀 조합으로 부드러운 도트 곡선을 만들어 낸 기하학 구조야.
    fillRect(dc, x + 11, y + 11, size - 22, size - 16, Outline);  // 세로가 긴 갈색 외곽 타원 프레임
    fillRect(dc, x + 8, y + 15, size - 16, size - 21, Outline);   // 가로가 긴 갈색 외곽 타원 프레임
    fillRect(dc, x + 12, y + 13, size - 24, size - 19, Water);    // 내부에 꽉 들어차는 파란색 물탱크 픽셀 투척
    fillRect(dc, x + 10, y + 17, size - 20, size - 25, Water);
    fillRect(dc, x + 15, y + 15, 7, 6, WaterLight);               // 물풍선 좌측 상단에 쨍하고 맺히는 말랑말랑한 물방울 하이라이트광 스케치
    fillRect(dc, x + 23, y + 9, 6, 5, Outline);                   // 폭탄 상단 물풍선 주둥이 매듭 갈색 끈
    fillRect(dc, x + 27, y + 7, 4, 4, Yellow);                    // 째깍거리며 타들어가는 노란색 심지 도트 삽입!
}

// [물줄기 불꽃 렌더 함수] 폭탄이 타이머 종료 후 사방 십자 레이어로 분출할 때 타오르는 세찬 액체 이펙트 그래픽이야.
void Renderer::drawFlame(HDC dc, int x, int y, int size)
{
    // 가로줄 물줄기와 세로줄 사각형 물감을 교차로 교차시켜 완벽한 십자(十字) 모양 폭발 광선을 구현해!
    fillRect(dc, x + 2, y + 13, size - 4, 10, Outline); // 가로 십자 줄기 갈색 외곽선
    fillRect(dc, x + 13, y + 2, 10, size - 4, Outline); // 세로 십자 줄기 갈색 외곽선
    fillRect(dc, x + 3, y + 14, size - 6, 8, Water);    // 내부에 고속 분출하는 세찬 파란색 물줄기 투척
    fillRect(dc, x + 14, y + 3, 8, size - 6, Water);
    fillRect(dc, x + 7, y + 16, size - 14, 4, WaterLight); // 물줄기 중앙을 관통하는 하얀색 고압 압축 코어선 스케치
    fillRect(dc, x + 16, y + 7, 4, size - 14, WaterLight);
    fillRect(dc, x + 14, y + 14, 8, 8, White);           // 격자 교차점 한가운데에는 쨍한 화이트 섬광 픽셀 배치!
}

// [주인공 캐릭터 렌더 대장 함수] 플레이어 엔티티의 ID 번호와 물풍선 피격 유무 플래그를 실시간 역추적해 기가 막히게 분기 조립하는 총감독 함수야.
void Renderer::drawPlayer(HDC dc, int x, int y, int size, const Player& player, COLORREF color)
{
    // 중요 예외 제어 ①: 만약 플레이어가 물풍선 불꽃에 맞아 물방울 갇힘 스태이트(`trapped == true`)가 되었다면?
    if (player.trapped)
    {
        drawBomb(dc, x, y, size); // 캐릭터 머리통 위에 거대한 물풍선 구체를 강제로 오버랩 중첩 렌더링해 가두어버려!
    }

    // 중요 예외 제어 ②: 1P와 2P 구분을 위해 플레이어 ID 고유 넘버를 스캔해 서로 다른 이목구비 펑션을 가동해!
    if (player.id == 1)
    {
        drawUncleFace(dc, x + 2, y + 1, size - 4); // 1P는 배오개 레트로 감성 도트 아저씨 얼굴 도장 쾅!
    }
    else
    {
        drawSimpleFace(dc, x + 1, y + 1, size - 2, color); // 2P는 전달받은 고유 컬러(레드) 프레임의 심플 눈코입 얼굴 도장 쾅!
    }
}

// [1P 전용 고전 도트 아저씨 얼굴 묘사 함수] 픽셀 그리드 단위를 최소 배수 단위(u)로 쪼개 정밀 장인 정신으로 눈, 눈동자, 살색, 단발머리를 한 땀 한 땀 찍어낸 그래픽의 정수야.
void Renderer::drawUncleFace(HDC dc, int x, int y, int size)
{
    const COLORREF skin = RGB(248, 202, 158);
    const COLORREF hair = RGB(18, 18, 16);
    const COLORREF shadow = RGB(221, 164, 123);
    const int u = std::max(2, size / 16); // 픽셀 단위 스케일 팩터 연산

    const int faceX = x + 3 * u;
    const int faceY = y + 7 * u;
    const int faceW = 10 * u;
    const int faceH = 7 * u;

    // 얼굴 살색 베이스 배치 및 턱 밑 그림자 묘사
    fillRect(dc, faceX, faceY, faceW, faceH, skin);
    fillRect(dc, faceX, faceY + faceH, faceW, u, shadow);

    // 단발머리 가발 픽셀 계단식 레이아웃 중첩 생성
    fillRect(dc, x + 2 * u, y + 5 * u, 12 * u, 3 * u, hair);
    fillRect(dc, x + 1 * u, y + 6 * u, 3 * u, 2 * u, hair);
    fillRect(dc, x + 12 * u, y + 6 * u, 3 * u, 2 * u, hair);
    fillRect(dc, x + 4 * u, y + 3 * u, 8 * u, 3 * u, hair);
    fillRect(dc, x + 6 * u, y + 2 * u, 4 * u, 2 * u, hair);
    fillRect(dc, x + 8 * u, y + 4 * u, u, 4 * u, skin);

    // 썬글라스 혹은 동공 눈동자 좌우 대칭 픽셀 박기
    fillRect(dc, x + 3 * u, y + 8 * u, 4 * u, 3 * u, Black);
    fillRect(dc, x + 9 * u, y + 8 * u, 4 * u, 3 * u, Black);
    fillRect(dc, x + 4 * u, y + 9 * u, 2 * u, u, White);          // 반짝이는 하얀 눈빛 눈동자 도트 박기
    fillRect(dc, x + 10 * u, y + 9 * u, 2 * u, u, White);
    fillRect(dc, x + 7 * u, y + 9 * u, 2 * u, u, Black);          // 코대 음영 음영선 삽입

    fillRect(dc, faceX - u, faceY + faceH, faceW + 2 * u, u, Outline); // 목덜미 옷깃 갈색 외곽선 마감
}

// [2P 및 범용 심플 페이스 함수] 전달받은 팀 고유 컬러 블록 테두리 안에 살색 바닥을 깔고 도트 눈알 두 개를 콕콕 박아 완성하는 미니멀 캐릭터 묘사 함수야.
void Renderer::drawSimpleFace(HDC dc, int x, int y, int size, COLORREF frameColor)
{
    const int pad = std::max(4, size / 6);   // 외곽 마진 폭 연산
    const int eye = std::max(3, size / 11);  // 눈알 픽셀 스케일 크기 연산
    const int inner = size - 2 * pad;

    fillRect(dc, x, y, size, size, frameColor); // 1P/2P 구동 고유 프레임 컬러 도배
    fillRect(dc, x + pad, y + pad, inner, inner, RGB(255, 241, 222)); // 내장 부드러운 백색 피부 스케치

    // 정확한 3등분 비율 연산 공식을 돌려 좌측 눈알과 우측 눈알 검은색 도트를 한 치의 오차 없이 배치해!
    fillRect(dc, x + pad + inner / 3 - eye / 2, y + pad + inner / 2 - eye / 2, eye, eye, Black);
    fillRect(dc, x + pad + (inner * 2) / 3 - eye / 2, y + pad + inner / 2 - eye / 2, eye, eye, Black);
}

// [4대 보물 아이템 스프라이트 드로잉 함수] 상자가 깨진 빈길 바닥에 아이템 유형 매치 스캔을 돌려 각각 고유 형상의 아이템 상자를 렌더링해 줘.
void Renderer::drawItem(HDC dc, int x, int y, int size, ItemType item)
{
    if (item == ItemType::None) return; // 아이템이 없는 꽝 바닥이면 즉시 연산을 생략하고 철수해.

    const int cx = x + size / 2;
    const int cy = y + size / 2;

    // 아이템을 감싸 안는 하얀색 정사각형 보석 상자 프레임 기단 깔기
    fillRect(dc, x + 8, y + 8, size - 16, size - 16, White);
    fillRect(dc, x + 7, y + 9, 2, size - 18, Outline);
    fillRect(dc, x + size - 9, y + 9, 2, size - 18, Outline);
    fillRect(dc, x + 9, y + 7, size - 18, 2, Outline);
    fillRect(dc, x + 9, y + size - 9, size - 18, 2, Outline);

    // 아이템 유형 번호 조건 분기 시작!
    if (item == ItemType::BombCount) // 1. 파란 별 아이템: 최대 설치 물풍선 개수를 실시간 보너스 1 정산해 줘!
    {
        const COLORREF blue = RGB(47, 155, 255);
        // 물풍선 모양 미니어처 아이콘 픽셀 조각
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
    else if (item == ItemType::Range) // 2. 물약 아이템: 십자 폭발 물줄기 화력 거리를 +1 스케일업해 줘!
    {
        // 분홍색 크로스(십자가) 마크 아이콘 정밀 조각
        fillRect(dc, cx - 4, cy - 12, 8, 24, Pink);
        fillRect(dc, cx - 12, cy - 4, 24, 8, Pink);
        fillRect(dc, cx - 2, cy - 10, 4, 20, RGB(255, 190, 232));
        fillRect(dc, cx - 10, cy - 2, 20, 4, RGB(255, 190, 232));
    }
    else if (item == ItemType::Speed) // 3. 신발 아이템: 캐릭터 이동 속도 대기 타임 프레임을 단축시켜 줘!
    {
        const COLORREF green = RGB(88, 222, 95);
        // 초록색 번개 모양 질주 기하학 도트 조각
        fillRect(dc, cx - 1, cy - 13, 9, 5, green);
        fillRect(dc, cx - 5, cy - 8, 11, 5, green);
        fillRect(dc, cx - 9, cy - 3, 13, 5, green);
        fillRect(dc, cx - 4, cy + 2, 12, 5, green);
        fillRect(dc, cx - 8, cy + 7, 8, 5, green);
        fillRect(dc, cx - 11, cy + 12, 5, 3, green);
        fillRect(dc, cx + 2, cy - 10, 4, 4, RGB(207, 255, 196));
    }
    else if (item == ItemType::Needle) // 4. 노란 바늘 아이템: 물방울 터짐 방어용 바늘 스탯 보유량을 추가해 줘!
    {
        // 박스 자체를 황금색 노란 보석함으로 물들이고 한가운데 유니코드 "침" 자소 레이아웃 박기 실행
        fillRect(dc, x + 8, y + 8, size - 16, size - 16, Yellow);
        fillRect(dc, x + 7, y + 9, 2, size - 18, Outline);
        fillRect(dc, x + size - 9, y + 9, 2, size - 18, Outline);
        fillRect(dc, x + 9, y + 7, size - 18, 2, Outline);
        fillRect(dc, x + 9, y + size - 9, size - 18, 2, Outline);
        text(dc, x + 8, y + 8, L"침", 20, Ink, true);
    }
}

// =================================================================
// 8. 레이어 중첩 그리기 사전 전수조사 스캐너 검사기 (안전 함수군)
// =================================================================

// 현재 스캔 중인 바둑판 칸(p) 위에 "아직 안 터지고 째깍거리는 실물 폭탄"이 놓여있는지 감지하는 추적 장치야.
bool Renderer::hasBombAt(Vec2 p, const std::vector<Bomb>& bombs) const
{
    for (const Bomb& bomb : bombs)
    {
        if (!bomb.exploded && bomb.pos == p) return true; // 아직 안 터진 폭탄이 이 자리에 고여 있다면 즉시 true 리턴!
    }
    return false;
}

// 현재 스캔 중인 바둑판 칸(p) 내부 격자 범위 공간에 "펑! 하고 터져버린 세찬 물줄기 벡터 줄기"가 한 가닥이라도 점유 중인지 체크하는 센서야.
bool Renderer::hasFlameAt(Vec2 p, const std::vector<Flame>& flames) const
{
    for (const Flame& flame : flames)
    {
        for (Vec2 cell : flame.cells)
        {
            if (cell == p) return true; // 폭발 물줄기 불꽃 타일 리스트 레이어 영역에 겹치면 무조건 true 리턴!
        }
    }
    return false;
}