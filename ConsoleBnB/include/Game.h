#pragma once
#include "Input.h"
#include "Map.h"
#include "Renderer.h"
#include "Types.h"
#include <random>
#include <vector>

/**
 * @brief 게임 전체 로직을 담당하는 핵심 클래스
 *
 * 메인 루프, 플레이어 업데이트, 폭탄/불꽃 처리,
 * 승패 판정, 화면 전환 등 모든 게임 로직을 관리한다.
 */
class Game
{
public:
    /**
     * @brief 생성자 - 플레이어 초기 상태 및 난수 생성기 초기화
     */
    Game();

    /**
     * @brief 게임 전체 실행 진입점
     * @return 프로그램 종료 코드 (정상 종료 시 0)
     *
     * setupConsole() → runMenu() → reset() → 게임 루프 → showResultScreen() 순으로 진행
     */
    int run();

private:
    // ───────────────────────────────────────────────
    // 멤버 변수
    // ───────────────────────────────────────────────

    GameMap map_;           ///< 17×13 타일 맵 (Wall / Block / Empty) 및 아이템 관리
    Renderer renderer_;     ///< Win32 GDI+ 기반 화면 렌더러
    Input input_;           ///< 키보드 입력 상태 수집기
    Player p1_;             ///< 1P 플레이어 (파란색, WASD 조작)
    Player p2_;             ///< 2P 플레이어 (빨간색, 방향키 조작)
    std::vector<Bomb> bombs_;   ///< 현재 맵에 설치된 폭탄 목록
    std::vector<Flame> flames_; ///< 현재 맵에 활성화된 불꽃(물줄기) 목록
    std::mt19937 rng_;          ///< 아이템 드롭 등에 사용하는 메르센 트위스터 난수 생성기
    GameResult result_ = GameResult::Running; ///< 현재 게임 결과 상태 (Running / P1Win / P2Win / Draw / Quit)
    MenuChoice menuChoice_ = MenuChoice::Start; ///< 메인 메뉴에서 현재 선택된 항목

    // ───────────────────────────────────────────────
    // 게임 흐름 제어
    // ───────────────────────────────────────────────

    /**
     * @brief 게임 시작 전 모든 상태를 초기 값으로 리셋
     *
     * 맵 재생성, 플레이어 위치·능력치 초기화, 폭탄·불꽃 벡터 클리어
     */
    void reset();

    /**
     * @brief 메인 메뉴 루프 실행
     * @return true: 게임 시작 선택, false: 나가기 선택(종료)
     *
     * 방향키/클릭으로 [시작 / 도움말 / 나가기] 중 선택
     */
    bool runMenu();

    /**
     * @brief 도움말 화면 표시
     *
     * ESC 또는 Enter 입력 시 메인 메뉴로 복귀
     */
    void showHelp();

    /**
     * @brief 게임 시작 전 카운트다운 연출
     *
     * 3 → 2 → 1 → 시작! 순서로 화면에 표시 후 게임 루프 진입
     */
    void playCountdown();

    /**
     * @brief 게임 결과 화면 표시
     * @return true: 메인 메뉴로 복귀, false: 게임 완전 종료
     *
     * 1P 승리 / 2P 승리 / 무승부 / 게임 종료 중 결과를 표시하고
     * Enter(메뉴 복귀) 또는 ESC(종료) 입력 대기
     */
    bool showResultScreen();

    // ───────────────────────────────────────────────
    // 게임 루프 업데이트
    // ───────────────────────────────────────────────

    /**
     * @brief 매 프레임 호출되는 게임 상태 전체 업데이트 함수
     * @param input 이번 프레임의 키 입력 상태
     *
     * updatePlayer → updateBombs → checkDeaths → updateFlames → checkResult 순으로 실행
     */
    void update(const InputState& input);

    /**
     * @brief 플레이어 1명의 상태 업데이트
     * @param player 업데이트할 플레이어 (p1_ 또는 p2_)
     * @param input 이번 프레임의 키 입력 상태
     *
     * - 갇힘(trapped) 상태: 바늘 사용 여부 확인 및 trappedFrames 카운트다운
     * - 일반 상태: 이동 입력(WASD/방향키) 및 폭탄 설치(Shift/Ctrl/Space) 처리
     * - 속도 버프(speedFrames) 카운트다운
     */
    void updatePlayer(Player& player, const InputState& input);

    /**
     * @brief 플레이어 이동 처리
     * @param player 이동할 플레이어
     * @param delta 이동 방향 벡터 (예: {0,-1} = 위, {1,0} = 오른쪽)
     *
     * 벽, 상자, 폭탄, 맵 경계, 상대 플레이어 위치를 체크하여 이동 가능 여부 판단
     * 상대가 갇힘 상태인 칸으로 이동 시 상대 즉시 사망 처리
     */
    void tryMove(Player& player, Vec2 delta);

    /**
     * @brief 플레이어 위치에 폭탄 설치 시도
     * @param player 폭탄을 설치하는 플레이어
     *
     * activeBombs < maxBombs 이고 해당 칸에 폭탄이 없을 때만 설치
     * 설치 시 activeBombs +1, 사운드 재생
     */
    void tryPlaceBomb(Player& player);

    /**
     * @brief 모든 폭탄의 타이머 감소 및 폭발 처리
     *
     * - 매 프레임 모든 폭탄의 timer를 1씩 감소
     * - timer ≤ 0이 된 폭탄은 explodeBomb() 호출
     * - 연쇄 폭발: 폭발 범위 내 다른 폭탄의 timer를 0으로 설정 후 while 루프 반복
     * - 폭발 완료된 폭탄(exploded == true)은 벡터에서 제거
     */
    void updateBombs();

    /**
     * @brief 특정 인덱스의 폭탄을 폭발시키기
     * @param index bombs_ 벡터 내 폭발할 폭탄의 인덱스
     *
     * - 상하좌우로 range만큼 불꽃(Flame) 셀 생성
     * - 벽(Wall)에서 전파 차단
     * - 상자(Block)에서 전파 차단 후 map_.breakBlock() 호출 (아이템 드롭)
     * - 설치자의 activeBombs -1
     * - 폭발 사운드 재생
     */
    void explodeBomb(size_t index);

    /**
     * @brief 모든 불꽃(물줄기)의 타이머 감소 및 소멸 처리
     *
     * - 매 프레임 모든 Flame의 timer를 1씩 감소
     * - timer ≤ 0이 된 Flame은 flames_ 벡터에서 제거
     */
    void updateFlames();

    /**
     * @brief 플레이어가 아이템 위에 있을 경우 아이템 효과 적용
     * @param player 아이템을 획득할 플레이어
     *
     * - BombCount: maxBombs +1 (최대 8)
     * - Range: range +1 (최대 8)
     * - Speed: speedFrames = 100 (이동 속도 증가)
     * - Needle: needles +1 (최대 3)
     * - 획득 후 해당 타일의 아이템을 None으로 초기화
     */
    void applyItems(Player& player);

    // ───────────────────────────────────────────────
    // 판정 함수
    // ───────────────────────────────────────────────

    /**
     * @brief 불꽃 위에 있는 플레이어를 갇힘(trapped) 상태로 전환
     *
     * - isOnFlame()으로 각 플레이어 위치 체크
     * - 불꽃 위에 있으면 trapped = true, trappedFrames = 100 설정
     * - 사망 사운드 재생
     */
    void checkDeaths();

    /**
     * @brief 현재 생존 상태를 기반으로 승패 판정
     *
     * - 두 플레이어 모두 alive == false → GameResult::Draw
     * - p1만 alive → GameResult::P2Win
     * - p2만 alive → GameResult::P1Win
     * - result_를 갱신하여 게임 루프 종료 트리거
     */
    void checkResult();

    // ───────────────────────────────────────────────
    // 사운드 함수 (Windows Beep API 사용)
    // ───────────────────────────────────────────────

    void soundMenu() const;      ///< 메뉴 이동 시 효과음
    void soundStart() const;     ///< 게임 시작 시 효과음
    void soundBomb() const;      ///< 폭탄 설치 시 효과음
    void soundExplosion() const; ///< 폭탄 폭발 시 효과음
    void soundItem() const;      ///< 아이템 획득 시 효과음
    void soundDeath() const;     ///< 플레이어 사망 시 효과음

    // ───────────────────────────────────────────────
    // 유틸리티 함수
    // ───────────────────────────────────────────────

    /**
     * @brief 특정 위치에 폭탄이 존재하는지 확인
     * @param p 확인할 좌표
     * @return 해당 위치에 아직 폭발하지 않은 폭탄이 있으면 true
     *
     * 이동 가능 여부 판단에 사용 (폭탄은 이동 불가 타일로 취급)
     */
    bool blockedByBomb(Vec2 p) const;

    /**
     * @brief 특정 위치에 불꽃(물줄기)이 존재하는지 확인
     * @param p 확인할 좌표
     * @return 해당 위치에 활성 불꽃이 있으면 true
     *
     * checkDeaths()에서 갇힘 판정에 사용
     */
    bool isOnFlame(Vec2 p) const;
};
