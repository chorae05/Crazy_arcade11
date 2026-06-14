#pragma once

#include "Map.h"
#include "Types.h"
#include <Windows.h>
#include <string>
#include <vector>

// =================================================================
// 게임의 화면 출력과 그래픽 렌더링을 총괄하는 모니터 (클래스)
// =================================================================
// 숫자 데이터로 구성된 게임 스탯들을 넘겨받아 '더블 버퍼링' 기술을 활용해
// 깜빡임 없는 예쁜 Win32 GDI 그래픽 화면으로 그려주는 총책임자야.
class Renderer
{
public:
    // [생성자] 윈도우 창 크기(1000x800)와 마우스 클릭 상태 등 기본 세팅값을 초기화해.
    Renderer();

    void setupConsole();                           // 게임용 그래픽 창(가로 1000, 세로 800)을 OS에 요청해서 띄우는 함수
    void hideCursor();                             // 인게임 몰입도를 위해 마우스 커서 화살표를 화면에서 잠시 숨겨주는 함수
    void clearScreen();                            // 도화지를 깨끗하게 검은색이나 기본 배경색으로 싹 밀어버리는 청소 함수
    bool pumpMessages();                           // 윈도우 창의 마우스 클릭이나 닫기 같은 외부 이벤트 신호를 실시간으로 수집하는 함수
    bool windowClosed() const;                     // 플레이어가 우측 상단 X 버튼을 눌러 창을 닫았는지 감시하는 센서 함수
    bool consumeMenuClick(MenuChoice& choice);     // 메인 메뉴 화면에서 내 마우스 클릭이 버튼 영역에 닿았는지 정밀 정산하는 함수

    // --- 칠판 전환 전용 렌더링 함수들 (장면 바꾸기) ---
    void drawMainMenu(MenuChoice selected);        // 메인 타이틀 메뉴 화면(시작/도움말/종료 버튼)을 그려주는 함수
    void drawHelp();                               // 조작법과 아이템 정보가 가득 적힌 설명서 화면을 그려주는 함수
    void drawCountdown(int count);                 // 게임 시작 직전 3, 2, 1 카운트다운 숫자를 화면 정중앙에 크게 그려주는 함수

    // [인게임 렌더러 마스터] 매 프레임(0.05초)마다 최신 맵 상태, 플레이어 좌표, 폭탄/물줄기 목록을 한꺼번에 받아 화면에 다 찍어내는 종합 함수
    void draw(
        const GameMap& map,
        const Player& p1,
        const Player& p2,
        const std::vector<Bomb>& bombs,
        const std::vector<Flame>& flames,
        GameResult result);

    void drawGameOver(GameResult result);          // 게임이 끝났을 때 화면을 반투명하게 흐리고 문구를 띄우는 연출 함수
    void drawResultScreen(GameResult result);      // 최종 승리자(1P승, 2P승, 무승부) 결과가 적힌 스코어보드 칠판을 그려주는 함수

private:
    // =================================================================
    // Win32 OS 제어 및 하드웨어 연동 변수들 (비밀 금고)
    // =================================================================
    HANDLE out_;                                   // 콘솔 창의 출력 제어권을 쥐고 있는 운영체제용 핸들 변수
    HWND hwnd_ = nullptr;                          // 생성된 윈도우 창의 고유 주소 아이디 (주민등록번호 같은 핸들 변수)
    int width_ = 1000;                             // 게임 화면의 가로 해상도 크기 (1000 픽셀)
    int height_ = 800;                             // 게임 화면의 세로 해상도 크기 (800 픽셀)
    bool closed_ = false;                          // 현재 창이 닫혔는지 닫히지 않았는지 기록하는 플래그 변수
    bool hasClick_ = false;                        // 마우스 클릭 이벤트가 방금 발생했는지 감지하는 센서 변수
    POINT click_{};                                // 마우스가 클릭된 실제 화면 픽셀 위치(x, y)를 저장해 두는 주소록

    // [윈도우 메시지 수신기 안테나] 마우스 클릭 등 Windows OS의 신호를 Renderer 내부 변수들과 동기화해 주는 프렌드 함수
    friend LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // --- 더블 버퍼링 (화면 깜빡임 완벽 차단 기술) 내부 가동 함수 ---
    // 가짜 도화지(메모리 DC)를 만들어서 그림을 몰래 다 그린 뒤, 완성작을 진짜 모니터 도화지(windowDc)에 쾅! 복사하는 기술의 시작과 끝이야.
    HDC beginPaintFrame(HDC& windowDc, HDC& memoryDc, HBITMAP& bitmap, HBITMAP& oldBitmap); // 가짜 도화지를 가동하고 붓을 쥐어주는 함수
    void endPaintFrame(HDC windowDc, HDC memoryDc, HBITMAP bitmap, HBITMAP oldBitmap);       // 그림이 완성되면 모니터에 고속 복사(BitBlt)하고 정산하는 함수

    
    void fillRect(HDC dc, int x, int y, int w, int h, COLORREF color); // 지정한 색상 물감으로 사각형을 꽉 채워 그려주는 함수
    void roundRect(HDC dc, int x, int y, int w, int h, int radius, COLORREF fill, COLORREF border); // 테두리가 둥근 세련된 UI 버튼 박스를 그리는 함수

    // --- 비트맵 폰트 아틀라스 가위질 출력 함수 ---
    // font_korean.png라는 큰 문자 지도책 이미지에서 필요한 단어만 격자 단위로 가위질(Crop)해서 찍어내는 기술이야.
    bool bitmapText(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color); // 큰 글자 지도에서 한 글자씩 오려 화면에 붙이는 함수
    SIZE bitmapTextSize(const std::wstring& value, int size); // 출력할 글자(문자열)가 화면에서 차지할 가로/세로 길이를 미리 계산해 여백을 맞추는 함수

    void text(HDC dc, int x, int y, const std::wstring& value, int size, COLORREF color, bool bold = false); // 일반 텍스트 출력 서포터 함수
    void centered(HDC dc, int y, const std::wstring& value, int size, COLORREF color, bool bold = false); // 화면 가로 정중앙 정렬로 글씨를 써주는 함수
    void centeredInRect(HDC dc, RECT rect, const std::wstring& value, int size, COLORREF color, bool bold = false); // UI 버튼 사각형 딱 한가운데에 이쁘게 글씨 쓰는 함수

    RECT menuButtonRect(MenuChoice choice) const;   // 시작, 도움말, 종료 버튼들의 마우스 클릭 감지 범위(좌하단/우상단 RECT 영역)를 연산하는 함수
    void drawButton(HDC dc, MenuChoice choice, const std::wstring& label, bool selected); // 마우스가 올라갔거나 선택된 버튼을 하이라이트 효과 줘서 그리는 함수

    // --- 인게임 2D 스프라이트(도장) 세부 렌더링 함수들 ---
    void drawGrass(HDC dc, int x, int y, int w, int h); // 바닥 배경 레이어인 푸른색 잔디밭 격자 타일을 깔아주는 함수
    void drawWall(HDC dc, int x, int y, int size);      // 외곽 테두리와 내부를 막아서 절대로 안 부서지는 단단한 콘크리트 벽 타일을 그리는 함수
    void drawCrate(HDC dc, int x, int y, int size);     // 부서지면 물약이나 번개 아이템이 튀어 나오는 맵 상의 나무 상자 타일을 그리는 함수
    void drawBomb(HDC dc, int x, int y, int size);      // 바닥에 설치되어 터지기 직전까지 대기 상태로 크기가 실시간으로 째깍거리는 물풍선을 그리는 함수
    void drawFlame(HDC dc, int x, int y, int size);     // 폭발이 일어난 순간 사방으로 세차게 뿜어져 나오는 푸른색 물줄기 이펙트를 그리는 함수
    void drawPlayer(HDC dc, int x, int y, int size, const Player& player, COLORREF color); // p1_과 p2_ 캐릭터의 고유 색상과 좌표를 매칭해 그려주는 함수
    void drawUncleFace(HDC dc, int x, int y, int size); // (디버그/기념용) 아케이드 레트로 스타일 캐릭터의 기본 얼굴 외형을 묘사하는 함수
    void drawSimpleFace(HDC dc, int x, int y, int size, COLORREF frameColor); // 일반 상태나 물풍선에 갇혀 눈물을 흘릴 때 캐릭터 눈코입 표현 함수
    void drawItem(HDC dc, int x, int y, int size, ItemType item); // 상자가 부서진 빈 바닥 자리에 신발, 물약, 별, 바늘 아이템 그래픽을 얹어주는 함수

    // --- 레이어 오차 방지 사전 검사 안전장치 기능 ---
    bool hasBombAt(Vec2 p, const std::vector<Bomb>& bombs) const;   // 현재 이 좌표(p) 자리에 다른 폭탄 이미지가 렌더링 중인지 중복 검사하는 함수
    bool hasFlameAt(Vec2 p, const std::vector<Flame>& flames) const; // 현재 이 좌표(p) 자리에 다른 물줄기 불꽃이 겹쳐서 나오고 있는지 확인하는 함수
};