#ifndef MOVE_ORDER_UTIL_H
#define MOVE_ORDER_UTIL_H

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "board_state.h"
#include "move.h"
#include "pieces.h"
#include "precomp_move_data.h"
#include "evaluation.h"

namespace Chess {

class MoveOrderUtil {
public:
    static constexpr int MAX_PLY = 128;

    static int SEECapture(const BoardState& board, const Move& move) {
        if (!move.isValid()) return 0;

        const int from = move.startSquare();
        const int to = move.targetSquare();
        int side = board.getSide();
        int opponent = side ^ 1;

        const int movingPiece = board.getPieceTypeAt(from);
        if (movingPiece < 0) return 0;

        int capturedPiece = board.getPieceTypeAt(to);
        if (move.flag() == Move::Flag::EnPassantCapture) {
            capturedPiece = PIECE_PAWN;
        }
        if (capturedPiece < 0) return 0;

        std::array<std::array<uint64_t, 6>, 2> pieces = board.getPieceBoards();
        uint64_t occ = board.getMainBoard();

        std::array<int, 32> gain{};
        int depth = 0;

        int attackerPiece = move.isPromotion() ? move.promotionPieceType() : movingPiece;
        gain[depth] = getPieceValue(capturedPiece);

        removePiece(pieces, side, movingPiece, from);
        occ &= ~(1ULL << from);

        if (move.flag() == Move::Flag::EnPassantCapture) {
            const int epCapturedSq = to + (side == COLOR_WHITE ? -8 : 8);
            removePiece(pieces, opponent, PIECE_PAWN, epCapturedSq);
            occ &= ~(1ULL << epCapturedSq);
        }

        sideToMoveSwap(side, opponent);

        while (true) {
            ++depth;
            gain[depth] = getPieceValue(attackerPiece) - gain[depth - 1];

            if (std::max(-gain[depth - 1], gain[depth]) < 0) {
                break;
            }

            int nextPiece = -1;
            int nextSquare = -1;
            if (!leastValuableAttacker(pieces, occ, side, to, nextPiece, nextSquare)) {
                break;
            }

            removePiece(pieces, side, nextPiece, nextSquare);
            occ &= ~(1ULL << nextSquare);

            attackerPiece = nextPiece;
            sideToMoveSwap(side, opponent);
        }

        while (--depth) {
            gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        }

        return gain[0];
    }

    static bool isCaptureMove(const BoardState& board, const Move& move) {
        if (!move.isValid()) return false;
        if (move.flag() == Move::Flag::EnPassantCapture) return true;
        return board.getPieceTypeAt(move.targetSquare()) >= 0;
    }

    static void orderMoves(const BoardState& board, const std::vector<Move>& moves,
                            int ply, const Move& ttMove, std::vector<Move>& orderedMoves) {
        std::vector<std::pair<int, Move>> goodCaptures;
        std::vector<std::pair<int, Move>> badCaptures;
        std::vector<Move> killerQuiets;
        std::vector<Move> historyQuiets;

        for (const Move& move : moves) {
            if (!move.isValid() || move == ttMove) continue;

            if (isCaptureMove(board, move)) {
                int see = SEECapture(board, move);
                if (see >= 0) {
                    goodCaptures.emplace_back(see, move);
                } else {
                    badCaptures.emplace_back(see, move);
                }
            } else {
                // Classify quiets into killer or history
                if (ply >= 0 && ply < MAX_PLY && 
                    (move == killers_[ply][0] || move == killers_[ply][1])) {
                    killerQuiets.push_back(move);
                } else {
                    historyQuiets.push_back(move);
                }
            }
        }

        // Sort captures by SEE
        auto sortByScore = [](const std::pair<int, Move>& a, const std::pair<int, Move>& b) {
            return a.first > b.first;
        };
        std::stable_sort(goodCaptures.begin(), goodCaptures.end(), sortByScore);
        std::stable_sort(badCaptures.begin(), badCaptures.end(), sortByScore);

        // Sort history quiets by history score
        std::vector<std::pair<int, Move>> scoredHistory;
        scoredHistory.reserve(historyQuiets.size());
        for (const Move& move : historyQuiets) {
            int score = history_[COLOR_WHITE][move.startSquare()][move.targetSquare()]
                + history_[COLOR_BLACK][move.startSquare()][move.targetSquare()];
            scoredHistory.emplace_back(score, move);
        }
        std::stable_sort(scoredHistory.begin(), scoredHistory.end(), sortByScore);

        // Assemble final move order
        orderedMoves.clear();
        orderedMoves.reserve(moves.size());

        // TT move
        if (ttMove.isValid()) {
            orderedMoves.push_back(ttMove);
        }

        // Good captures (SEE >= 0)
        for (const auto& [see, move] : goodCaptures) {
            orderedMoves.push_back(move);
        }

        // Killer quiets
        orderedMoves.insert(orderedMoves.end(), killerQuiets.begin(), killerQuiets.end());

        // History quiets
        for (const auto& [score, move] : scoredHistory) {
            orderedMoves.push_back(move);
        }

        // Bad captures (SEE < 0)
        for (const auto& [see, move] : badCaptures) {
            orderedMoves.push_back(move);
        }
    }

    static void orderMoves(const BoardState& board, const std::vector<Move>& moves,
                           std::vector<Move>& orderedMoves) {
        orderMoves(board, moves, -1, Move::invalid(), orderedMoves);
    }

    static void updateKiller(const Move& move, int ply) {
        if (ply < 0 || ply >= MAX_PLY || !move.isValid()) return;
        if (killers_[ply][0] == move) return;
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = move;
    }

    static void updateHistory(const Move& move, int side, int depth) {
        if (!move.isValid() || side < 0 || side > 1) return;
        const int from = move.startSquare();
        const int to = move.targetSquare();
        if (from < 0 || from >= 64 || to < 0 || to >= 64) return;

        const int bonus = depth * depth;
        int& entry = history_[side][from][to];
        entry += bonus;
        if (entry > 1'000'000) entry = 1'000'000;
    }

    static void clearHeuristics() {
        killers_ = {};
        history_ = {};
    }

private:
    using KillerTable = std::array<std::array<Move, 2>, MAX_PLY>;
    using HistoryTable = std::array<std::array<std::array<int, 64>, 64>, 2>; // [side][from][to]

    inline static KillerTable killers_{};
    inline static HistoryTable history_{};

    static void sideToMoveSwap(int& side, int& opponent) {
        const int tmp = side;
        side = opponent;
        opponent = tmp;
    }

    static void removePiece(std::array<std::array<uint64_t, 6>, 2>& pieces, int color, int pieceType, int square) {
        if (color < 0 || color > 1 || pieceType < 0 || pieceType > PIECE_QUEEN) return;
        pieces[color][pieceType] &= ~(1ULL << square);
    }

    static bool leastValuableAttacker(const std::array<std::array<uint64_t, 6>, 2>& pieces,
                                      uint64_t occ, int color, int target, int& outPiece,
                                      int& outSquare) {

        static constexpr std::array<int, 6> order = {
            PIECE_PAWN, PIECE_KNIGHT, PIECE_BISHOP, PIECE_ROOK, PIECE_QUEEN, PIECE_KING
        };

        for (int pieceType : order) {
            uint64_t bb = pieces[color][pieceType];
            while (bb) {
                const int sq = static_cast<int>(getLSB(bb));
                bb &= (bb - 1);

                if (pieceAttacksSquare(pieceType, color, sq, target, occ)) {
                    outPiece = pieceType;
                    outSquare = sq;
                    return true;
                }
            }
        }

        return false;
    }

    static bool pieceAttacksSquare(int pieceType, int color, int from, int target, uint64_t occ) {
        switch (pieceType) {
        case PIECE_PAWN: {
            const int fileFrom = BoardRepresentation::FileIndex(from);
            const int fileTo = BoardRepresentation::FileIndex(target);
            const int delta = target - from;
            if (color == COLOR_WHITE) {
                return (delta == 7 || delta == 9) && std::abs(fileTo - fileFrom) == 1;
            }
            return (delta == -7 || delta == -9) && std::abs(fileTo - fileFrom) == 1;
        }
        case PIECE_KNIGHT:
            return (PrecomputedMoveData::getKnightAttacks(from) & (1ULL << target)) != 0;
        case PIECE_KING:
            return (PrecomputedMoveData::getKingMoves(from) & (1ULL << target)) != 0;
        case PIECE_BISHOP:
            return slidingAttacksSquare(from, target, occ, true, false);
        case PIECE_ROOK:
            return slidingAttacksSquare(from, target, occ, false, true);
        case PIECE_QUEEN:
            return slidingAttacksSquare(from, target, occ, true, true);
        default:
            return false;
        }
    }

    static bool slidingAttacksSquare(int from, int target, uint64_t occ, bool bishopLike, bool rookLike) {
        const int dir = PrecomputedMoveData::getDirection(from, target);
        if (dir < 0) return false;

        const bool isDiagonal = (dir >= 4);
        if (isDiagonal && !bishopLike) return false;
        if (!isDiagonal && !rookLike) return false;

        const int step = PrecomputedMoveData::dirOffsets[dir];
        int sq = from + step;
        while (sq != target) {
            if (sq < 0 || sq >= 64) return false;
            if (occ & (1ULL << sq)) return false;
            sq += step;
        }
        return true;
    }
};

} // namespace Chess

#endif // MOVE_ORDER_UTIL_H

