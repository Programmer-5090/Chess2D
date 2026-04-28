#include "search.h"
#include "move_generator.h"
#include "board_state.h"
#include "precomp_move_data.h"
#include "chess2D/board.h"
#include "logger.h"
#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <random>

namespace {

struct EngineConfig {
    std::string name;
    Chess::EvaluationOptions eval{};
    bool useThreading = true;
    int depth = 4;
    int timeMs = 2000;
};

enum class GameResult {
    WhiteWin,
    BlackWin,
    Draw
};

struct MatchStats {
    int wins = 0;
    int losses = 0;
    int draws = 0;

    int whiteWins = 0;
    int whiteLosses = 0;
    int whiteDraws = 0;
    int blackWins = 0;
    int blackLosses = 0;
    int blackDraws = 0;

    double points() const {
        return static_cast<double>(wins) + 0.5 * static_cast<double>(draws);
    }
};

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
    if (!in.is_open()) {
        return fens;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        fens.push_back(line);
        if (maxPositions > 0 && static_cast<int>(fens.size()) >= maxPositions) {
            break;
        }
    }

    return fens;
}

GameResult playGame(const std::string& startFen,
                    const EngineConfig& whiteCfg,
                    const EngineConfig& blackCfg,
                    int maxPlies,
                    Chess::Board* guiBoard,
                    SDL_Renderer* renderer,
                    int renderDelayMs,
                    bool& guiQuitRequested) {
    using namespace Chess;

    BoardState board;
    board.loadFEN(startFen);

    if (guiBoard && renderer) {
        guiBoard->loadFEN(startFen);
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        guiBoard->draw(-1);
        SDL_RenderPresent(renderer);
    }

    MoveGenerator gen;
    gen.init();

    for (int ply = 0; ply < maxPlies; ++ply) {
        if (guiBoard && renderer) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    guiQuitRequested = true;
                    return GameResult::Draw;
                }
            }
        }

        gen.generateLegalMoves(board, true);
        if (gen.getLegalMoveCount() == 0) {
            if (gen.getInCheck()) {
                return board.getSide() == COLOR_WHITE ? GameResult::BlackWin : GameResult::WhiteWin;
            }
            return GameResult::Draw;
        }

        if (board.isDrawByFiftyMove() || board.isInsufficientMaterial()) {
            return GameResult::Draw;
        }

        const EngineConfig& cfg = (board.getSide() == COLOR_WHITE) ? whiteCfg : blackCfg;
        SearchSettings settings = Search::DefaultSettings();
        settings.depth = cfg.depth;
        settings.useIterativeDeepening = true;
        settings.useThreading = cfg.useThreading;
        settings.searchTime = cfg.timeMs;
        settings.endlessSearch = false;
        settings.abortSearch = false;
        settings.evaluation = cfg.eval;
        settings.detectRepetitionDraw = true;

        Search search(board, 64, settings);
        const int score = search.runSearch(cfg.depth);
        const Move best = search.getBestMove();

        if (!best.isValid()) {
            return GameResult::Draw;
        }

        LOG_DEBUG_F("[Versus] ply=%d side=%s engine=%s move=%s score=%d", ply,
            board.getSide() == COLOR_WHITE ? "white" : "black",
            cfg.name.c_str(), best.toString().c_str(), score);

        board.makeMove(best);

        if (guiBoard && renderer) {
            guiBoard->makeMove(best);
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
            SDL_RenderClear(renderer);
            guiBoard->draw(-1);
            SDL_RenderPresent(renderer);
            if (renderDelayMs > 0) {
                SDL_Delay(static_cast<Uint32>(renderDelayMs));
            }
        }
    }

    // Simple adjudication after move limit to avoid reporting every long game as draw.
    int materialBalance = 0;
    for (int pieceType = PIECE_PAWN; pieceType <= PIECE_QUEEN; ++pieceType) {
        materialBalance += board.getPieceList(COLOR_WHITE, pieceType).count() * getPieceValue(pieceType);
        materialBalance -= board.getPieceList(COLOR_BLACK, pieceType).count() * getPieceValue(pieceType);
    }

    if (materialBalance >= 300) return GameResult::WhiteWin;
    if (materialBalance <= -300) return GameResult::BlackWin;
    return GameResult::Draw;
}

void applyResultFromPerspective(GameResult result, bool testEngineIsWhite, MatchStats& stats) {
    if (result == GameResult::Draw) {
        stats.draws++;
        return;
    }

    const bool whiteWon = (result == GameResult::WhiteWin);
    const bool testEngineWon = (testEngineIsWhite && whiteWon) || (!testEngineIsWhite && !whiteWon);

    if (testEngineWon) {
        stats.wins++;
    } else {
        stats.losses++;
    }
}

void applyResultWithColor(GameResult result, bool testEngineIsWhite, MatchStats& stats) {
    applyResultFromPerspective(result, testEngineIsWhite, stats);
    
    if (result == GameResult::Draw) {
        if (testEngineIsWhite) stats.whiteDraws++;
        else stats.blackDraws++;
    } else if ((result == GameResult::WhiteWin && testEngineIsWhite) ||
               (result == GameResult::BlackWin && !testEngineIsWhite)) {
        if (testEngineIsWhite) stats.whiteWins++;
        else stats.blackWins++;
    } else {
        if (testEngineIsWhite) stats.whiteLosses++;
        else stats.blackLosses++;
    }
}

}

int main(int argc, char** argv) {
    using namespace Chess;

    PrecomputedMoveData::initialize();

#ifdef CHESS2D_SOURCE_DIR
    std::error_code ec;
    std::filesystem::current_path(CHESS2D_SOURCE_DIR, ec);
#endif

    int maxPositions = 50;
    int maxPlies = 100;
    int depth = 5;
    bool renderGui = true;
    int renderDelayMs = 50;

    if (!ChessLog::isInitialized()) {
        ChessLog::init("logs", spdlog::level::debug);
    }

    if (argc > 1) maxPositions = std::max(1, std::atoi(argv[1]));
    if (argc > 2) maxPlies = std::max(10, std::atoi(argv[2]));
    if (argc > 3) depth = std::max(1, std::atoi(argv[3]));
    if (argc > 4) renderGui = std::atoi(argv[4]) != 0;
    if (argc > 5) renderDelayMs = std::max(0, std::atoi(argv[5]));

    const std::string fenPath = findFenFile();
    if (fenPath.empty()) {
        std::cerr << "[FAIL] Could not find FEN file at tests/Test Positions/Fens.txt\n";
        return 1;
    }

    auto fens = loadFens(fenPath, maxPositions);
    if (fens.empty()) {
        std::cerr << "[FAIL] No FENs loaded from: " << fenPath << '\n';
        return 1;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(fens.begin(), fens.end(), g);

    EngineConfig oldEngine{"old_eval", EvaluationOptions{ false, false, false, false, false, false, 8 }, false, depth, 2000 };
    EngineConfig newEngine{"new_eval", EvaluationOptions{ true, true, true, true, true, true, 8 }, false, depth, 2000};

    MatchStats stats;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<Board> guiBoard;
    bool guiQuitRequested = false;

    if (renderGui) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "[WARN] SDL_Init failed, continuing without GUI: " << SDL_GetError() << '\n';
            renderGui = false;
        } else {
            window = SDL_CreateWindow("Chess2D Versus Test", 700, 700, SDL_WINDOW_RESIZABLE);
            if (!window) {
                std::cerr << "[WARN] SDL_CreateWindow failed, continuing without GUI: " << SDL_GetError() << '\n';
                renderGui = false;
            } else {
                renderer = SDL_CreateRenderer(window, nullptr);
                if (!renderer) {
                    std::cerr << "[WARN] SDL_CreateRenderer failed, continuing without GUI: " << SDL_GetError() << '\n';
                    renderGui = false;
                } else {
                    guiBoard = std::make_unique<Board>(680);
                    guiBoard->initializeBoard(renderer);
                }
            }
        }
    }

    std::cout << "Loaded " << fens.size() << " FENs from: " << fenPath << '\n';
    std::cout << "Running 2 games per FEN (swap colors), depth=" << depth << ", maxPlies=" << maxPlies
              << ", threadedSearch=" << (newEngine.useThreading ? "on" : "off") << "\n\n";

    int gameIndex = 1;
    for (const auto& fen : fens) {
        bool quit1 = false;
        const GameResult g1 = playGame(fen, newEngine, oldEngine, maxPlies,
            renderGui ? guiBoard.get() : nullptr, 
            renderGui ? renderer : nullptr, 
            renderDelayMs, quit1);
        applyResultWithColor(g1, true, stats);
        std::cout << "Game " << gameIndex++ << " (new as White): "
                  << (g1 == GameResult::WhiteWin ? "1-0" : g1 == GameResult::BlackWin ? "0-1" : "1/2-1/2")
                  << '\n';

        if (quit1) break;

        bool quit2 = false;
        const GameResult g2 = playGame(fen, oldEngine, newEngine, maxPlies,
            renderGui ? guiBoard.get() : nullptr, 
            renderGui ? renderer : nullptr, 
            renderDelayMs, quit2);
        applyResultWithColor(g2, false, stats);
        std::cout << "Game " << gameIndex++ << " (new as Black): "
                  << (g2 == GameResult::WhiteWin ? "1-0" : g2 == GameResult::BlackWin ? "0-1" : "1/2-1/2")
                  << '\n';

        if (quit2) break;
    }

    const int totalGames = stats.wins + stats.losses + stats.draws;
    std::cout << "\n=== Versus Summary (new eval vs old eval) ===\n";
    std::cout << "Games:  " << totalGames << '\n';
    std::cout << "Wins:   " << stats.wins << '\n';
    std::cout << "Losses: " << stats.losses << '\n';
    std::cout << "Draws:  " << stats.draws << '\n';
    std::cout << "Score:  " << stats.points() << " / " << totalGames << '\n';
    
    std::cout << "\nAs White:  " << stats.whiteWins << "W " << stats.whiteLosses << "L " << stats.whiteDraws << "D\n";
    std::cout << "As Black:  " << stats.blackWins << "W " << stats.blackLosses << "L " << stats.blackDraws << "D\n";
    
    const double whiteScore = stats.whiteWins + 0.5 * stats.whiteDraws;
    const double blackScore = stats.blackWins + 0.5 * stats.blackDraws;
    const int whiteGames = stats.whiteWins + stats.whiteLosses + stats.whiteDraws;
    const int blackGames = stats.blackWins + stats.blackLosses + stats.blackDraws;
    
    std::cout << "\nAs White:  " << (whiteGames > 0 ? whiteScore / whiteGames * 100.0 : 0.0) << "% (" << whiteScore << " / " << whiteGames << ")\n";
    std::cout << "As Black:  " << (blackGames > 0 ? blackScore / blackGames * 100.0 : 0.0) << "% (" << blackScore << " / " << blackGames << ")\n";

    guiBoard.reset();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (renderGui) SDL_Quit();
    if (ChessLog::isInitialized()) {
        ChessLog::shutdown();
    }

    return 0;
}
