#pragma once

#include "XiangqiTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool Init(const std::string& dbPath);

    std::int64_t CreateGame(GameMode mode, const BoardState& initialState);
    void RecordMove(std::int64_t gameId, int ply, const Move& move,
                    const std::string& notation, const BoardState& afterState);
    void UndoMovesAfter(std::int64_t gameId, int keepPly, const BoardState& restoredState);
    void UpdateGameResult(std::int64_t gameId, GameResult result);

    bool LoadLatestUnfinishedGame(std::int64_t& outGameId, GameMode& outMode,
                                  BoardState& outState, std::vector<SavedMove>& outMoves);
    bool LoadLatestGameWithMoves(std::int64_t& outGameId, GameMode& outMode,
                                 GameResult& outResult, std::vector<SavedMove>& outMoves);
    GameStats GetStats() const;

private:
    bool ExecSimple(const char* sql);
    std::vector<SavedMove> LoadMovesForGame(std::int64_t gameId) const;

    sqlite3* db_ = nullptr;
};