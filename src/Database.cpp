#include "Database.hpp"
#include "XiangqiEngine.hpp"

#include <sqlite3.h>

namespace {

const char* ModeToStr(GameMode mode) {
    return (mode == GameMode::PvE) ? "PvE" : "PvP";
}

GameMode StrToMode(const std::string& s) {
    return (s == "PvP") ? GameMode::PvP : GameMode::PvE;
}

const char* ResultToStr(GameResult r) {
    switch (r) {
        case GameResult::RedWin:   return "RedWin";
        case GameResult::BlackWin: return "BlackWin";
        case GameResult::Draw:     return "Draw";
        default:                   return "Ongoing";
    }
}

GameResult StrToResult(const std::string& s) {
    if (s == "RedWin")   return GameResult::RedWin;
    if (s == "BlackWin") return GameResult::BlackWin;
    if (s == "Draw")     return GameResult::Draw;
    return GameResult::Ongoing;
}

std::string SafeColText(sqlite3_stmt* stmt, int col) {
    const unsigned char* txt = sqlite3_column_text(stmt, col);
    return txt ? reinterpret_cast<const char*>(txt) : "";
}

} // namespace

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::ExecSimple(const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::Init(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        return false;
    }

    ExecSimple("PRAGMA foreign_keys = ON;");

    const char* createGamesSql =
        "CREATE TABLE IF NOT EXISTS games ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  mode TEXT NOT NULL,"
        "  result TEXT NOT NULL,"
        "  move_count INTEGER NOT NULL DEFAULT 0,"
        "  board_fen TEXT NOT NULL,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),"
        "  updated_at TEXT NOT NULL DEFAULT (datetime('now', 'localtime'))"
        ");";

    const char* createMovesSql =
        "CREATE TABLE IF NOT EXISTS moves ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  game_id INTEGER NOT NULL,"
        "  ply INTEGER NOT NULL,"
        "  from_r INTEGER NOT NULL,"
        "  from_c INTEGER NOT NULL,"
        "  to_r INTEGER NOT NULL,"
        "  to_c INTEGER NOT NULL,"
        "  notation TEXT NOT NULL,"
        "  board_fen TEXT NOT NULL,"
        "  FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE"
        ");";

    return ExecSimple(createGamesSql) && ExecSimple(createMovesSql);
}

std::int64_t Database::CreateGame(GameMode mode, const BoardState& initialState) {
    if (!db_) return -1;

    const char* sql =
        "INSERT INTO games (mode, result, move_count, board_fen) VALUES (?, 'Ongoing', 0, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    std::string fen = XiangqiEngine::SerializeState(initialState);
    sqlite3_bind_text(stmt, 1, ModeToStr(mode), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fen.c_str(), -1, SQLITE_TRANSIENT);

    std::int64_t id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        id = sqlite3_last_insert_rowid(db_);
    }
    sqlite3_finalize(stmt);
    return id;
}

void Database::RecordMove(std::int64_t gameId, int ply, const Move& move,
                          const std::string& notation, const BoardState& afterState) {
    if (!db_ || gameId <= 0) return;

    std::string fen = XiangqiEngine::SerializeState(afterState);
    ExecSimple("BEGIN TRANSACTION;");

    const char* insertMoveSql =
        "INSERT INTO moves (game_id, ply, from_r, from_c, to_r, to_c, notation, board_fen) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insertMoveSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, gameId);
        sqlite3_bind_int(stmt, 2, ply);
        sqlite3_bind_int(stmt, 3, move.from.row);
        sqlite3_bind_int(stmt, 4, move.from.col);
        sqlite3_bind_int(stmt, 5, move.to.row);
        sqlite3_bind_int(stmt, 6, move.to.col);
        sqlite3_bind_text(stmt, 7, notation.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, fen.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char* updateGameSql =
        "UPDATE games SET move_count = ?, board_fen = ?, updated_at = datetime('now', 'localtime') "
        "WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, updateGameSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, ply);
        sqlite3_bind_text(stmt, 2, fen.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, gameId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    ExecSimple("COMMIT;");
}

void Database::UndoMovesAfter(std::int64_t gameId, int keepPly, const BoardState& restoredState) {
    if (!db_ || gameId <= 0) return;

    std::string fen = XiangqiEngine::SerializeState(restoredState);
    ExecSimple("BEGIN TRANSACTION;");

    const char* delSql = "DELETE FROM moves WHERE game_id = ? AND ply > ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, delSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, gameId);
        sqlite3_bind_int(stmt, 2, keepPly);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char* updSql =
        "UPDATE games SET result = 'Ongoing', move_count = ?, board_fen = ?, "
        "updated_at = datetime('now', 'localtime') WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, updSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, keepPly);
        sqlite3_bind_text(stmt, 2, fen.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, gameId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    ExecSimple("COMMIT;");
}

void Database::UpdateGameResult(std::int64_t gameId, GameResult result) {
    if (!db_ || gameId <= 0) return;

    const char* sql =
        "UPDATE games SET result = ?, updated_at = datetime('now', 'localtime') WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, ResultToStr(result), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, gameId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<SavedMove> Database::LoadMovesForGame(std::int64_t gameId) const {
    std::vector<SavedMove> list;
    const char* sql =
        "SELECT ply, from_r, from_c, to_r, to_c, notation, board_fen "
        "FROM moves WHERE game_id = ? ORDER BY ply ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }
    sqlite3_bind_int64(stmt, 1, gameId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedMove sm;
        sm.ply = sqlite3_column_int(stmt, 0);
        sm.move.from.row = sqlite3_column_int(stmt, 1);
        sm.move.from.col = sqlite3_column_int(stmt, 2);
        sm.move.to.row   = sqlite3_column_int(stmt, 3);
        sm.move.to.col   = sqlite3_column_int(stmt, 4);
        sm.notation = SafeColText(stmt, 5);
        std::string fen = SafeColText(stmt, 6);
        if (XiangqiEngine::DeserializeState(fen, sm.stateAfter)) {
            list.push_back(std::move(sm));
        }
    }

    sqlite3_finalize(stmt);
    return list;
}

bool Database::LoadLatestUnfinishedGame(std::int64_t& outGameId, GameMode& outMode,
                                        BoardState& outState, std::vector<SavedMove>& outMoves) {
    if (!db_) return false;

    const char* sql =
        "SELECT id, mode, board_fen FROM games "
        "WHERE result = 'Ongoing' AND move_count > 0 "
        "ORDER BY id DESC LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outGameId = sqlite3_column_int64(stmt, 0);
        outMode = StrToMode(SafeColText(stmt, 1));
        std::string fen = SafeColText(stmt, 2);
        if (XiangqiEngine::DeserializeState(fen, outState)) {
            found = true;
        }
    }
    sqlite3_finalize(stmt);

    if (found) {
        outMoves = LoadMovesForGame(outGameId);
    }
    return found;
}

bool Database::LoadLatestGameWithMoves(std::int64_t& outGameId, GameMode& outMode,
                                       GameResult& outResult, std::vector<SavedMove>& outMoves) {
    if (!db_) return false;

    const char* sql =
        "SELECT id, mode, result FROM games WHERE move_count > 0 ORDER BY id DESC LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outGameId = sqlite3_column_int64(stmt, 0);
        outMode = StrToMode(SafeColText(stmt, 1));
        outResult = StrToResult(SafeColText(stmt, 2));
        found = true;
    }
    sqlite3_finalize(stmt);

    if (found) {
        outMoves = LoadMovesForGame(outGameId);
        return !outMoves.empty();
    }
    return false;
}

GameStats Database::GetStats() const {
    GameStats stats;
    if (!db_) return stats;

    const char* sql =
        "SELECT COUNT(*), "
        "COALESCE(SUM(CASE WHEN result = 'RedWin' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN result = 'BlackWin' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN result = 'Draw' THEN 1 ELSE 0 END), 0) "
        "FROM games WHERE move_count > 0;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalGames = sqlite3_column_int(stmt, 0);
            stats.redWins    = sqlite3_column_int(stmt, 1);
            stats.blackWins  = sqlite3_column_int(stmt, 2);
            stats.draws      = sqlite3_column_int(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    return stats;
}