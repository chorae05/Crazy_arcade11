#include "../include/Game.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <thread>

// 익명 네임스페이스: 이 파일(.cpp) 내부에서만 은밀하게 사용하는 보조 이동 연산 유틸리티들이야.
namespace
{
    // 플레이어의 이동 속도 프레임 딜레이를 정산하는 함수야. (신발 아이템을 먹었다면 4프레임, 기본은 7프레임 대기)
    int moveDelayFor(const Player& player)
    {
        return player.speedFrames > 0 ? 4 : 7;
    }

    // 1P와 2P의 조작 키 입력을 판정해 가로축 이동 벡터 값(-1, 0, 1)을 산출해 주는 함수야.
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

    // 1P와 2P의 조작 키 입력을 판정해 세로축 이동 벡터 값(-1, 0, 1)을 산출해 주는 함수야.
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

// [생성자] 메르센 트위스터 난수 엔진에 초기 랜덤 하드웨어 시드를 주입하고 초기화(reset)를 가동해.
Game::Game()
    : rng_(std::random_device{}())
{
    reset();
}

// =================================================================
// 1. 메인 고정 프레임 고속 게임 루프 (엔진 심장박동)
// =================================================================
int Game::run()
{
    renderer_.setupConsole(); // 1000x800 그래픽 가상 윈도우 창 띄우기 오더 발주

    while (!renderer_.windowClosed())
    {
        menuChoice_ = MenuChoice::Start;
        if (!runMenu()) // 1. 메인 타이틀 로비 화면을 실행하고, 유저가 '나가기'를 누르면 전원을 꺼버려.
        {
            break;
        }

        reset();          // 새로운 판이 열렸으니 맵 데이터와 플레이어 스탯 리셋 정산
        playCountdown();  // 게임 몰입도를 높이는 "준비! 3... 2... 1... 시작!" 애니메이션 및 비프음 가동

        // 2 [고정 프레임 타임라인 제어 장치] 초당 20프레임(20FPS)의 일정한 심장박동 속도를 자로 재듯 동기화해.
        using clock = std::chrono::steady_clock;
        const auto frameTime = std::chrono::milliseconds(1000 / FPS); // 1프레임당 정밀 허용 시간 마진 (50ms)

        while (result_ == GameResult::Running)
        {
            const auto frameStart = clock::now(); // 현 프레임 시작점 시간 낚아채기

            // 안전장치: 사용자가 화면 우측 상단 X 버튼을 누르는 순간 루프를 즉시 격리 이탈해.
            if (!renderer_.pumpMessages() || renderer_.windowClosed())
            {
                result_ = GameResult::Quit;
                break;
            }

            //  아케이드 게임 루프 3대장 마스터 핵심 사이클] 
            const InputState input = input_.poll();                    // 1. 입력 감지: 조이스틱 키보드 실시간 수집
            update(input);                                             // 2. 물리 연산: 위치 이동, 충돌 검사, 폭탄 폭발 타이머 정산
            renderer_.draw(map_, p1_, p2_, bombs_, flames_, result_);  // 3. 화면 렌더링: 더블 버퍼링 도화지에 쾅! 쏴주기

            // 2. [정밀 슬립 프레임 보정 수식] 컴퓨터 연산 속도가 너무 빨라 게임이 순간이동하는 버그를 완벽 방어하는 하드웨어 홀딩 장치야.
            const auto elapsed = clock::now() - frameStart;
            if (elapsed < frameTime)
            {
                // 50ms 중 연산하고 남은 보너스 여유 시간만큼만 스레드를 휴면(sleep_for)시켜 일정한 배속을 칼같이 조율해!
                std::this_thread::sleep_for(frameTime - elapsed);
            }
        }

        // 경기가 끝난 후 최종 승리자 전광판 보드를 띄우고 사용자의 다음 엔터/ESC 선택을 대기해.
        if (!showResultScreen())
        {
            break;
        }
    }

    return 0;
}

// 매 판이 새로 갱신될 때마다 기존 물풍선/물줄기 리스트를 깨끗이 청소하고, 1P(좌상단)와 2P(우하단)의 초기 태생 스탯을 정산하는 함수야.
void Game::reset()
{
    map_.reset(); // 미로 상자 다시 배치 및 플레이어 전용 6칸 스폰 안전구역 밀어버리기
    bombs_.clear();
    flames_.clear();

    // 초기 스탯 세팅: Player{ id, 문자, 초기격자좌표, alive, maxBombs, range, needles, ... }
    p1_ = Player{ 1, 'P', { 1, 1 }, true, 2, 3, 0, 0, 0, 0 };
    p2_ = Player{ 2, 'Q', { MAP_W - 2, MAP_H - 2 }, true, 2, 3, 0, 0, 0, 0 };
    result_ = GameResult::Running; // 게임 진행 상태를 진행 중으로 전환
}

// [로비 대기 루프 펑션] 메인 메뉴 창에서 사용자가 키보드나 마우스 클릭을 통해 메뉴를 바꿀 수 있게 입력 흐름을 홀딩하는 차단 루프야.
bool Game::runMenu()
{
    // 입력 중복 가속 방지를 위한 직전 에지 기억 플래그 초기화
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

        renderer_.drawMainMenu(menuChoice_); // 로비 그래픽 사각형 박스와 글씨 그리기
        MenuChoice clickedChoice = menuChoice_;
        const bool clicked = renderer_.consumeMenuClick(clickedChoice); // 마우스 클릭 픽셀 충돌 연산 수행
        if (clicked)
        {
            menuChoice_ = clickedChoice; // 마우스로 누른 항목을 현재 선택값으로 동기화해.
        }

        // 실시간 로비 조작 키 감지
        const bool up = (GetAsyncKeyState(VK_UP) & 0x8000) != 0 || (GetAsyncKeyState('W') & 0x8000) != 0;
        const bool down = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0 || (GetAsyncKeyState('S') & 0x8000) != 0;
        const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        // 에지 트리거 분기: 위 키를 누르는 순간 메뉴 선택 단추를 위로 회전하고 사운드를 찌릿 쏴줘!
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
        // 엔터키를 누르거나 마우스로 진짜 버튼을 쾅 클릭했을 때 작동하는 결정 분기점이야.
        if ((enter && !prevEnter) || clicked)
        {
            soundStart();
            if (menuChoice_ == MenuChoice::Start)
            {
                return true; // 게임 인게임 월드로 가자!
            }
            if (menuChoice_ == MenuChoice::Help)
            {
                showHelp(); // 도움말 모드 칠판 열기
            }
            if (menuChoice_ == MenuChoice::Exit)
            {
                return false; // 안녕히 가세요 전원 종료!
            }
        }
        if (esc && !prevEsc)
        {
            return false;
        }

        // 과거 에지 백업 및 과부하 방지용 미세 슬립 거치
        prevUp = up;
        prevDown = down;
        prevEnter = enter;
        prevEsc = esc;
        Sleep(70);
    }
}

// 도움말 화면 전용 루프 함수야. 엔터나 ESC를 다시 누를 때까지 렌더러의 drawHelp를 화면에 무한 유지해 줘.
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
            return; // 조작법 다 읽었으니 다시 로비 메인 메뉴로 퇴근!
        }

        prevEnter = enter;
        prevEsc = esc;
        Sleep(70);
    }
}

// 인게임 시작 직전 사운드 하드웨어 비프음을 조합해 레트로 멜로디 3... 2... 1... 스타트 연출을 처리하는 함수야.
void Game::playCountdown()
{
    for (int count = 3; count >= 0; --count)
    {
        renderer_.drawCountdown(count);
        // 하드웨어 메인보드 비프음 주파수를 활용해 카운트다운 소리를 정산해. (3,2,1은 낮게, 시작은 하이톤 음역대로 삐-!)
        Beep(count > 0 ? 620 : 920, count > 0 ? 140 : 230);
        Sleep(count > 0 ? 720 : 520);
    }
}

// 매치 결과를 화면에 고정해 두고 유저가 엔터를 눌러 재경기를 할지, ESC를 눌러 타이틀로 나갈지 결정하는 함수야.
bool Game::showResultScreen()
{
    bool prevEnter = true;
    bool prevEsc = true;

    while (!renderer_.windowClosed())
    {
        renderer_.pumpMessages();
        renderer_.drawResultScreen(result_); // 1P승/2P승/무승부 칠판 그리기

        const bool enter = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 || (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if ((enter && !prevEnter) || (esc && !prevEsc))
        {
            return enter && !prevEnter; // 엔터를 누르면 true를 뱉어 즉시 리매치 재경기를 단행해 줘!
        }

        prevEnter = enter;
        prevEsc = esc;
        Sleep(50);
    }

    return false;
}

// =================================================================
// 2. 인게임 핵심 마스터 업데이트 파이프라인 (연산 총괄 총감독)
// =================================================================
// 매 프레임(0.05초)마다 엔진 심장이 뛰면 제일 먼저 실행되는 로직 연산 종합 세트야. 순서가 밀리면 그래픽 레이어가 꼬여버려!
void Game::update(const InputState& input)
{
    if (input.quit) // ESC 누르면 즉시 게임판 정산 단계로 이탈 오더
    {
        result_ = GameResult::Quit;
        return;
    }

    // [로직 연산 우선순위 파이프라인 패키지] 
    updatePlayer(p1_, input); // 1. 플레이어 1 연산 (이동, 폭탄 설치 검사)
    updatePlayer(p2_, input); // 2. 플레이어 2 연산
    updateBombs();           // 3. 물풍선 타이머 차감 및 대망의 연쇄 폭발 촉발 처리
    checkDeaths();           // 4. 캐릭터 피격 검사: 뿜어져 나온 물줄기 좌표에 캐릭터가 닿았는지 선검사해 갇힘 상태로 변이
    updateFlames();          // 5. 방출된 물줄기(Flame) 불꽃 수명 타이머 차감 및 가변 vector 소멸 정산
    checkResult();           // 6. 실시간 생존 플래그 스캔 후 승패 버킷 확정 판정
}

// 플레이어 개체 하나의 쿨타임, 신발 지속시간, 설치 입력, 이동 검사를 총괄 수치 정산하는 함수야.
void Game::updatePlayer(Player& player, const InputState& input)
{
    if (!player.alive)
    {
        return; // 탈락한 플레이어는 키 입력 조작을 원천 입구 컷 시켜버려.
    }

    // 플레이어 고유 넘버에 따라 1P(WASD/Shift) 및 2P(방향키/Space) 액션 단추 분크 매핑 매칭
    const bool placeBomb = (player.id == 1) ? input.p1Bomb : input.p2Bomb;
    const bool useNeedle = (player.id == 1) ? input.p1Needle : input.p2Needle;

    // --- 피격 상태 연산 부문: 물풍선 방울에 물방울 갇힘(trapped) 상태 스태이트 전개 ---
    if (player.trapped)
    {
        // 예외 처리 탈출벽: 만약 노란 바늘 아이템이 있고 유저가 침 키(N)를 눌렀다면? 바늘 소거하고 자력 부활 탈출 정산!
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
            --player.trappedFrames; // 갇힘 제한 시간 숨 참기 타이머 차감 연산
        }
        if (player.trappedFrames <= 0)
        {
            player.alive = false; // 숨 참기 프레임이 0이 되는 순간 사망 탈락(alive = false) 처리!
            soundDeath();
        }
        return; // 갇혀있는 동안에는 이동이나 추가 폭탄 설치를 못 하도록 강제로 이행 코드를 탈출시켜 차단해.
    }

    // 캐릭터 내부 물리 프레임 카운터 차감 연산
    if (player.moveCooldown > 0)
    {
        --player.moveCooldown; // 이동 제약 쿨타임 프레임 빼기
    }
    if (player.speedFrames > 0)
    {
        --player.speedFrames;  // 신발 아이템 스피드업 보너스 잔여 지속시간 빼기
    }

    // 폭탄 거치 명령 접수 시 처리부로 토스
    if (placeBomb)
    {
        tryPlaceBomb(player);
    }

    // --- 이동 벡터 연산 부문: 쿨타임이 0이 되는 순간에만 1칸 격자 이동 허용 ---
    if (player.moveCooldown == 0)
    {
        Vec2 delta{ keyMoveX(input, player.id), keyMoveY(input, player.id) };

        // 대각선 이동 제어 알고리즘: 가로축 조작 입력 신호가 있다면 세로축 이동 벡터를 강제로 0으로 오염시켜 십자 이동만 보장해!
        if (delta.x != 0)
        {
            delta.y = 0;
        }

        // 실제 이동 입력 변화량(delta)이 발생했다면 충돌 테스트를 단행해 좌표를 밀어줘.
        if (delta.x != 0 || delta.y != 0)
        {
            tryMove(player, delta);
            player.moveCooldown = moveDelayFor(player); // 내 이동 속도 능력치 스탯 수치만큼 쿨타임 재충전 거치
        }
    }
}

// 캐릭터가 다음으로 밟고 나갈 타일 격자에 물리적 장애물이 없는지 선제 검증하는 충돌 처리 함수야.
void Game::tryMove(Player& player, Vec2 delta)
{
    const Vec2 next{ player.pos.x + delta.x, player.pos.y + delta.y };
    Player& other = player.id == 1 ? p2_ : p1_;

    // 충돌 처리 1: 만약 내가 가려는 칸에 살아있는 상대방 캐릭터가 떡하니 서 있다면?
    if (other.alive && other.pos == next)
    {
        // 크레이지 아케이드 정석 규칙: 만약 상대방이 물방울 갇힘(trapped) 상태인데 내가 몸으로 밟고 지나가려고 한다면 즉시 즉사 킬(Kill) 처리를 단행해!
        if (other.trapped)
        {
            other.alive = false;
            other.trapped = false;
            soundDeath();
        }
        return; // 상대방이 멀쩡한 상태라면 육체 충돌이므로 전진을 거부하고 입구 컷 해버려.
    }

    // 충돌 처리 2: 가려는 바둑판 칸이 걸어갈 수 없는 단단한 벽/상자이거나, 다른 물풍선 이미 거치되어 막혀있다면 전진 거부!
    if (!map_.isWalkable(next) || blockedByBomb(next))
    {
        return;
    }

    // 모든 보안 검색대를 무사히 통과했다면 실제 캐릭터의 영구 좌표 벡터를 변경해!
    player.pos = next;
    applyItems(player); // 새로 밟은 자리에 보물 아이템이 있다면 인벤토리 스탯에 즉시 영구 영양분 주입!
}

// 유저 내 발밑 격자 자리에 물풍선 폭탄 인스턴스 객체를 동적으로 밀어 넣어 스폰시키는 함수야.
void Game::tryPlaceBomb(Player& player)
{
    // 예외 처리: 내가 가질 수 있는 최대 설치 가능 개수 제한을 초과했거나, 이미 발밑에 폭탄이 깔려있다면 중복 설치 차단!
    if (player.activeBombs >= player.maxBombs || blockedByBomb(player.pos))
    {
        return;
    }

    // 가변 STL vector 리스트 메모리 공간에 동적으로 실물 물풍선 폭탄 구조체 개체를 push_back 밀어 넣어 거치해!
    bombs_.push_back(Bomb{ player.pos, player.id, BOMB_FRAMES, player.range, false });
    ++player.activeBombs; // 플레이어가 필드에 깔아둔 활성화 폭탄 카운터 스케일 1 업!
    soundBomb();
}

// [물풍선 감시 메인 타임라인 제어기] 매 프레임 폭탄 타이머 숫자를 빼주다가, 시간이 다 된 폭탄이 있으면 폭발을 촉발하는 함수야.
void Game::updateBombs()
{
    for (Bomb& bomb : bombs_)
    {
        if (!bomb.exploded)
        {
            --bomb.timer; // 째깍째깍 수명 프레임 빼기 차감 연산
        }
    }

    // [최고 최중요 킬러 알고리즘: 연쇄 폭발 촉발 반복 루프 구조]
    // 하나의 물풍선이 펑 터지면서 옆에 있던 다른 물풍선의 도화선 타이머를 강제로 0으로 밀어버렸을 때,
    // 그 연쇄 폭탄까지 도미노처럼 도미노 식으로 연달아 연쇄적으로 연속 폭발시키도록 유기적으로 추적해 내는 루프 메커니즘이야!
    bool explodedAny = true;
    while (explodedAny)
    {
        explodedAny = false;
        for (size_t i = 0; i < bombs_.size(); ++i)
        {
            // 타이머 수명이 다 되었는데 아직 exploded 플래그가 안 켜진 폭탄 원천 발굴!
            if (!bombs_[i].exploded && bombs_[i].timer <= 0)
            {
                explodeBomb(i);     // 대망의 폭사 불꽃 방출 함수 가동!
                explodedAny = true; // 연쇄 폭발이 한 건이라도 발생했다면 한 번 더 루프를 돌며 연쇄 연쇄 전이 대상을 재전수조사해!
            }
        }
    }

    // 연산 처리가 완벽히 끝나 연쇄 박살이 끝난 잔해 플래그(`bomb.exploded == true`) 폭탄 인스턴스들은
    // 현대적인 C++ STL 자원 해제 기법인 'Erase-Remove_if 이디엄'을 사용하여 가변 vector 메모리 공간에서 칼같이 소멸 정산해 내 오버헤드를 막는단다!
    bombs_.erase(
        std::remove_if(
            bombs_.begin(),
            bombs_.end(),
            [](const Bomb& bomb) { return bomb.exploded; }),
        bombs_.end());
}

// [십자 폭발 연쇄 매핑 커널 함수] 하나의 물풍선 주소록을 뜯어 동·서·남·북 4방향으로 화력을 뿜어내고 상자를 깨부수는 핵심 알고리즘이야!
void Game::explodeBomb(size_t index)
{
    if (index >= bombs_.size() || bombs_[index].exploded)
    {
        return;
    }

    Bomb& source = bombs_[index];
    source.exploded = true; // 현재 폭탄 연산 완료 도장 쾅!

    // 주인 캐릭터 개체 추적해 설치 가능 가용 누적 개수 인벤토리를 다시 반환 보정해 줘.
    Player& owner = source.ownerId == 1 ? p1_ : p2_;
    owner.activeBombs = std::max(0, owner.activeBombs - 1);

    // 물줄기(Flame) 불꽃 이펙트 인스턴스를 하나 신설하고 우선 내 폭탄 중심부 좌표를 스택에 적재해.
    Flame flame;
    flame.cells.push_back(source.pos);

    // 4방향 나침반 주소 주소록 배열 세팅 { 동, 서, 남, 북 }
    const Vec2 directions[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (Vec2 dir : directions)
    {
        // 내 화력 사거리 스탯 수치(`source.range`)만큼 한 칸씩 점진적으로 뻗어 나가면서 전수 검사를 실행해!
        for (int step = 1; step <= source.range; ++step)
        {
            const Vec2 p{ source.pos.x + dir.x * step, source.pos.y + dir.y * step };
            const Tile tile = map_.tileAt(p); // 뻗어 나간 좌표에 무슨 타일이 놓여있는지 맵 레이어 속성 스캔

            // 케이스 1: 파괴가 불가능한 외곽 콘크리트 단단한 벽(Wall)을 때렸다면?
            if (tile == Tile::Wall)
            {
                break; // 물줄기 화력이 벽 기둥을 뚫지 못하므로 이 방향은 즉시 연산을 올스톱하고 뻗어 나감을 종료해버려!
            }

            // 벽이 아니므로 현재 도달한 이 좌표는 진짜 불꽃이 흐르는 치명적 구역 목록(`flame.cells`)에 영구 보관해 놔!
            flame.cells.push_back(p);

            // [연쇄 폭발 크로스 링크 연산] 만약 내가 쏜 물줄기가 뻗어 나간 칸(p) 자리에 다른 대기 중인 물풍선이 떡하니 놓여있다면?
            for (Bomb& bomb : bombs_)
            {
                if (!bomb.exploded && bomb.pos == p)
                {
                    bomb.timer = 0; // 거창한 공식 필요 없이 그 녀석의 도화선 남은 타이머를 즉시 강제로 0으로 강탈 동기화해 버려! (바로 다음 연쇄 루프에서 터지게 됨)
                }
            }

            // 케이스 2: 부서지는 나무 상자 타일(Block)을 때렸다면?
            if (tile == Tile::Block)
            {
                map_.breakBlock(p, rng_); // 상자를 박살 내 빈길로 만들고 메르센 트위스터 확률 주사위를 돌려 4대 아이템을 스폰 시켜!
                break;                    // 나무 상자가 고압 물줄기를 한 번 흡수하고 부서지는 정석 규칙이므로 이 방향 물줄기는 여기서 전진을 종료해.
            }
        }
    }

    flames_.push_back(flame); // 연산 조립이 끝난 종합 십자 물줄기 이펙트 팩을 글로벌 연산 리스트 메모리에 안착시켜.
    soundExplosion();         // 우르릉 쾅 효과음 비프음 송출!
}

// 화면에 피어오른 세찬 푸른 물줄기 불꽃들의 지속시간 수명 타이머를 차감하고, 다 타버린 이펙트는 vector에서 지워 없애는 정산 함수야.
// 원래 코드: void Renderer::updateFlames() 
void Game::updateFlames() 
{
    for (Flame& flame : flames_)
    {
        --flame.timer; // 불꽃 소멸 프레임 차감 연산
    }

    // 수명이 다 된(timer <= 0) 이펙트 잔해들은 Erase-Remove_if로 깔끔하게 청소해 메모리 누수를 원천 방어해 줘.
    flames_.erase(
        std::remove_if(
            flames_.begin(),
            flames_.end(),
            [](const Flame& flame) { return flame.timer <= 0; }),
        flames_.end());
}

// 플레이어가 발밑에 놓인 보물 상자 아이템을 먹었을 때 인벤토리 능력치 스탯 수치를 즉시 보너스 영구 스케일업 반영하는 실시간 섭취 함수야.
void Game::applyItems(Player& player)
{
    const ItemType item = map_.itemAt(player.pos); // 현재 내 밟고 있는 바닥에 숨겨진 보물이 있는지 스캔
    if (item == ItemType::None)
    {
        return; // 빈 땅바닥이면 아무 일도 안 하고 철수.
    }

    // 아이템 종류 번호 매칭 스위치 분기문 가동
    switch (item)
    {
    case ItemType::Speed: // 초록 번개 장화 장착!
        player.speedFrames = SPEED_FRAMES; // 일정 시간 동안 기동 대기 프레임을 단축시키는 지속시간 버프 충전
        break;
    case ItemType::Range: // 분홍 물약 장착!
        player.range = std::min(8, player.range + 1); // 십자 폭발 물줄기 사거리를 단번에 +1칸 영구 증폭 (최대 8칸 한계 마진 캡 적용)
        break;
    case ItemType::BombCount: // 파란 별 장착!
        player.maxBombs = std::min(8, player.maxBombs + 1); // 필드에 동시에 깔 수 있는 폭탄 한도 상한치 누적 스탯을 +1 영구 증폭 (최대 8개 한계)
        break;
    case ItemType::Needle: // 황금 바늘 침 장착!
        player.needles = std::min(3, player.needles + 1);   // 물풍선 피격 시 자력 부활 탈출이 가능한 노란 바늘 보유 개수를 +1 증폭 (최대 3개 적재)
        break;
    case ItemType::None:
        break;
    }

    map_.setItem(player.pos, ItemType::None); // 플레이어가 아이템 보물을 꿀꺽 먹었으니 바닥 지도 데이터를 깨끗한 빈 바닥(None)으로 갱신 정산해 줘!
    soundItem(); // 띠로링 보물 섭취 아이템 효과음 송출
}

// [피격 연산 수신기] 실시간으로 활성화되어 타오르는 물줄기 불꽃 좌표 위에 캐릭터가 육체적으로 발을 들이밀었는지 엄격히 선검사하는 충돌 센서 함수야.
void Game::checkDeaths()
{
    // 1P 검사: 멀쩡히 생존 중인 1P의 주소 위치가 현재 타오르는 불꽃 비디오 목록 레이어(`isOnFlame`)에 정확히 오버랩 겹쳤다면?
    if (p1_.alive && !p1_.trapped && isOnFlame(p1_.pos))
    {
        p1_.trapped = true; // 1P 상태를 즉시 물방울 물풍선 갇힘 스태이트 상태로 변이 시켜버려!
        p1_.trappedFrames = TRAP_FRAMES; // 제한 시간 숨 참기 타이머 리필 거치
        soundDeath(); // 꾸르륵 피격 효과음 송출
    }
    // 2P 검사: 동일 메커니즘 연산 작동
    if (p2_.alive && !p2_.trapped && isOnFlame(p2_.pos))
    {
        p2_.trapped = true;
        p2_.trappedFrames = TRAP_FRAMES;
        soundDeath();
    }
}

// 매 프레임 두 캐릭터의 생존alive 상태 성적표를 전수조사해서 인게임의 매치 결론 버킷을 최종 도장 찍는 판정 함수야.
void Game::checkResult()
{
    // 케이스 1: 둘 다 장렬히 전사했거나 동시에 폭사했다면?
    if (!p1_.alive && !p2_.alive)
    {
        result_ = GameResult::Draw; // 공평하게 "무승부" 보드로 매치 결론 종지부!
    }
    // 케이스 2: 1P가 죽고 2P만 살아남았다면?
    else if (!p1_.alive)
    {
        result_ = GameResult::P2Win; // "2P 최종 승리" 버킷 확정!
    }
    // 케이스 3: 2P가 죽고 1P만 살아숨쉬고 있다면?
    else if (!p2_.alive)
    {
        result_ = GameResult::P1Win; // "1P 최종 승리" 버킷 확정!
    }
}

// =================================================================
// 3. Win32 메인보드 사운드 하드웨어 제어 주파수 이펙트 함수군 (오디오 팩)
// =================================================================
// 별도의 외부 오디오 에셋 mp3 종속성을 완전히 오프라인 격리하기 위해 Windows 메인보드 내부 마이크로 스피커 칩셋에 직접 특정 헤르츠(Hz) 주파수 진동 오더를 쏴서 소리를 연산해 낸 센스 있는 오디오 펑션 규격들이야!

void Game::soundMenu() const { Beep(520, 35); }  // 메뉴 전환 시 틱 째깍하는 미세 경량 주파수 발송
void Game::soundStart() const { Beep(740, 80); }  // 결정 단추 단추 클릭 시 명쾌한 하이톤 쏘기
void Game::soundBomb() const { Beep(430, 55); }  // 폭탄 거치 시 퉁- 하는 기단 주파수 연산
void Game::soundExplosion() const { Beep(160, 90); Beep(220, 45); } // 고압 폭발 연쇄 시 어두운 저음역대 묵직한 이중 주파수 결합 폭사음 발송
void Game::soundItem() const { Beep(880, 55); }  // 보물 보석함 상자 습득 시 뾱- 하는 초고속 상승 주파수 발송
void Game::soundDeath() const { Beep(260, 120); Beep(180, 140); } // 갇히거나 사망 시 음역대가 아래로 수직 하강하는 슬픈 단조 비프음 조합 송출

// =================================================================
// 4. 레이어 충돌 물리 사전 확인 전수조사 스캐너 도구들
// =================================================================

// 지정한 좌표(p) 칸 위에 "아직 안 터지고 대기 타며 째깍거리는 실제 폭탄"이 점유 중인지 감지하는 벽 센서야.
bool Game::blockedByBomb(Vec2 p) const
{
    for (const Bomb& bomb : bombs_)
    {
        if (!bomb.exploded && bomb.pos == p)
        {
            return true; // 물풍선 폭탄체가 이 바둑판 주소에 실존하므로 전진 불가 물리 벽 레이어로 판정해!
        }
    }
    return false;
}

// 지정한 좌표(p) 자리에 "펑! 하고 폭발하여 세차게 방출 중인 푸른 고압 물줄기"가 한 가닥이라도 점유하며 흐르고 있는지 체크하는 치명적 킬 존 센서야.
bool Game::isOnFlame(Vec2 p) const
{
    for (const Flame& flame : flames_)
    {
        for (Vec2 cell : flame.cells)
        {
            if (cell == p)
            {
                return true; // 죽음의 불꽃 십자 폭사 범위에 물리적으로 오버랩 적중했으므로 피격 대상 판정 true 발송!
            }
        }
    }
    return false;
}