#include "XiangqiUI.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr Color WINDOW_BG        = {34, 37, 43, 255};
constexpr Color BOARD_WOOD       = {224, 186, 128, 255};
constexpr Color BOARD_LINE_COLOR = {70, 44, 24, 255};
constexpr Color PANEL_BG         = {45, 49, 58, 255};
constexpr Color PIECE_FILL       = {245, 224, 182, 255};
constexpr Color PIECE_BORDER     = {138, 92, 46, 255};
constexpr Color RED_PIECE_INK    = {185, 28, 28, 255};
constexpr Color BLACK_PIECE_INK  = {28, 30, 34, 255};

const char* PieceGlyph(const Piece& p) {
    if (p.side == Side::Red) {
        switch (p.type) {
            case PieceType::King:     return "帥";
            case PieceType::Advisor:  return "仕";
            case PieceType::Elephant: return "相";
            case PieceType::Horse:    return "傌";
            case PieceType::Chariot:  return "俥";
            case PieceType::Cannon:   return "炮";
            case PieceType::Soldier:  return "兵";
            default: return "";
        }
    }
    switch (p.type) {
        case PieceType::King:     return "將";
        case PieceType::Advisor:  return "士";
        case PieceType::Elephant: return "象";
        case PieceType::Horse:    return "馬";
        case PieceType::Chariot:  return "車";
        case PieceType::Cannon:   return "砲";
        case PieceType::Soldier:  return "卒";
        default: return "";
    }
}

} // namespace

XiangqiUI::XiangqiUI() {
    LoadChineseFont();
    db_.Init("xiangqi.db");
    stats_ = db_.GetStats();

    // 优先从 SQLite 恢复上次未完成的对局
    std::int64_t savedId = -1;
    GameMode savedMode = GameMode::PvE;
    BoardState savedState{};
    std::vector<SavedMove> savedMoves;

    if (db_.LoadLatestUnfinishedGame(savedId, savedMode, savedState, savedMoves)) {
        currentGameId_ = savedId;
        mode_ = savedMode;
        state_ = savedState;
        history_ = std::move(savedMoves);

        stateStack_.clear();
        stateStack_.push_back(XiangqiEngine::CreateInitialState());
        for (size_t i = 0; i + 1 < history_.size(); ++i) {
            stateStack_.push_back(history_[i].stateAfter);
        }

        legalMoves_ = XiangqiEngine::GenerateLegalMoves(state_);
        result_ = XiangqiEngine::EvaluateResult(state_, legalMoves_);
    } else {
        StartNewGame(GameMode::PvE);
    }
}

XiangqiUI::~XiangqiUI() {
    if (customFontLoaded_) {
        UnloadFont(font_);
    }
}

void XiangqiUI::LoadChineseFont() {
    // 只抽取本游戏所有用到的 ASCII + 中文码点，加载速度极快
    const char* allGlyphs =
        " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz[]^_{}|~"
        "帥仕相傌俥炮兵將士象馬車砲卒"
        "楚河漢界一二三四五六七八九進退平"
        "中国象棋存档复盘版人机对战双人红方黑方走子将死和棋"
        "局数胜负总记录步当前第回放中退出新开悔切模式数据库";

    int rawCount = 0;
    int* rawCodepoints = LoadCodepoints(allGlyphs, &rawCount);
    std::vector<int> uniqueCodepoints;
    uniqueCodepoints.reserve(rawCount);
    for (int i = 0; i < rawCount; ++i) {
        if (std::find(uniqueCodepoints.begin(), uniqueCodepoints.end(), rawCodepoints[i]) == uniqueCodepoints.end()) {
            uniqueCodepoints.push_back(rawCodepoints[i]);
        }
    }
    UnloadCodepoints(rawCodepoints);

    const char* candidates[] = {
        "C:/Windows/Fonts/simkai.ttf", // 楷体（最适合象棋）
        "C:/Windows/Fonts/simhei.ttf", // 黑体
        "C:/Windows/Fonts/msyh.ttc"    // 微软雅黑
    };

    for (const char* path : candidates) {
        if (FileExists(path)) {
            font_ = LoadFontEx(path, 42, uniqueCodepoints.data(), static_cast<int>(uniqueCodepoints.size()));
            if (font_.texture.id > 0) {
                SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);
                customFontLoaded_ = true;
                return;
            }
        }
    }

    font_ = GetFontDefault();
    customFontLoaded_ = false;
}

void XiangqiUI::DrawTextCn(const char* text, float x, float y, float fontSize, Color color) const {
    DrawTextEx(font_, text, {x, y}, fontSize, 1.0f, color);
}

void XiangqiUI::StartNewGame(GameMode mode) {
    replayMode_ = false;
    mode_ = mode;
    state_ = XiangqiEngine::CreateInitialState();
    legalMoves_ = XiangqiEngine::GenerateLegalMoves(state_);
    result_ = GameResult::Ongoing;
    history_.clear();
    stateStack_.clear();
    selected_ = {-1, -1};
    aiTimer_ = 0.0f;

    currentGameId_ = db_.CreateGame(mode_, state_);
    stats_ = db_.GetStats();
}

void XiangqiUI::ExecuteMove(const Move& move) {
    BoardState before = state_;
    std::string note = XiangqiEngine::MoveToChineseNotation(before, move);

    stateStack_.push_back(before);
    XiangqiEngine::MakeMove(state_, move);

    int ply = static_cast<int>(history_.size()) + 1;
    history_.push_back({ply, move, note, state_});
    db_.RecordMove(currentGameId_, ply, move, note, state_);

    legalMoves_ = XiangqiEngine::GenerateLegalMoves(state_);
    result_ = XiangqiEngine::EvaluateResult(state_, legalMoves_);

    if (result_ != GameResult::Ongoing) {
        db_.UpdateGameResult(currentGameId_, result_);
        stats_ = db_.GetStats();
    }

    selected_ = {-1, -1};
    aiTimer_ = 0.0f;
}

void XiangqiUI::Undo() {
    if (replayMode_ || history_.empty()) return;

    int steps = 1;
    if (mode_ == GameMode::PvE) {
        if ((state_.turn == Side::Red || result_ == GameResult::BlackWin) && history_.size() >= 2) {
            steps = 2;
        }
    }

    for (int i = 0; i < steps && !history_.empty(); ++i) {
        state_ = stateStack_.back();
        stateStack_.pop_back();
        history_.pop_back();
    }

    int keepPly = static_cast<int>(history_.size());
    db_.UndoMovesAfter(currentGameId_, keepPly, state_);

    legalMoves_ = XiangqiEngine::GenerateLegalMoves(state_);
    result_ = GameResult::Ongoing;
    stats_ = db_.GetStats();
    selected_ = {-1, -1};
    aiTimer_ = 0.0f;
}

void XiangqiUI::ToggleReplayMode() {
    if (replayMode_) {
        replayMode_ = false;
        return;
    }

    std::int64_t id = -1;
    GameMode m = GameMode::PvE;
    GameResult r = GameResult::Ongoing;
    std::vector<SavedMove> moves;

    if (db_.LoadLatestGameWithMoves(id, m, r, moves)) {
        replayMode_ = true;
        replayMoves_ = std::move(moves);
        replayIndex_ = static_cast<int>(replayMoves_.size());
        selected_ = {-1, -1};
    }
}

bool XiangqiUI::GetBoardPosFromMouse(Pos& outPos) const {
    Vector2 mouse = GetMousePosition();
    int col = static_cast<int>((mouse.x - BOARD_OFFSET_X + CELL_SIZE * 0.5f) / CELL_SIZE);
    int row = static_cast<int>((mouse.y - BOARD_OFFSET_Y + CELL_SIZE * 0.5f) / CELL_SIZE);

    if (mouse.x < BOARD_OFFSET_X - CELL_SIZE * 0.5f || mouse.y < BOARD_OFFSET_Y - CELL_SIZE * 0.5f) {
        return false;
    }
    if (row < 0 || row >= 10 || col < 0 || col >= 9) {
        return false;
    }

    outPos = {row, col};
    return true;
}

void XiangqiUI::Update() {
    if (IsKeyPressed(KEY_P)) {
        ToggleReplayMode();
        return;
    }

    if (replayMode_) {
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            if (replayIndex_ < static_cast<int>(replayMoves_.size())) ++replayIndex_;
        } else if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            if (replayIndex_ > 0) --replayIndex_;
        } else if (IsKeyPressed(KEY_N)) {
            StartNewGame(mode_);
        }
        return;
    }

    if (IsKeyPressed(KEY_N)) {
        StartNewGame(mode_);
        return;
    }
    if (IsKeyPressed(KEY_M)) {
        StartNewGame((mode_ == GameMode::PvE) ? GameMode::PvP : GameMode::PvE);
        return;
    }
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_BACKSPACE)) {
        Undo();
        return;
    }

    if (result_ != GameResult::Ongoing) return;

    // PvE 模式下 AI（黑方）思考落子
    if (mode_ == GameMode::PvE && state_.turn == Side::Black) {
        aiTimer_ += GetFrameTime();
        if (aiTimer_ >= 0.15f) {
            Move best = XiangqiEngine::FindBestMove(state_, 3);
            if (best.from.IsValid()) {
                ExecuteMove(best);
            }
        }
        return;
    }

    // 玩家鼠标交互
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Pos clicked{};
        if (!GetBoardPosFromMouse(clicked)) {
            selected_ = {-1, -1};
            return;
        }

        if (selected_.IsValid()) {
            for (const Move& lm : legalMoves_) {
                if (lm.from == selected_ && lm.to == clicked) {
                    ExecuteMove(lm);
                    return;
                }
            }
        }

        if (state_.board[clicked.row][clicked.col].side == state_.turn) {
            selected_ = clicked;
        } else {
            selected_ = {-1, -1};
        }
    }
}

void XiangqiUI::DrawPiece(const Piece& piece, int row, int col) const {
    if (piece.IsEmpty()) return;

    float cx = static_cast<float>(BOARD_OFFSET_X + col * CELL_SIZE);
    float cy = static_cast<float>(BOARD_OFFSET_Y + row * CELL_SIZE);
    float r = CELL_SIZE * 0.43f;

    DrawCircle(static_cast<int>(cx + 2), static_cast<int>(cy + 3), r, Fade(BLACK, 0.28f));
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), r, PIECE_FILL);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r, PIECE_BORDER);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r - 4.0f, PIECE_BORDER);

    const char* glyph = PieceGlyph(piece);
    Color ink = (piece.side == Side::Red) ? RED_PIECE_INK : BLACK_PIECE_INK;
    float fontSize = 31.0f;
    Vector2 sz = MeasureTextEx(font_, glyph, fontSize, 1.0f);
    DrawTextEx(font_, glyph, {cx - sz.x * 0.5f, cy - sz.y * 0.5f}, fontSize, 1.0f, ink);
}

void XiangqiUI::DrawBoard(const BoardState& viewState, const Move* lastMovePtr) const {
    const float bx = static_cast<float>(BOARD_OFFSET_X);
    const float by = static_cast<float>(BOARD_OFFSET_Y);
    const float bw = static_cast<float>(CELL_SIZE * 8);
    const float bh = static_cast<float>(CELL_SIZE * 9);

    // 木质棋盘底板
    DrawRectangleRounded({bx - 34, by - 34, bw + 68, bh + 68}, 0.02f, 6, BOARD_WOOD);
    DrawRectangleLinesEx({bx - 6, by - 6, bw + 12, bh + 12}, 2.5f, BOARD_LINE_COLOR);

    // 10条横线
    for (int r = 0; r < 10; ++r) {
        float y = by + r * CELL_SIZE;
        DrawLineEx({bx, y}, {bx + bw, y}, 1.8f, BOARD_LINE_COLOR);
    }

    // 9条纵线（中间河界断开）
    for (int c = 0; c < 9; ++c) {
        float x = bx + c * CELL_SIZE;
        if (c == 0 || c == 8) {
            DrawLineEx({x, by}, {x, by + bh}, 1.8f, BOARD_LINE_COLOR);
        } else {
            DrawLineEx({x, by}, {x, by + 4 * CELL_SIZE}, 1.8f, BOARD_LINE_COLOR);
            DrawLineEx({x, by + 5 * CELL_SIZE}, {x, by + bh}, 1.8f, BOARD_LINE_COLOR);
        }
    }

    // 九宫斜线
    DrawLineEx({bx + 3 * CELL_SIZE, by}, {bx + 5 * CELL_SIZE, by + 2 * CELL_SIZE}, 1.6f, BOARD_LINE_COLOR);
    DrawLineEx({bx + 5 * CELL_SIZE, by}, {bx + 3 * CELL_SIZE, by + 2 * CELL_SIZE}, 1.6f, BOARD_LINE_COLOR);
    DrawLineEx({bx + 3 * CELL_SIZE, by + 7 * CELL_SIZE}, {bx + 5 * CELL_SIZE, by + 9 * CELL_SIZE}, 1.6f, BOARD_LINE_COLOR);
    DrawLineEx({bx + 5 * CELL_SIZE, by + 7 * CELL_SIZE}, {bx + 3 * CELL_SIZE, by + 9 * CELL_SIZE}, 1.6f, BOARD_LINE_COLOR);

    // 楚河汉界文字
    float riverY = by + 4.5f * CELL_SIZE - 16.0f;
    DrawTextCn("楚 河", bx + 1.15f * CELL_SIZE, riverY, 32.0f, BOARD_LINE_COLOR);
    DrawTextCn("漢 界", bx + 5.35f * CELL_SIZE, riverY, 32.0f, BOARD_LINE_COLOR);

    // 兵/炮位置十字小角标
    auto drawCrossMarker = [&](int row, int col) {
        float cx = bx + col * CELL_SIZE;
        float cy = by + row * CELL_SIZE;
        float g = 5.0f, len = 10.0f;
        if (col > 0) {
            DrawLineEx({cx - g, cy - g}, {cx - g - len, cy - g}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx - g, cy - g}, {cx - g, cy - g - len}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx - g, cy + g}, {cx - g - len, cy + g}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx - g, cy + g}, {cx - g, cy + g + len}, 1.5f, BOARD_LINE_COLOR);
        }
        if (col < 8) {
            DrawLineEx({cx + g, cy - g}, {cx + g + len, cy - g}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx + g, cy - g}, {cx + g, cy - g - len}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx + g, cy + g}, {cx + g + len, cy + g}, 1.5f, BOARD_LINE_COLOR);
            DrawLineEx({cx + g, cy + g}, {cx + g, cy + g + len}, 1.5f, BOARD_LINE_COLOR);
        }
    };
    for (int c : {1, 7}) { drawCrossMarker(2, c); drawCrossMarker(7, c); }
    for (int c : {0, 2, 4, 6, 8}) { drawCrossMarker(3, c); drawCrossMarker(6, c); }

    // 最后一步高亮框
    if (lastMovePtr != nullptr) {
        for (Pos p : {lastMovePtr->from, lastMovePtr->to}) {
            float x = bx + p.col * CELL_SIZE - CELL_SIZE * 0.45f;
            float y = by + p.row * CELL_SIZE - CELL_SIZE * 0.45f;
            DrawRectangleLinesEx({x, y, CELL_SIZE * 0.90f, CELL_SIZE * 0.90f}, 2.4f, {40, 130, 220, 220});
        }
    }

    // 绘制所有棋子
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            DrawPiece(viewState.board[r][c], r, c);
        }
    }

    // 被将军红框提示
    if (XiangqiEngine::IsInCheck(viewState, viewState.turn)) {
        Pos kp = XiangqiEngine::FindKing(viewState, viewState.turn);
        if (kp.IsValid()) {
            float cx = bx + kp.col * CELL_SIZE;
            float cy = by + kp.row * CELL_SIZE;
            DrawRing({cx, cy}, CELL_SIZE * 0.44f, CELL_SIZE * 0.50f, 0.0f, 360.0f, 32, RED);
        }
    }

    // 选中子与合法目标点提示
    if (!replayMode_ && selected_.IsValid()) {
        float sx = bx + selected_.col * CELL_SIZE - CELL_SIZE * 0.47f;
        float sy = by + selected_.row * CELL_SIZE - CELL_SIZE * 0.47f;
        DrawRectangleLinesEx({sx, sy, CELL_SIZE * 0.94f, CELL_SIZE * 0.94f}, 3.0f, GOLD);

        for (const Move& lm : legalMoves_) {
            if (lm.from != selected_) continue;
            float hx = bx + lm.to.col * CELL_SIZE;
            float hy = by + lm.to.row * CELL_SIZE;
            if (viewState.board[lm.to.row][lm.to.col].IsEmpty()) {
                DrawCircle(static_cast<int>(hx), static_cast<int>(hy), 8.0f, Fade(DARKGREEN, 0.7f));
            } else {
                DrawRing({hx, hy}, CELL_SIZE * 0.44f, CELL_SIZE * 0.49f, 0.0f, 360.0f, 32, RED);
            }
        }
    }
}

void XiangqiUI::DrawSidePanel() const {
    const float px = static_cast<float>(BOARD_OFFSET_X + CELL_SIZE * 8 + 56);
    const float py = static_cast<float>(BOARD_OFFSET_Y - 34);
    const float pw = static_cast<float>(WINDOW_WIDTH) - px - 24.0f;
    const float ph = static_cast<float>(CELL_SIZE * 9 + 68);

    DrawRectangleRounded({px, py, pw, ph}, 0.03f, 6, PANEL_BG);

    DrawTextCn("中国象棋 (SQLite存档版)", px + 18, py + 16, 24, RAYWHITE);

    if (replayMode_) {
        DrawTextCn("【复盘回放模式】", px + 18, py + 52, 20, GOLD);
        std::string progress = "当前第 " + std::to_string(replayIndex_) + " / " +
                               std::to_string(replayMoves_.size()) + " 步";
        DrawTextCn(progress.c_str(), px + 18, py + 82, 18, SKYBLUE);
        DrawTextCn("<- / -> 键逐步复盘", px + 18, py + 110, 17, LIGHTGRAY);
    } else {
        const char* modeStr = (mode_ == GameMode::PvE) ? "模式: 人机对战 (红方先手)" : "模式: 双人对战";
        DrawTextCn(modeStr, px + 18, py + 52, 18, SKYBLUE);

        const char* statusStr = "";
        Color statusColor = RAYWHITE;
        switch (result_) {
            case GameResult::Ongoing:
                statusStr = (state_.turn == Side::Red) ? "当前走子: 红方" : "当前走子: 黑方";
                statusColor = (state_.turn == Side::Red) ? Color{255, 110, 110, 255} : LIGHTGRAY;
                break;
            case GameResult::RedWin:
                statusStr = "将死! 红方获胜";
                statusColor = GREEN;
                break;
            case GameResult::BlackWin:
                statusStr = "将死! 黑方获胜";
                statusColor = ORANGE;
                break;
            case GameResult::Draw:
                statusStr = "和棋 (60回合规则)";
                statusColor = YELLOW;
                break;
        }
        DrawTextCn(statusStr, px + 18, py + 82, 20, statusColor);
    }

    // SQLite 战绩统计卡片
    DrawRectangleRounded({px + 14, py + 140, pw - 28, 76}, 0.08f, 6, {34, 38, 46, 255});
    std::string statLine1 = "数据库战绩统计  总局数: " + std::to_string(stats_.totalGames);
    std::string statLine2 = "红胜: " + std::to_string(stats_.redWins) +
                            "   黑胜: " + std::to_string(stats_.blackWins) +
                            "   和棋: " + std::to_string(stats_.draws);
    DrawTextCn(statLine1.c_str(), px + 24, py + 150, 17, GOLD);
    DrawTextCn(statLine2.c_str(), px + 24, py + 180, 17, RAYWHITE);

    // 棋谱记录列表
    DrawTextCn("走子棋谱记录:", px + 18, py + 230, 18, LIGHTGRAY);
    const auto& list = replayMode_ ? replayMoves_ : history_;
    int visibleCount = replayMode_ ? replayIndex_ : static_cast<int>(list.size());
    int totalRounds = (visibleCount + 1) / 2;
    int startRound = std::max(0, totalRounds - 11);
    float yCursor = py + 260.0f;

    for (int r = startRound; r < totalRounds; ++r) {
        int rIdx = r * 2;
        int bIdx = rIdx + 1;
        std::string line = std::to_string(r + 1) + ". " + list[static_cast<size_t>(rIdx)].notation;
        if (bIdx < visibleCount) {
            line += "   " + list[static_cast<size_t>(bIdx)].notation;
        }
        DrawTextCn(line.c_str(), px + 20, yCursor, 18, RAYWHITE);
        yCursor += 25.0f;
    }

    // 底部操作说明
    DrawLineEx({px + 16, py + ph - 116}, {px + pw - 16, py + ph - 116}, 1.0f, GRAY);
    DrawTextCn("[N] 新开一局 (自动存档)", px + 18, py + ph - 104, 17, LIGHTGRAY);
    DrawTextCn("[Z] 悔棋 (同步更新数据库)", px + 18, py + ph - 80, 17, LIGHTGRAY);
    DrawTextCn("[M] 切换 人机 / 双人 模式", px + 18, py + ph - 56, 17, LIGHTGRAY);
    DrawTextCn("[P] 切换历史棋局复盘", px + 18, py + ph - 32, 17, LIGHTGRAY);
}

void XiangqiUI::Draw() const {
    BeginDrawing();
    ClearBackground(WINDOW_BG);

    if (replayMode_) {
        BoardState viewState = (replayIndex_ == 0)
                                   ? XiangqiEngine::CreateInitialState()
                                   : replayMoves_[static_cast<size_t>(replayIndex_ - 1)].stateAfter;
        const Move* lastPtr = (replayIndex_ == 0)
                                  ? nullptr
                                  : &replayMoves_[static_cast<size_t>(replayIndex_ - 1)].move;
        DrawBoard(viewState, lastPtr);
    } else {
        const Move* lastPtr = history_.empty() ? nullptr : &history_.back().move;
        DrawBoard(state_, lastPtr);
    }

    DrawSidePanel();
    EndDrawing();
}