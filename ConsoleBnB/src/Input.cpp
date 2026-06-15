#include "../include/Input.h"

#include <Windows.h>
#include <conio.h>

// 익명 네임스페이스(namespace): 이 파일(.cpp) 내부에서만 비밀리에 사용하는 독립 함수 정의 공간이야.
namespace
{
    // [하드웨어 신호 센서] 특정 키보드 키가 "지금 이 순간 물리적으로 눌렸는지" 검사하는 함수야.
    bool isDown(int virtualKey)
    {
        // GetAsyncKeyState는 윈도우 OS가 제공하는 함수로, 해당 키가 눌려있으면 맨 앞 1비트(최상위 비트)를 1로 채워.
        // 16진수 0x8000(이진수 1000 0000 0000 0000)과 비트 AND(&) 연산을 해서 0이 아니면 "진짜 눌렸다!"라고 판단해.
        return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }
}

// [핵심 스캔 함수] 매 프레임(0.05초)마다 게임 엔진이 호출하여 최신 키보드 성적표(state)를 작성해 주는 함수야.
InputState Input::poll()
{
    // 1. [버퍼 청소 작업] 콘솔창에 키보드 입력이 쌓여서 버벅거리는 잔상 현상을 막기 위한 예외 처리야.
    // _kbhit()로 콘솔 입력 버퍼에 찌꺼기 키 신호가 남아있는지 확인하고, 남아있다면 _getch()로 전부 읽어서 강제로 비워버려(void 캐스팅).
    while (_kbhit())
    {
        (void)_getch();
    }

    // 최신 실시간 키보드 장부를 새로 개설해.
    InputState state;

    // ESC 키가 눌렸다면 게임 종료 플래그(quit)를 true로 세팅해.
    state.quit = isDown(VK_ESCAPE);

    // --- 1P 플레이어 실시간 이동 조작계 스캔 (W, S, A, D) ---
    state.p1Up = isDown('W');
    state.p1Down = isDown('S');
    state.p1Left = isDown('A');
    state.p1Right = isDown('D');

    // --- 2P 플레이어 실시간 이동 조작계 스캔 (방향키 Arrow Keys) ---
    state.p2Up = isDown(VK_UP);
    state.p2Down = isDown(VK_DOWN);
    state.p2Left = isDown(VK_LEFT);
    state.p2Right = isDown(VK_RIGHT);

    // --- 폭탄 및 아이템 액션 버튼 실시간 입력 수집 ---
    const bool space = isDown(VK_SPACE);                // 2P 폭탄 (스페이스 바)
    const bool enter = isDown(VK_SHIFT) || isDown(VK_CONTROL); // 1P 폭탄 (시프트 또는 컨트롤)
    const bool needle = isDown('N');                    // 1P, 2P 공용 바늘 아이템 (N 키)

    // 2 [핵심 알고리즘: 에지 트리거(Edge Trigger) 연사 방지 공식] 
    // "지금 이 순간 키를 눌렀고(&&), 바로 직전 프레임(prev)에는 안 눌렀던 상태"일 때만 딱 1번 true(발사)로 인정해 주는 공식이야.
    // 이 처리가 없으면 스페이스바를 0.2초만 꾹 누르고 있어도 4~5개의 폭탄이 한 칸에 겹쳐서 마구 설치되는 치명적인 버그가 터져.
    state.p1Bomb = enter && !prevEnter_;
    state.p2Bomb = space && !prevSpace_;
    state.p1Needle = needle && !prevNeedle_;
    state.p2Needle = needle && !prevNeedle_; // 공용 키이므로 동일하게 처리

    //3. [과거 기억 정산] 다음 프레임(0.05초 뒤)을 위해, "지금 누른 상태"를 "과거의 기억" 변수들에 안전하게 저장해 둬.
    prevSpace_ = space;
    prevEnter_ = enter;
    prevNeedle_ = needle;

    // 완성된 최신 실시간 입력 장부를 게임 엔진 코어에 리턴해 줘.
    return state;
}