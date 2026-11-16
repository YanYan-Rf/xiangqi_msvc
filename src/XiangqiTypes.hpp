#pragma once

#include <array>
#include <cstdint>
#include <string>

enum class PieceType {
    None = 0,
    King,     // 帥 / 將
    Advisor,  // 仕 / 士
    Elephant, // 相 / 象
    Horse,    // 傌 / 馬
    Chariot,  // 俥 / 車
    Cannon,   // 炮 / 砲
    Soldier   // 兵 / 卒
};

enum class Side {
    None = 0,
    Red,   // 红方（下方，先手）
    Black  // 黑方（上方）
};

enum class GameMode {
    PvE,
    PvP
};

enum class GameResult {
    Ongoing,
    RedWin,
    BlackWin,
    Draw
};

struct Pos {
    int row = -1; // 0..9
    int col = -1; // 0..8

    bool IsValid() const {
        return row >= 0 && row < 10 && col >= 0 && col < 9;
    }
    bool operator==(const Pos& o) const { return row == o.row && col == o.col; }
    bool operator!=(const Pos& o) const { return !(*this == o); }
};

struct Piece {
    PieceType type = PieceType::None;
    Side side = Side::None;

    bool IsEmpty() const {
        return type == PieceType::None || side == Side::None;
    }
};

struct Move {
    Pos from{};
    Pos to{};

    bool operator==(const Move& o) const {
        return from == o.from && to == o.to;
    }
};

struct BoardState {
    std::array<std::array<Piece, 9>, 10> board{};
    Side turn = Side::Red;
    int halfMoveClock = 0;
};

struct SavedMove {
    int ply = 0;
    Move move{};
    std::string notation;
    BoardState stateAfter{};
};

struct GameStats {
    int totalGames = 0;
    int redWins = 0;
    int blackWins = 0;
    int draws = 0;
};