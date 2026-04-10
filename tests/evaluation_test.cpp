#include "evaluation.h"
#include "board_state.h"
#include "precomp_move_data.h"

#include <iostream>
#include <string>

namespace {

bool expectTrue(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "[FAIL] " << label << '\n';
        return false;
    }

    std::cout << "[PASS] " << label << '\n';
    return true;
}

struct TablesBreakdown {
    int material = 0;
    int pst = 0;

    int total() const {
        return material + pst;
    }
};

TablesBreakdown calculateManualTables(const Chess::BoardState& board, const Chess::Evaluation& eval) {
    using namespace Chess;

    TablesBreakdown result;
    const int phase = eval.getGamePhase();

    for (int color = 0; color < 2; ++color) {
        for (int pieceType = PIECE_KING; pieceType <= PIECE_QUEEN; ++pieceType) {
            const int sign = (color == COLOR_WHITE) ? 1 : -1;
            const int material = getPieceValue(pieceType);
            const auto table = eval.getInterpolatedPieceTable(pieceType, color, phase);
            const auto& pieceList = board.getPieceList(color, pieceType);

            for (int i = 0; i < pieceList.count(); ++i) {
                const int sq = pieceList[i];
                result.material += sign * material;
                result.pst += sign * table[sq];
            }
        }
    }

    return result;
}

bool expectEqual(int actual, int expected, const std::string& label) {
    if (actual != expected) {
        std::cerr << "[FAIL] " << label << " | expected=" << expected << ", actual=" << actual << '\n';
        return false;
    }

    std::cout << "[PASS] " << label << " | value=" << actual << '\n';
    return true;
}

Chess::Evaluation makeEvaluationFromFen(const std::string& fen, Chess::BoardState& board) {
    board.loadFEN(fen);
    return Chess::Evaluation(board);
}

}

int main() {
    using namespace Chess;

    PrecomputedMoveData::initialize();

    bool ok = true;

    {
        std::cout << "\n=== Position 1: Starting position ===\n";
        BoardState board;
        board.reset();
        Evaluation eval(board);

        const int gamePhase = eval.getGamePhase();
        const int tableEval = eval.EvaluateTables();
        const int mopUpEval = eval.MopUpEval();
        const int totalEval = eval.Evaluate();
        const TablesBreakdown manual = calculateManualTables(board, eval);

        std::cout << "getGamePhase() = " << gamePhase << '\n';
        std::cout << "EvaluateTables() = " << tableEval << '\n';
        std::cout << "Manual material = " << manual.material << '\n';
        std::cout << "Manual PST = " << manual.pst << '\n';
        std::cout << "Manual material + PST = " << manual.total() << '\n';
        std::cout << "MopUpEval() = " << mopUpEval << '\n';
        std::cout << "Evaluate() = " << totalEval << '\n';

        ok &= expectEqual(gamePhase, 24, "Starting position phase should be 24");
        ok &= expectEqual(tableEval, manual.total(), "Starting position EvaluateTables matches manual material+PST");
        ok &= expectEqual(manual.material, 0, "Starting position material should be 0");
        ok &= expectEqual(mopUpEval, 0, "Starting position mop-up should be 0");
    }

    {
        std::cout << "\n=== Position 2: Kings only ===\n";
        BoardState board;
        auto eval = makeEvaluationFromFen("4k3/8/8/8/8/8/8/4K3 w - - 0 1", board);

        const int gamePhase = eval.getGamePhase();
        const int tableEval = eval.EvaluateTables();
        const int mopUpEval = eval.MopUpEval();
        const int totalEval = eval.Evaluate();
        const TablesBreakdown manual = calculateManualTables(board, eval);

        std::cout << "getGamePhase() = " << gamePhase << '\n';
        std::cout << "EvaluateTables() = " << tableEval << '\n';
        std::cout << "Manual material = " << manual.material << '\n';
        std::cout << "Manual PST = " << manual.pst << '\n';
        std::cout << "Manual material + PST = " << manual.total() << '\n';
        std::cout << "MopUpEval() = " << mopUpEval << '\n';
        std::cout << "Evaluate() = " << totalEval << '\n';

        ok &= expectEqual(gamePhase, 0, "Kings-only phase should be 0");
        ok &= expectEqual(tableEval, manual.total(), "Kings-only EvaluateTables matches manual material+PST");
        ok &= expectEqual(manual.material, 0, "Kings-only material should be 0");
        ok &= expectEqual(mopUpEval, 0, "Kings-only mop-up should be 0");
    }

    {
        std::cout << "\n=== Position 3: Mop-up bonus check (KQ vs K) ===\n";
        BoardState board;
        auto eval = makeEvaluationFromFen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1", board);

        const int tableEval = eval.EvaluateTables();
        const int mopUpEval = eval.MopUpEval();
        const int totalEval = eval.Evaluate();
        const TablesBreakdown manual = calculateManualTables(board, eval);

        std::cout << "EvaluateTables() = " << tableEval << '\n';
        std::cout << "Manual material = " << manual.material << '\n';
        std::cout << "Manual PST = " << manual.pst << '\n';
        std::cout << "Manual material + PST = " << manual.total() << '\n';
        std::cout << "MopUpEval() = " << mopUpEval << '\n';
        std::cout << "Evaluate() = " << totalEval << '\n';

        ok &= expectEqual(tableEval, manual.total(), "KQ vs K EvaluateTables matches manual material+PST");
        ok &= expectEqual(manual.material, 900, "KQ vs K material should be +900");
        ok &= expectTrue(mopUpEval > 0, "Mop-up should be positive for the materially winning side");
    }

    {
        std::cout << "\n=== Position 3b: Material sign check (K vs KQ) ===\n";
        BoardState board;
        auto eval = makeEvaluationFromFen("3qk3/8/8/8/8/8/8/4K3 w - - 0 1", board);

        const int tableEval = eval.EvaluateTables();
        const TablesBreakdown manual = calculateManualTables(board, eval);

        std::cout << "EvaluateTables() = " << tableEval << '\n';
        std::cout << "Manual material = " << manual.material << '\n';
        std::cout << "Manual PST = " << manual.pst << '\n';
        std::cout << "Manual material + PST = " << manual.total() << '\n';

        ok &= expectEqual(tableEval, manual.total(), "K vs KQ EvaluateTables matches manual material+PST");
        ok &= expectEqual(manual.material, -900, "K vs KQ material should be -900");
    }

    {
        std::cout << "\n=== Position 4: Interpolated table consistency ===\n";
        BoardState board;
        board.reset();
        Evaluation eval(board);

        const auto mgPawn = eval.getPieceSqTable(PIECE_PAWN, COLOR_WHITE, false);
        const auto egPawn = eval.getPieceSqTable(PIECE_PAWN, COLOR_WHITE, true);
        const auto i24Pawn = eval.getInterpolatedPieceTable(PIECE_PAWN, COLOR_WHITE, 24);
        const auto i0Pawn = eval.getInterpolatedPieceTable(PIECE_PAWN, COLOR_WHITE, 0);

        const int sq = BoardRepresentation::e4;
        std::cout << "MG pawn table at e4 = " << mgPawn[sq] << '\n';
        std::cout << "EG pawn table at e4 = " << egPawn[sq] << '\n';
        std::cout << "Interpolated phase=24 at e4 = " << i24Pawn[sq] << '\n';
        std::cout << "Interpolated phase=0 at e4 = " << i0Pawn[sq] << '\n';

        ok &= expectEqual(i24Pawn[sq], mgPawn[sq], "Interpolated phase=24 should match MG table");
        ok &= expectEqual(i0Pawn[sq], egPawn[sq], "Interpolated phase=0 should match EG table");

        const auto whitePawnTable = eval.getPieceSqTable(PIECE_PAWN, COLOR_WHITE, false);
        const auto blackPawnTable = eval.getPieceSqTable(PIECE_PAWN, COLOR_BLACK, false);
        std::cout << "White pawn table at a2 = " << whitePawnTable[BoardRepresentation::a2] << '\n';
        std::cout << "Black pawn table at a7 = " << blackPawnTable[BoardRepresentation::a7] << '\n';
        ok &= expectEqual(whitePawnTable[BoardRepresentation::a2], blackPawnTable[BoardRepresentation::a7],
            "White/Black pawn table orientation should mirror correctly");
    }

    {
        std::cout << "\n=== Position 5: Castling + rook development component (indirect) ===\n";
        BoardState board;
        auto eval = makeEvaluationFromFen("r3k2r/8/8/8/8/R6R/8/4K3 w - - 0 1", board);

        const int tableEval = eval.EvaluateTables();
        const int mobilityEval = eval.EvaluateMobility();
        const int mopUpEval = eval.MopUpEval();
        const int castlingEval = eval.EvaluateCastlingAndRookDevelopment();
        const int totalEval = eval.Evaluate();
        const int castlingAndRookComponent = totalEval - tableEval - mobilityEval - mopUpEval;

        std::cout << "EvaluateTables() = " << tableEval << '\n';
        std::cout << "EvaluateMobility() = " << mobilityEval << '\n';
        std::cout << "MopUpEval() = " << mopUpEval << '\n';
        std::cout << "EvaluateCastlingAndRookDevelopment() = " << castlingEval << '\n';
        std::cout << "Evaluate() = " << totalEval << '\n';
        std::cout << "Evaluate() - EvaluateTables() - EvaluateMobility() - MopUpEval() = " << castlingAndRookComponent << '\n';

        ok &= expectEqual(eval.getGamePhase(), 8, "Expected phase for this setup should be 8");
        ok &= expectEqual(castlingAndRookComponent, castlingEval,
            "Indirect castling component should match direct castling evaluation");
        ok &= expectEqual(castlingEval, 0,
            "With both sides having no castling rights at home king squares, castling component should be 0");
    }

    {
        std::cout << "\n=== Position 6: Side-to-move sign flip ===\n";
        BoardState whiteToMoveBoard;
        auto whiteEval = makeEvaluationFromFen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1", whiteToMoveBoard);
        const int whiteScore = whiteEval.Evaluate();

        BoardState blackToMoveBoard;
        auto blackEval = makeEvaluationFromFen("4k3/8/8/8/8/8/8/3QK3 b - - 0 1", blackToMoveBoard);
        const int blackScore = blackEval.Evaluate();

        std::cout << "Evaluate() with white to move = " << whiteScore << '\n';
        std::cout << "Evaluate() with black to move = " << blackScore << '\n';

        ok &= expectEqual(blackScore, -whiteScore, "Evaluate should flip sign with side to move");
    }

    {
        std::cout << "\n=== Position 7: Mobility centralization (knight) ===\n";
        BoardState centerKnightBoard;
        auto centerEval = makeEvaluationFromFen("k7/8/8/8/3N4/8/8/4K3 w - - 0 1", centerKnightBoard);
        const int centerMob = centerEval.EvaluateMobility();

        BoardState cornerKnightBoard;
        auto cornerEval = makeEvaluationFromFen("N3k3/8/8/8/8/8/8/4K3 w - - 0 1", cornerKnightBoard);
        const int cornerMob = cornerEval.EvaluateMobility();

        std::cout << "EvaluateMobility() with knight on d4 = " << centerMob << '\n';
        std::cout << "EvaluateMobility() with knight on a8 = " << cornerMob << '\n';

        ok &= expectTrue(centerMob > cornerMob,
            "Centralized knight should score higher mobility than corner knight");
    }

    {
        std::cout << "\n=== Position 8: Mobility discourages black knight retreat h6->g8 ===\n";
        BoardState h6Board;
        auto h6Eval = makeEvaluationFromFen("4k3/8/7n/8/8/8/8/4K3 b - - 0 1", h6Board);
        const int h6Mob = h6Eval.EvaluateMobility();

        BoardState g8Board;
        auto g8Eval = makeEvaluationFromFen("4k1n1/8/8/8/8/8/8/4K3 b - - 0 1", g8Board);
        const int g8Mob = g8Eval.EvaluateMobility();

        std::cout << "EvaluateMobility() with black knight on h6 = " << h6Mob << '\n';
        std::cout << "EvaluateMobility() with black knight on g8 = " << g8Mob << '\n';

        ok &= expectTrue(h6Mob < g8Mob,
            "For white-minus-black mobility score, black knight h6 should be better for black than g8");
    }

    if (!ok) {
        std::cerr << "\nEvaluation unit test FAILED\n";
        return 1;
    }

    std::cout << "\nEvaluation unit test PASSED\n";
    return 0;
}
