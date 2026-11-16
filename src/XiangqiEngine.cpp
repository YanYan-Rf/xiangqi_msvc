#include "XiangqiEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace {

constexpr int PIECE_VALUES[8] = {
    0,     // None
    20000, // King
    200,   // Advisor
    200,   // Elephant
    420,   // Horse
    900,   // Chariot
    460,   // Cannon
    100    // Soldier
};

// 红方视角位置附加分（10行 x 9列）
constexpr int SOLDIER_PST[10][9] = {
    { 9,  9,  9, 11, 13, 11,  9,  9,  9},
    {39, 49, 69, 79, 89, 79, 69, 49, 39},
    {39, 49, 64, 74, 79, 74, 64, 49, 39},
    {29, 39, 49, 59, 64, 59, 49, 39, 29},
    {19, 29, 39, 49, 54, 49, 39, 29, 19},
    { 7,  0, 13,  0, 16,  0, 13,  0,  7},
    { 7,  0,  7,  0, 15,  0,  7,  0,  7},
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},
    { 0,  0,  0,  0,  0,  0,  0,  0,  0},
    { 0,  0,  0,  0,  0,  0,  0,  0,  0}
};

constexpr int HORSE_PST[10][9] = {
    { 4,  8, 16, 12,  4, 12, 16,  8,  4},
    { 4, 10, 28, 16,  8, 16, 28, 10,  4},
    {12, 14, 16, 20, 18, 20, 16, 14, 12},
    { 8, 24, 18, 24, 20, 24, 18, 24,  8},
    { 6, 16, 14, 18, 16, 18, 14, 16,  6},
    { 4, 12, 16, 14, 12, 14, 16, 12,  4},
    { 2,  6,  8,  6, 10,  6,  8,  6,  2},
    { 4,  2,  8,  8,  4,  8,  8,  2,  4},
    { 0,  2,  4,  4, -2,  4,  4,  2,  0},
    { 0, -4,  0,  0,  0,  0,  0, -4,  0}
};

constexpr int CANNON_PST[10][9] = {
    { 6,  4,  0,-10,-12,-10,  0,  4,  6},
    { 2,  2,  0, -4,-14, -4,  0,  2,  2},
    { 2,  2,  0,-10, -8,-10,  0,  2,  2},
    { 0,  0, -2,  4, 10,  4, -2,  0,  0},
    { 0,  0,  0,  2,  8,  2,  0,  0,  0},
    {-2,  0,  4,  2,  6,  2,  4,  0, -2},
    { 0,  0,  0,  2,  4,  2,  0,  0,  0},
    { 4,  0,  8,  6, 10,  6,  8,  0,  4},
    { 0,  2,  4,  6,  6,  6,  4,  2,  0},
    { 0,  0,  2,  6,  6,  6,  2,  0,  0}
};

constexpr int CHARIOT_PST[10][9] = {
    {14, 14, 12, 18, 16, 18, 12, 14, 14},
    {16, 20, 18, 24, 26, 24, 18, 20, 16},
    {12, 12, 12, 18, 18, 18, 12, 12, 12},
    {12, 18, 16, 22, 22, 22, 16, 18, 12},
    {12, 14, 12, 18, 18, 18, 12, 14, 12},
    {12, 16, 14, 20, 20, 20, 14, 16, 12},
    { 6, 10,  8, 14, 14, 14,  8, 10,  6},
    { 4,  8,  6, 14, 12, 14,  6,  8,  4},
    { 8,  4,  8, 16,  8, 16,  8,  4,  8},
    {-2, 10,  6, 14, 12, 14,  6, 10, -2}
};

bool InPalace(int r, int c, Side side) {
    if (c < 3 || c > 5) return false;
    return (side == Side::Red) ? (r >= 7 && r <= 9) : (r >= 0 && r <= 2);
}

bool OnOwnSide(int r, Side side) {
    return (side == Side::Red) ? (r >= 5) : (r <= 4);
}

char PieceToChar(const Piece& p) {
    if (p.IsEmpty()) return '.';
    char ch = '.';
    switch (p.type) {
        case PieceType::King:     ch = 'k'; break;
        case PieceType::Advisor:  ch = 'a'; break;
        case PieceType::Elephant: ch = 'b'; break;
        case PieceType::Horse:    ch = 'n'; break;
        case PieceType::Chariot:  ch = 'r'; break;
        case PieceType::Cannon:   ch = 'c'; break;
        case PieceType::Soldier:  ch = 'p'; break;
        default: return '.';
    }
    if (p.side == Side::Red) {
        ch = static_cast<char>(ch - 'a' + 'A');
    }
    return ch;
}

Piece CharToPiece(char ch) {
    if (ch == '.') return {};
    Side side = (ch >= 'A' && ch <= 'Z') ? Side::Red : Side::Black;
    char low = (side == Side::Red) ? static_cast<char>(ch - 'A' + 'a') : ch;
    PieceType type = PieceType::None;
    switch (low) {
        case 'k': type = PieceType::King; break;
        case 'a': type = PieceType::Advisor; break;
        case 'b': type = PieceType::Elephant; break;
        case 'n': type = PieceType::Horse; break;
        case 'r': type = PieceType::Chariot; break;
        case 'c': type = PieceType::Cannon; break;
        case 'p': type = PieceType::Soldier; break;
        default: return {};
    }
    return {type, side};
}

std::string PieceName(const Piece& p) {
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

std::string FileText(int col, Side side) {
    static const char* RED_NUM[9]   = {"九", "八", "七", "六", "五", "四", "三", "二", "一"};
    static const char* BLACK_NUM[9] = {"1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9"};
    return (side == Side::Red) ? RED_NUM[col] : BLACK_NUM[col];
}

std::string StepText(int steps, Side side) {
    static const char* RED_STEP[10]   = {"", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
    static const char* BLACK_STEP[10] = {"", "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9"};
    if (steps < 1 || steps > 9) return "";
    return (side == Side::Red) ? RED_STEP[steps] : BLACK_STEP[steps];
}

} // namespace

BoardState XiangqiEngine::CreateInitialState() {
    BoardState s;
    constexpr PieceType backRank[9] = {
        PieceType::Chariot, PieceType::Horse, PieceType::Elephant,
        PieceType::Advisor, PieceType::King, PieceType::Advisor,
        PieceType::Elephant, PieceType::Horse, PieceType::Chariot
    };

    for (int c = 0; c < 9; ++c) {
        s.board[0][c] = {backRank[c], Side::Black};
        s.board[9][c] = {backRank[c], Side::Red};
    }

    s.board[2][1] = {PieceType::Cannon, Side::Black};
    s.board[2][7] = {PieceType::Cannon, Side::Black};
    s.board[7][1] = {PieceType::Cannon, Side::Red};
    s.board[7][7] = {PieceType::Cannon, Side::Red};

    for (int c : {0, 2, 4, 6, 8}) {
        s.board[3][c] = {PieceType::Soldier, Side::Black};
        s.board[6][c] = {PieceType::Soldier, Side::Red};
    }

    s.turn = Side::Red;
    s.halfMoveClock = 0;
    return s;
}

Side XiangqiEngine::Opposite(Side s) {
    if (s == Side::Red) return Side::Black;
    if (s == Side::Black) return Side::Red;
    return Side::None;
}

Pos XiangqiEngine::FindKing(const BoardState& state, Side side) {
    int rStart = (side == Side::Red) ? 7 : 0;
    int rEnd   = (side == Side::Red) ? 9 : 2;
    for (int r = rStart; r <= rEnd; ++r) {
        for (int c = 3; c <= 5; ++c) {
            if (state.board[r][c].type == PieceType::King &&
                state.board[r][c].side == side) {
                return {r, c};
            }
        }
    }
    return {-1, -1};
}

bool XiangqiEngine::IsInCheck(const BoardState& state, Side side) {
    Pos kPos = FindKing(state, side);
    if (!kPos.IsValid()) return true;

    Side enemy = Opposite(side);

    // 1. 直线检测：車、砲、以及“将帅照面”
    constexpr int ortho[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& d : ortho) {
        int r = kPos.row + d[0];
        int c = kPos.col + d[1];
        int blockers = 0;
        while (r >= 0 && r < 10 && c >= 0 && c < 9) {
            const Piece& p = state.board[r][c];
            if (!p.IsEmpty()) {
                if (blockers == 0) {
                    if (p.side == enemy && (p.type == PieceType::Chariot || p.type == PieceType::King)) {
                        return true;
                    }
                    ++blockers;
                } else {
                    if (p.side == enemy && p.type == PieceType::Cannon) {
                        return true;
                    }
                    break;
                }
            }
            r += d[0];
            c += d[1];
        }
    }

    // 2. 敌方馬攻击（同时校验马腿不被蹩）
    constexpr int horseOffsets[8][4] = {
        {-2, -1,  1,  0}, {-2,  1,  1,  0},
        { 2, -1, -1,  0}, { 2,  1, -1,  0},
        {-1, -2,  0,  1}, { 1, -2,  0,  1},
        {-1,  2,  0, -1}, { 1,  2,  0, -1}
    };
    for (const auto& h : horseOffsets) {
        int hr = kPos.row + h[0];
        int hc = kPos.col + h[1];
        if (hr >= 0 && hr < 10 && hc >= 0 && hc < 9) {
            const Piece& p = state.board[hr][hc];
            if (p.side == enemy && p.type == PieceType::Horse) {
                int legR = hr + h[2];
                int legC = hc + h[3];
                if (state.board[legR][legC].IsEmpty()) {
                    return true;
                }
            }
        }
    }

    // 3. 敌方兵/卒攻击
    int forwardDir = (side == Side::Red) ? -1 : 1; // 敌兵在将/帅的前方或左右
    int frontR = kPos.row + forwardDir;
    if (frontR >= 0 && frontR < 10) {
        const Piece& p = state.board[frontR][kPos.col];
        if (p.side == enemy && p.type == PieceType::Soldier) return true;
    }
    for (int dc : {-1, 1}) {
        int sc = kPos.col + dc;
        if (sc >= 0 && sc < 9) {
            const Piece& p = state.board[kPos.row][sc];
            if (p.side == enemy && p.type == PieceType::Soldier) return true;
        }
    }

    return false;
}

std::vector<Move> XiangqiEngine::GeneratePseudoLegalMoves(const BoardState& state) {
    std::vector<Move> moves;
    moves.reserve(64);
    Side us = state.turn;

    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            const Piece& p = state.board[r][c];
            if (p.side != us) continue;

            Pos from{r, c};
            switch (p.type) {
                case PieceType::King: {
                    constexpr int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (const auto& d : dirs) {
                        int nr = r + d[0];
                        int nc = c + d[1];
                        if (InPalace(nr, nc, us) && state.board[nr][nc].side != us) {
                            moves.push_back({from, {nr, nc}});
                        }
                    }
                    break;
                }

                case PieceType::Advisor: {
                    constexpr int dirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
                    for (const auto& d : dirs) {
                        int nr = r + d[0];
                        int nc = c + d[1];
                        if (InPalace(nr, nc, us) && state.board[nr][nc].side != us) {
                            moves.push_back({from, {nr, nc}});
                        }
                    }
                    break;
                }

                case PieceType::Elephant: {
                    constexpr int dirs[4][2] = {{-2, -2}, {-2, 2}, {2, -2}, {2, 2}};
                    for (const auto& d : dirs) {
                        int nr = r + d[0];
                        int nc = c + d[1];
                        if (nr >= 0 && nr < 10 && nc >= 0 && nc < 9 && OnOwnSide(nr, us)) {
                            int eyeR = r + d[0] / 2;
                            int eyeC = c + d[1] / 2;
                            if (state.board[eyeR][eyeC].IsEmpty() && state.board[nr][nc].side != us) {
                                moves.push_back({from, {nr, nc}});
                            }
                        }
                    }
                    break;
                }

                case PieceType::Horse: {
                    constexpr int jumps[8][4] = {
                        {-2, -1, -1,  0}, {-2,  1, -1,  0},
                        { 2, -1,  1,  0}, { 2,  1,  1,  0},
                        {-1, -2,  0, -1}, { 1, -2,  0, -1},
                        {-1,  2,  0,  1}, { 1,  2,  0,  1}
                    };
                    for (const auto& j : jumps) {
                        int nr = r + j[0];
                        int nc = c + j[1];
                        if (nr >= 0 && nr < 10 && nc >= 0 && nc < 9) {
                            int legR = r + j[2];
                            int legC = c + j[3];
                            if (state.board[legR][legC].IsEmpty() && state.board[nr][nc].side != us) {
                                moves.push_back({from, {nr, nc}});
                            }
                        }
                    }
                    break;
                }

                case PieceType::Chariot: {
                    constexpr int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (const auto& d : dirs) {
                        int nr = r + d[0];
                        int nc = c + d[1];
                        while (nr >= 0 && nr < 10 && nc >= 0 && nc < 9) {
                            if (state.board[nr][nc].IsEmpty()) {
                                moves.push_back({from, {nr, nc}});
                            } else {
                                if (state.board[nr][nc].side != us) {
                                    moves.push_back({from, {nr, nc}});
                                }
                                break;
                            }
                            nr += d[0];
                            nc += d[1];
                        }
                    }
                    break;
                }

                case PieceType::Cannon: {
                    constexpr int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (const auto& d : dirs) {
                        int nr = r + d[0];
                        int nc = c + d[1];
                        bool jumped = false;
                        while (nr >= 0 && nr < 10 && nc >= 0 && nc < 9) {
                            if (!jumped) {
                                if (state.board[nr][nc].IsEmpty()) {
                                    moves.push_back({from, {nr, nc}});
                                } else {
                                    jumped = true;
                                }
                            } else {
                                if (!state.board[nr][nc].IsEmpty()) {
                                    if (state.board[nr][nc].side != us) {
                                        moves.push_back({from, {nr, nc}});
                                    }
                                    break;
                                }
                            }
                            nr += d[0];
                            nc += d[1];
                        }
                    }
                    break;
                }

                case PieceType::Soldier: {
                    int forward = (us == Side::Red) ? -1 : 1;
                    int nr = r + forward;
                    if (nr >= 0 && nr < 10 && state.board[nr][c].side != us) {
                        moves.push_back({from, {nr, c}});
                    }
                    if (!OnOwnSide(r, us)) { // 已过河可左右移动
                        for (int dc : {-1, 1}) {
                            int nc = c + dc;
                            if (nc >= 0 && nc < 9 && state.board[r][nc].side != us) {
                                moves.push_back({from, {r, nc}});
                            }
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }

    return moves;
}

void XiangqiEngine::MakeMove(BoardState& state, const Move& move) {
    bool isCapture = !state.board[move.to.row][move.to.col].IsEmpty();
    state.halfMoveClock = isCapture ? 0 : (state.halfMoveClock + 1);

    state.board[move.to.row][move.to.col] = state.board[move.from.row][move.from.col];
    state.board[move.from.row][move.from.col] = {};
    state.turn = Opposite(state.turn);
}

std::vector<Move> XiangqiEngine::GenerateLegalMoves(const BoardState& state) {
    std::vector<Move> pseudo = GeneratePseudoLegalMoves(state);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());

    Side us = state.turn;
    for (const Move& m : pseudo) {
        BoardState next = state;
        MakeMove(next, m);
        if (!IsInCheck(next, us)) {
            legal.push_back(m);
        }
    }
    return legal;
}

GameResult XiangqiEngine::EvaluateResult(const BoardState& state, const std::vector<Move>& legalMoves) {
    // 中国象棋中：无论是将死还是困毙（无合法走法），均判走棋方负
    if (legalMoves.empty()) {
        return (state.turn == Side::Red) ? GameResult::BlackWin : GameResult::RedWin;
    }
    if (state.halfMoveClock >= 120) {
        return GameResult::Draw;
    }
    return GameResult::Ongoing;
}

std::string XiangqiEngine::MoveToChineseNotation(const BoardState& before, const Move& move) {
    const Piece& p = before.board[move.from.row][move.from.col];
    if (p.IsEmpty()) return "";

    std::string note = PieceName(p) + FileText(move.from.col, p.side);
    int dr = move.to.row - move.from.row;

    if (dr == 0) {
        note += "平" + FileText(move.to.col, p.side);
    } else {
        bool advancing = (p.side == Side::Red) ? (dr < 0) : (dr > 0);
        note += advancing ? "進" : "退";

        // 直走子力（车/炮/兵/帅）用步数，斜走子力（马/象/士）用目标纵线
        bool straightPiece = (p.type == PieceType::King || p.type == PieceType::Chariot ||
                              p.type == PieceType::Cannon || p.type == PieceType::Soldier);
        if (straightPiece) {
            note += StepText(std::abs(dr), p.side);
        } else {
            note += FileText(move.to.col, p.side);
        }
    }
    return note;
}

std::string XiangqiEngine::SerializeState(const BoardState& state) {
    std::string out;
    out.reserve(105);
    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            out += PieceToChar(state.board[r][c]);
        }
        if (r < 9) out += '/';
    }
    out += (state.turn == Side::Red) ? "|R|" : "|B|";
    out += std::to_string(state.halfMoveClock);
    return out;
}

bool XiangqiEngine::DeserializeState(const std::string& data, BoardState& outState) {
    size_t p1 = data.find('|');
    size_t p2 = data.find('|', (p1 == std::string::npos) ? 0 : p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;

    std::string boardPart = data.substr(0, p1);
    std::string turnPart  = data.substr(p1 + 1, p2 - p1 - 1);
    std::string clockPart = data.substr(p2 + 1);

    BoardState s;
    std::istringstream iss(boardPart);
    std::string rowStr;
    int r = 0;
    while (std::getline(iss, rowStr, '/')) {
        if (r >= 10 || rowStr.size() != 9) return false;
        for (int c = 0; c < 9; ++c) {
            s.board[r][c] = CharToPiece(rowStr[static_cast<size_t>(c)]);
        }
        ++r;
    }
    if (r != 10) return false;

    s.turn = (turnPart == "B") ? Side::Black : Side::Red;
    try {
        s.halfMoveClock = std::stoi(clockPart);
    } catch (...) {
        s.halfMoveClock = 0;
    }

    outState = s;
    return true;
}

int XiangqiEngine::EvaluateBoard(const BoardState& state) {
    int redScore = 0;
    int blackScore = 0;

    for (int r = 0; r < 10; ++r) {
        for (int c = 0; c < 9; ++c) {
            const Piece& p = state.board[r][c];
            if (p.IsEmpty()) continue;

            int pstRow = (p.side == Side::Red) ? r : (9 - r);
            int val = PIECE_VALUES[static_cast<int>(p.type)];

            switch (p.type) {
                case PieceType::Soldier: val += SOLDIER_PST[pstRow][c]; break;
                case PieceType::Horse:   val += HORSE_PST[pstRow][c]; break;
                case PieceType::Cannon:  val += CANNON_PST[pstRow][c]; break;
                case PieceType::Chariot: val += CHARIOT_PST[pstRow][c]; break;
                default: break;
            }

            if (p.side == Side::Red) redScore += val;
            else blackScore += val;
        }
    }

    int diff = redScore - blackScore;
    return (state.turn == Side::Red) ? diff : -diff;
}

int XiangqiEngine::MoveOrderScore(const BoardState& state, const Move& m) {
    const Piece& victim = state.board[m.to.row][m.to.col];
    const Piece& attacker = state.board[m.from.row][m.from.col];
    int score = 0;
    if (!victim.IsEmpty()) {
        score += 10 * PIECE_VALUES[static_cast<int>(victim.type)] - PIECE_VALUES[static_cast<int>(attacker.type)];
    }
    return score;
}

int XiangqiEngine::Negamax(BoardState& state, int depth, int alpha, int beta) {
    if (depth == 0) {
        return EvaluateBoard(state);
    }

    std::vector<Move> moves = GenerateLegalMoves(state);
    if (moves.empty()) {
        return -(100000 + depth * 100);
    }

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return MoveOrderScore(state, a) > MoveOrderScore(state, b);
    });

    int best = std::numeric_limits<int>::min() + 1;
    for (const Move& m : moves) {
        BoardState next = state;
        MakeMove(next, m);
        int score = -Negamax(next, depth - 1, -beta, -alpha);
        if (score > best) best = score;
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    return best;
}

Move XiangqiEngine::FindBestMove(const BoardState& state, int depth) {
    std::vector<Move> moves = GenerateLegalMoves(state);
    if (moves.empty()) return {};

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        return MoveOrderScore(state, a) > MoveOrderScore(state, b);
    });

    Move bestMove = moves[0];
    int alpha = std::numeric_limits<int>::min() + 1;
    int beta = std::numeric_limits<int>::max();

    for (const Move& m : moves) {
        BoardState next = state;
        MakeMove(next, m);
        int score = -Negamax(next, depth - 1, -beta, -alpha);
        if (score > alpha) {
            alpha = score;
            bestMove = m;
        }
    }
    return bestMove;
}