#include "search.h"
#include "board_state.h"
#include "move_generator.h"
#include "precomp_move_data.h"
#include "profiler.h"
#include "logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <random>

namespace {

std::string findFenFile() {
    const std::vector<std::string> candidates = {
        "tests/Test Positions/Fens.txt",
        "../tests/Test Positions/Fens.txt",
        "../../tests/Test Positions/Fens.txt",
        "../../../tests/Test Positions/Fens.txt"
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return {};
}

std::vector<std::string> loadFens(const std::string& path, int maxPositions) {
    std::vector<std::string> fens;
    std::ifstream in(path);
    if (!in.is_open()) return fens;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        fens.push_back(line);
        if (maxPositions > 0 && static_cast<int>(fens.size()) >= maxPositions) break;
    }
    return fens;
}

bool isMoveLegal(const Chess::BoardState& board, const Chess::Move& move) {
    Chess::MoveGenerator gen;
    gen.init();
    Chess::BoardState boardCopy = board;
    gen.generateLegalMoves(boardCopy, true);
    for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
        if (gen.moveList[i] == move) return true;
    }
    return false;
}

}

int main(int argc, char** argv) {
    using namespace Chess;

    PrecomputedMoveData::initialize();

#ifdef CHESS2D_SOURCE_DIR
    std::error_code ec;
    std::filesystem::current_path(CHESS2D_SOURCE_DIR, ec);
#endif

    if (!ChessLog::isInitialized()) {
        ChessLog::init("logs", spdlog::level::debug);
    }

    int maxPositions = 25;
    int depth = 7;
    int timeMs = 8000;

    if (argc > 1) maxPositions = std::max(1, std::atoi(argv[1]));
    if (argc > 2) depth = std::max(1, std::atoi(argv[2]));
    if (argc > 3) timeMs = std::max(0, std::atoi(argv[3]));

    const std::string fenPath = findFenFile();
    if (fenPath.empty()) {
        std::cerr << "[FAIL] Could not find FEN file.\n";
        return 1;
    }

    auto fens = loadFens(fenPath, maxPositions);
    if (fens.empty()) {
        std::cerr << "[FAIL] No FENs loaded.\n";
        return 1;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(fens.begin(), fens.end(), g);

    struct BenchResult {
        long long totalMs = 0;
        int invalidMoves = 0;
        int kingMoves = 0;
    };

    auto runBenchmark = [&](bool useMoveOrdering, const char* timerName) {
        BenchResult result;
        const auto t0 = std::chrono::steady_clock::now();

        for (const auto& fen : fens) {
            BoardState board;
            board.loadFEN(fen);

            SearchSettings settings = Search::DefaultSettings();
            settings.depth = depth;
            settings.useIterativeDeepening = true;
            settings.useThreading = true;
            settings.searchTime = timeMs;
            settings.detectRepetitionDraw = true;
            settings.evaluation.useMobilityEvaluation = true;
            settings.useMoveOrdering = useMoveOrdering;
            settings.logMoveOrdering = false;
            settings.logDepthTiming = true;

            Move best = Move::invalid();
            {
                Search search(board, 64, settings);
                search.runSearch(depth);
                best = search.getBestMove();
            }

            if (!best.isValid() || !isMoveLegal(board, best)) {
                result.invalidMoves++;
                continue;
            }

            if (board.getPieceTypeAt(best.startSquare()) == PIECE_KING) {
                result.kingMoves++;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        result.totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        return result;
    };

    const BenchResult withOrdering = runBenchmark(true, "Search.runSearch.orderingOn");
    const BenchResult withoutOrdering = runBenchmark(false, "Search.runSearch.orderingOff");

    std::cout << "\n=== Search Benchmark Summary ===\n";
    std::cout << "Positions:       " << fens.size() << '\n';
    std::cout << "Depth:           " << depth << '\n';
    std::cout << "Time limit (ms): " << timeMs << '\n';
    std::cout << "\n[Move ordering ON]\n";
    std::cout << "Total time (ms): " << withOrdering.totalMs << '\n';
    std::cout << "Avg/pos (ms):    " << (fens.empty() ? 0.0 : static_cast<double>(withOrdering.totalMs) / fens.size()) << '\n';
    std::cout << "Invalid moves:   " << withOrdering.invalidMoves << '\n';
    std::cout << "King moves:      " << withOrdering.kingMoves << " / " << fens.size() << '\n';

    std::cout << "\n[Move ordering OFF]\n";
    std::cout << "Total time (ms): " << withoutOrdering.totalMs << '\n';
    std::cout << "Avg/pos (ms):    " << (fens.empty() ? 0.0 : static_cast<double>(withoutOrdering.totalMs) / fens.size()) << '\n';
    std::cout << "Invalid moves:   " << withoutOrdering.invalidMoves << '\n';
    std::cout << "King moves:      " << withoutOrdering.kingMoves << " / " << fens.size() << '\n';

    if (ChessLog::isInitialized()) {
        ChessLog::shutdown();
    }

    return (withOrdering.invalidMoves == 0 && withoutOrdering.invalidMoves == 0) ? 0 : 1;
}
