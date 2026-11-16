#pragma once

#include "XiangqiTypes.hpp"
#include <string>
#include <vector>

class XiangqiEngine {
public:
    static BoardState CreateInitialState();
    static Side Opposite(Side s);

    static Pos FindKing(const BoardState& state, Side side);
    static bool IsInCheck(const BoardState& state, Side side);

    static std::vector<Move> GenerateLegalMoves(const BoardState& state);
    static void MakeMove(BoardState& state, const Move& move);
    static GameResult EvaluateResult(const BoardState& state, const std::vector<Move>& legalMoves);

    static std::string MoveToChineseNotation(const BoardState& before, const Move& move);
    static std::string SerializeState(const BoardState& state);
    static bool DeserializeState(const std::string& data, BoardState& outState);

    // AI 搜索（Negamax + Alpha-Beta 剪枝）
    static Move FindBestMove(const BoardState& state, int depth = 3);

private:
    static std::vector<Move> GeneratePseudoLegalMoves(const BoardState& state);
    static int EvaluateBoard(const BoardState& state);
    static int MoveOrderScore(const BoardState& state, const Move& m);
    static int Negamax(BoardState& state, int depth, int alpha, int beta);
};