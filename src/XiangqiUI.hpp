#pragma once

#include "Database.hpp"
#include "XiangqiEngine.hpp"
#include "XiangqiTypes.hpp"
#include "raylib.h"

#include <cstdint>
#include <vector>

class XiangqiUI {
public:
    static constexpr int WINDOW_WIDTH = 980;
    static constexpr int WINDOW_HEIGHT = 760;
    static constexpr int CELL_SIZE = 66;
    static constexpr int BOARD_OFFSET_X = 72;
    static constexpr int BOARD_OFFSET_Y = 82;

    XiangqiUI();
    ~XiangqiUI();

    void Update();
    void Draw() const;

private:
    void LoadChineseFont();
    void StartNewGame(GameMode mode);
    void Undo();
    void ExecuteMove(const Move& move);
    void ToggleReplayMode();
    bool GetBoardPosFromMouse(Pos& outPos) const;

    void DrawBoard(const BoardState& viewState, const Move* lastMovePtr) const;
    void DrawPiece(const Piece& piece, int row, int col) const;
    void DrawSidePanel() const;
    void DrawTextCn(const char* text, float x, float y, float fontSize, Color color) const;

    Database db_;
    Font font_{};
    bool customFontLoaded_ = false;

    std::int64_t currentGameId_ = -1;
    BoardState state_{};
    std::vector<Move> legalMoves_;
    std::vector<SavedMove> history_;
    std::vector<BoardState> stateStack_; // 用于悔棋恢复每步前局面

    Pos selected_{-1, -1};
    GameMode mode_ = GameMode::PvE;
    GameResult result_ = GameResult::Ongoing;
    GameStats stats_{};
    float aiTimer_ = 0.0f;

    // SQLite 历史复盘模式状态
    bool replayMode_ = false;
    std::vector<SavedMove> replayMoves_;
    int replayIndex_ = 0; // 0 = 初始开局, 1..N = 第N步后局面
};