#ifndef EVALUATION_H
#define EVALUATION_H

#include <array>
#include <algorithm>

#include "pieces.h"
#include "magics.h" 
#include "board_rep.h"
#include "board_state.h"
#include "precomp_move_data.h"
#include "logger.h"

namespace Chess {

    struct EvaluationOptions {
        bool useMobilityEvaluation = true;
        bool useProtectedPassedPawn = true;
        bool usePawnStructure = true;
        bool useKingSafety = true;
        bool useVirtualKingMobility = true;
        bool usePieceEvaluations = true;
        int contemptScore = 0;
    };

    class Evaluation {
    public:
        explicit Evaluation(const BoardState& board, EvaluationOptions options = {})
            : board(board), options(options) {}

        ~Evaluation() = default;

        int Evaluate() const {
            int evaluation = EvaluateTables();
            if (options.useMobilityEvaluation) evaluation += EvaluateMobility();
            evaluation += MopUpEval();
            evaluation += EvaluateCastlingAndRookDevelopment();
            if (options.usePawnStructure) evaluation += EvaluatePawnStructure();
            if (options.useKingSafety) evaluation += EvaluateKingSafety();
            if (options.usePieceEvaluations) {
                evaluation += EvaluateKnights();
                evaluation += EvaluateBishops();
                evaluation += EvaluateRooks();
            }
            return (board.getSide() == COLOR_WHITE) ? evaluation : -evaluation;
        }

        int EvaluateTables() const {
            int evaluation = 0;
            const int phase = getGamePhase();

            for (int color = 0; color < 2; ++color) {
                for (int pieceType = PIECE_KING; pieceType <= PIECE_QUEEN; ++pieceType) {
                    const auto table = getInterpolatedPieceTable(pieceType, color, phase);
                    const int pieceValue = getPieceValue(pieceType);
                    const auto& pieceList = board.getPieceList(color, pieceType);

                    for (int i = 0; i < pieceList.count(); ++i) {
                        const int sq = pieceList[i];
                        const int value = pieceValue + static_cast<int>(table[sq]);
                        evaluation += (color == COLOR_WHITE) ? value : -value;
                    }
                }
            }
            return evaluation;
        }

        int MopUpEval() const {
            auto sideMopUp = [this](int attacker, int defender) {
                const int attackerMaterial = getMaterialWithoutKing(attacker);
                const int defenderMaterial = getMaterialWithoutKing(defender);
                const int materialDiff = attackerMaterial - defenderMaterial;
                if (materialDiff < 200) return 0;

                const int attackerKingSq = getKingSquare(attacker);
                const int defenderKingSq = getKingSquare(defender);
                if (attackerKingSq < 0 || defenderKingSq < 0) return 0;

                const int defenderKingCenterDist = PrecomputedMoveData::getCenterManhattanDistance(defenderKingSq);
                const int kingsDist = PrecomputedMoveData::getKingDistance(attackerKingSq, defenderKingSq);

                int bonus = defenderKingCenterDist * 10;
                bonus += (14 - kingsDist) * 4;
                return bonus;
            };

            const int whiteBonus = sideMopUp(COLOR_WHITE, COLOR_BLACK);
            const int blackBonus = sideMopUp(COLOR_BLACK, COLOR_WHITE);
            return whiteBonus - blackBonus;
        }

        int EvaluateCastlingAndRookDevelopment() const {
            const int phase = getGamePhase();

            auto sidePenalty = [this, phase](int color) {
                const int kingSq = getKingSquare(color);
                if (kingSq < 0) return 0;

                const int kingHome = (color == COLOR_WHITE) ? BoardRepresentation::e1 : BoardRepresentation::e8;
                if (kingSq != kingHome) return 0;

                int penalty = 0;
                const int rights = board.getCastleRights();
                const bool hasKingSideRight = (color == COLOR_WHITE)
                    ? ((rights & 0x01) != 0)
                    : ((rights & 0x04) != 0);
                const bool hasQueenSideRight = (color == COLOR_WHITE)
                    ? ((rights & 0x02) != 0)
                    : ((rights & 0x08) != 0);
                const bool hasAnyCastlingRight = hasKingSideRight || hasQueenSideRight;

                const int kingFile = BoardRepresentation::FileIndex(kingSq);
                const bool kingCentral = (kingFile >= 3 && kingFile <= 4);
                if (!hasAnyCastlingRight && kingCentral) {
                    penalty += 40;
                }

                // Penalize permanently lost castling rights while king is still uncastled.
                // This avoids rewarding artificial rook shuffles back to the home square.
                if (!hasKingSideRight) penalty += 18;
                if (!hasQueenSideRight) penalty += 12;

                return (penalty * phase) / 24;
                };

            return sidePenalty(COLOR_WHITE) - sidePenalty(COLOR_BLACK);
        }

        int EvaluateKingSafety() const {
            auto sideSafety = [this](int color) {
                const int kingSq = getKingSquare(color);
                if (kingSq < 0) return 0;

                const int opponent = color ^ 1;
                const uint64_t ownPawns = board.getPieceBoards()[color][PIECE_PAWN];
                const uint64_t oppPawns = board.getPieceBoards()[opponent][PIECE_PAWN];

                const int kingFile = BoardRepresentation::FileIndex(kingSq);
                int penalty = 0;

                // Pawn shield + (semi-)open files around king file.
                const int nearShieldRank = (color == COLOR_WHITE) ? 1 : 6;
                const int farShieldRank = (color == COLOR_WHITE) ? 2 : 5;
                for (int f = std::max(0, kingFile - 1); f <= std::min(7, kingFile + 1); ++f) {
                    const uint64_t fileMask = BoardRepresentation::fileMask(f);

                    const bool hasNearShieldPawn = (ownPawns & (1ULL << (nearShieldRank * 8 + f))) != 0ULL;
                    const bool hasFarShieldPawn = (ownPawns & (1ULL << (farShieldRank * 8 + f))) != 0ULL;

                    if (!hasNearShieldPawn) penalty += KING_SHIELD_MISSING_NEAR_PAWN_PENALTY;
                    else if (!hasFarShieldPawn) penalty += KING_SHIELD_MISSING_FAR_PAWN_PENALTY;

                    const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
                    const bool oppPawnOnFile = (oppPawns & fileMask) != 0ULL;
                    if (!ownPawnOnFile && !oppPawnOnFile) penalty += KING_OPEN_FILE_PENALTY;
                    else if (!ownPawnOnFile && oppPawnOnFile) penalty += KING_SEMI_OPEN_FILE_PENALTY;
                }

                // Pawn storm near king wing (kept lower than open-file penalties).
                const auto& oppPawnList = board.getPieceList(opponent, PIECE_PAWN);
                for (int i = 0; i < oppPawnList.count(); ++i) {
                    const int sq = oppPawnList[i];
                    const int file = BoardRepresentation::FileIndex(sq);
                    if (file < kingFile - 1 || file > kingFile + 1) continue;

                    const int rank = BoardRepresentation::RankIndex(sq);
                    if (color == COLOR_WHITE) {
                        if (rank <= 2) penalty += KING_PAWN_STORM_NEAR_PENALTY;
                        else if (rank == 3) penalty += KING_PAWN_STORM_MID_PENALTY;
                        else if (rank == 4) penalty += KING_PAWN_STORM_FAR_PENALTY;
                    } else {
                        if (rank >= 5) penalty += KING_PAWN_STORM_NEAR_PENALTY;
                        else if (rank == 4) penalty += KING_PAWN_STORM_MID_PENALTY;
                        else if (rank == 3) penalty += KING_PAWN_STORM_FAR_PENALTY;
                    }
                }

                if (options.useVirtualKingMobility) {
                    // Virtual mobility: queen mobility from king square as a proxy for king exposure.
                    // Higher mobility means more open lines around the king.
                    const uint64_t occNoKing = board.getMainBoard() & ~(1ULL << kingSq);
                    const uint64_t ownOccNoKing = board.getOccupancy(color) & ~(1ULL << kingSq);
                    const uint64_t virtualQueenAttacks =
                        (bishopAttacks(kingSq, occNoKing) | rookAttacks(kingSq, occNoKing)) & ~ownOccNoKing;

                    const int virtualMobility = popCount(virtualQueenAttacks);

                    // Scale this term by enemy sliding material presence to reduce false positives.
                    const int enemySliders =
                        board.getPieceList(opponent, PIECE_BISHOP).count() +
                        board.getPieceList(opponent, PIECE_ROOK).count() +
                        2 * board.getPieceList(opponent, PIECE_QUEEN).count();

                    penalty += (virtualMobility * enemySliders) / VIRTUAL_KING_MOBILITY_DIVISOR;
                }

                // Scale with opponent non-pawn material.
                const int oppAttackMat = getNonPawnMaterial(opponent);
                const int scaledPenalty = (penalty * std::clamp(oppAttackMat, 0, 3200)) / 3200;
                return -scaledPenalty;
            };

            return sideSafety(COLOR_WHITE) - sideSafety(COLOR_BLACK);
        }

        int EvaluatePawnStructure() const {
            auto evaluateSide = [this](int color) {
                const int opponent = color ^ 1;
                const uint64_t pawns = board.getPieceBoards()[color][PIECE_PAWN];
                
                if (pawns == 0) return 0;
                
                int score = 0;
                
                uint64_t stops = (color == COLOR_WHITE) 
                    ? (pawns << 8) 
                    : (pawns >> 8);
                
                uint64_t ownAttackSpans = 0ULL;
                const auto& pawnList = board.getPieceList(color, PIECE_PAWN);
                for (int i = 0; i < pawnList.count(); ++i) {
                    const int pawnSq = pawnList[i];
                    ownAttackSpans |= getPawnAttackSpan(pawnSq, color);
                }
                
                uint64_t oppPawnAttacks = pawnAttackMap(opponent);
                uint64_t backwardStops = (stops & oppPawnAttacks & ~ownAttackSpans);
                uint64_t backwardPawns = (color == COLOR_WHITE)
                    ? (backwardStops >> 8)
                    : (backwardStops << 8);
                
                score -= popCount(backwardPawns) * PAWN_BACKWARD_PENALTY;
                
                uint64_t openPawns = getOpenPawns(color);
                score -= popCount(openPawns) * PAWN_OPEN_PENALTY;
                
                uint64_t stragglerPawns = getStragglerPawns(color);
                score -= popCount(stragglerPawns) * PAWN_STRAGGLER_PENALTY;

                uint64_t isolatedPawns = getIsolatedPawns(color);
                score -= popCount(isolatedPawns) * PAWN_ISOLATED_PENALTY;

                uint64_t halfIsolatedPawns = getHalfIsolatedPawns(color);
                score -= popCount(halfIsolatedPawns) * PAWN_HALF_ISOLATED_PENALTY;

                uint64_t doubledPawns = getDoubledPawns(color);
                score -= popCount(doubledPawns) * PAWN_DOUBLED_PENALTY;

                uint64_t duoPawns = getPawnDuos(pawns);
                score += popCount(duoPawns) * PAWN_DUO_BONUS;

                uint64_t defendedOpenPawns = getDefendedOpenPawns(color);
                score += popCount(defendedOpenPawns) * PAWN_DEFENDED_OPEN_BONUS;
                
                return score;
            };
            
            return evaluateSide(COLOR_WHITE) - evaluateSide(COLOR_BLACK);
        }

        int EvaluateKnights() const {
            auto sideEval = [this](int color) {
                int score = 0;
                const int opponent = color ^ 1;
                const uint64_t ownPawnAttacks = pawnAttackMap(color);
                const int totalPawns = getTotalPawnCount();
                const int centralRamCount = getCentralRamCount();

                const auto& knights = board.getPieceList(color, PIECE_KNIGHT);
                for (int i = 0; i < knights.count(); ++i) {
                    const int sq = knights[i];
                    const uint64_t sqMask = (1ULL << sq);

                    // Simple outpost: on opponent half, defended by pawn, not attackable by enemy pawns.
                    const int rank = BoardRepresentation::RankIndex(sq);
                    const bool onOpponentHalf = (color == COLOR_WHITE) ? (rank >= 4) : (rank <= 3);
                    const bool pawnDefended = (ownPawnAttacks & sqMask) != 0ULL;
                    const bool enemyPawnCanChallenge = isSquareInPawnFrontSpan(opponent, sq);
                    if (onOpponentHalf && pawnDefended && !enemyPawnCanChallenge) {
                        score += KNIGHT_OUTPOST_BONUS;
                    }

                    // Knights get weaker as pawns disappear.
                    score += (totalPawns - KNIGHT_PAWN_SCALE_BASE);

                    // Closed center (many rams) favors knights.
                    score += centralRamCount * KNIGHT_CLOSED_CENTER_BONUS_PER_RAM;

                    // Penalty for minor piece not defended by a pawn.
                    if (!pawnDefended) score -= MINOR_UNDEFENDED_BY_PAWN_PENALTY;
                }

                return score;
            };

            return sideEval(COLOR_WHITE) - sideEval(COLOR_BLACK);
        }

        int EvaluateBishops() const {
            auto sideEval = [this](int color) {
                int score = 0;
                const int opponent = color ^ 1;
                const uint64_t ownPawns = board.getPieceBoards()[color][PIECE_PAWN];
                const uint64_t ownPawnAttacks = pawnAttackMap(color);
                const int centralRamCount = getCentralRamCount();

                const auto& bishops = board.getPieceList(color, PIECE_BISHOP);
                const auto& ownKnights = board.getPieceList(color, PIECE_KNIGHT);
                const auto& oppBishops = board.getPieceList(opponent, PIECE_BISHOP);
                const auto& oppKnights = board.getPieceList(opponent, PIECE_KNIGHT);

                // Bishop pair.
                if (bishops.count() >= 2) score += BISHOP_PAIR_BONUS;

                // Bishop vs knight preference shifts with center openness.
                const int openness = 4 - std::min(4, centralRamCount);
                score += (bishops.count() - ownKnights.count()) * openness;
                score -= (oppBishops.count() - oppKnights.count()) * openness;

                for (int i = 0; i < bishops.count(); ++i) {
                    const int sq = bishops[i];
                    const uint64_t sqMask = (1ULL << sq);

                    // Bad bishop: low mobility + many own pawns on same color squares.
                    const uint64_t attacks = bishopAttacks(sq, board.getMainBoard()) & ~board.getOccupancy(color);
                    const int mob = popCount(attacks);
                    const bool light = isLightSquare(sq);
                    const int ownPawnsSameColor = countPawnsOnColor(ownPawns, light);
                    if (mob <= BAD_BISHOP_MOBILITY_THRESHOLD && ownPawnsSameColor >= BAD_BISHOP_SAME_COLOR_PAWN_THRESHOLD) {
                        score -= BAD_BISHOP_PENALTY;
                    }

                    // In blocked centers bishops are generally less effective.
                    score -= centralRamCount * BISHOP_BLOCKED_CENTER_PENALTY_PER_RAM;

                    if (isFianchettoBishop(sq, color, ownPawns)) {
                        score += FIANCHETTO_BISHOP_BONUS;
                    }

                    // Penalty for minor piece not defended by a pawn.
                    if ((ownPawnAttacks & sqMask) == 0ULL) score -= MINOR_UNDEFENDED_BY_PAWN_PENALTY;
                }

                return score;
            };

            return sideEval(COLOR_WHITE) - sideEval(COLOR_BLACK);
        }

        int EvaluateRooks() const {
            auto sideEval = [this](int color) {
                int score = 0;
                const int opponent = color ^ 1;
                const uint64_t ownPawns = board.getPieceBoards()[color][PIECE_PAWN];
                const uint64_t oppPawns = board.getPieceBoards()[opponent][PIECE_PAWN];
                const int totalPawns = getTotalPawnCount();

                const auto& rooks = board.getPieceList(color, PIECE_ROOK);
                for (int i = 0; i < rooks.count(); ++i) {
                    const int sq = rooks[i];
                    const int file = BoardRepresentation::FileIndex(sq);
                    const int rank = BoardRepresentation::RankIndex(sq);
                    const uint64_t fileMask = BoardRepresentation::fileMask(file);

                    const bool ownPawnOnFile = (ownPawns & fileMask) != 0ULL;
                    const bool oppPawnOnFile = (oppPawns & fileMask) != 0ULL;

                    // Rook on open / semi-open file.
                    if (!ownPawnOnFile && !oppPawnOnFile) score += ROOK_OPEN_FILE_BONUS;
                    else if (!ownPawnOnFile && oppPawnOnFile) score += ROOK_SEMI_OPEN_FILE_BONUS;

                    // Rook on 7th rank.
                    if ((color == COLOR_WHITE && rank == 6) || (color == COLOR_BLACK && rank == 1)) {
                        score += ROOK_SEVENTH_RANK_BONUS;
                    }

                    // Rooks get stronger as pawns disappear.
                    score += (ROOK_PAWN_SCALE_BASE - totalPawns);
                }

                return score;
            };

            return sideEval(COLOR_WHITE) - sideEval(COLOR_BLACK);
        }

        int EvaluateMobility() const {
            const int phase = getGamePhase();

            auto sideMob = [this, phase](int color) {
                const int opponent = color ^ 1;
                const uint64_t occ = board.getMainBoard();
                const uint64_t ownOcc = board.getOccupancy(color);
                const uint64_t allPawns = board.getPieceBoards()[COLOR_WHITE][PIECE_PAWN]
                                      | board.getPieceBoards()[COLOR_BLACK][PIECE_PAWN];

                const uint64_t center = BoardRepresentation::makeSqMask(2, 5, 2, 5);      // c3-f6
                const uint64_t centerInner = BoardRepresentation::makeSqMask(3, 4, 3, 4); // d4-e5
                const uint64_t c3 = centerInner | BoardRepresentation::rankMask(color == COLOR_WHITE ? 1 : 6);
                const uint64_t mb = mobilityMask(color, occ) & center;

                const int enemyKingSq = getKingSquare(opponent);
                const uint64_t enemyPawns = board.getPieceBoards()[opponent][PIECE_PAWN];
                const uint64_t kingRing = (enemyKingSq >= 0)
                    ? (PrecomputedMoveData::getKingMoves(enemyKingSq) & ~allPawns)
                    : 0ULL;

                uint64_t slidingOcc = occ;
                slidingOcc ^= board.getPieceBoards()[COLOR_WHITE][PIECE_QUEEN];
                slidingOcc ^= board.getPieceBoards()[COLOR_BLACK][PIECE_QUEEN];
                if (enemyKingSq >= 0) slidingOcc ^= (1ULL << enemyKingSq);

                int score = 0;
                int kingAttackPressure = 0;

                auto mobilityScore = [mb, c3](uint64_t attacks) {
                    return popCount(attacks) + popCount(attacks & mb) + popCount(attacks & mb & c3);
                };

                const auto& pawns = board.getPieceList(color, PIECE_PAWN);
                for (int i = 0; i < pawns.count(); ++i) {
                    if (isPassedPawn(pawns[i], color, enemyPawns)) {
                        int rank = BoardRepresentation::RankIndex(pawns[i]);
                        int squaresFromProm = (color == COLOR_WHITE) ? 7 - rank : rank;
                        int distBonus = (7 - squaresFromProm);
                        score += distBonus * distBonus * 4;

                        const int extraBonus = (24 - phase) / 2;
                        const int defenderKingSq = getKingSquare(opponent);
                        if (defenderKingSq >= 0) {
                            const int kingDist = PrecomputedMoveData::getKingDistance(defenderKingSq, pawns[i]);
                            if (kingDist > squaresFromProm) {  // King can't stop it
                                score += extraBonus;
                            }
                        }
                        
                        if (options.useProtectedPassedPawn) {
                            // Bonus for protected passed pawns
                            if (isPawnProtected(pawns[i], color)) {
                                score += 20;
                            }
                        }
                    };
                }

                const auto& knights = board.getPieceList(color, PIECE_KNIGHT);
                for (int i = 0; i < knights.count(); ++i) {
                    const uint64_t attacks = PrecomputedMoveData::getKnightAttacks(knights[i]) & ~ownOcc;
                    kingAttackPressure += popCount(attacks & kingRing);
                    score += mobilityScore(attacks) * 4;
                }

                const auto& bishops = board.getPieceList(color, PIECE_BISHOP);
                for (int i = 0; i < bishops.count(); ++i) {
                    const uint64_t attacks = bishopAttacks(bishops[i], slidingOcc) & ~ownOcc;
                    kingAttackPressure += popCount(attacks & kingRing);
                    score += mobilityScore(attacks) * 4;
                }

                const auto& rooks = board.getPieceList(color, PIECE_ROOK);
                for (int i = 0; i < rooks.count(); ++i) {
                    const uint64_t attacks = rookAttacks(rooks[i], slidingOcc) & ~ownOcc;
                    kingAttackPressure += popCount(attacks & kingRing);
                    score += mobilityScore(attacks) * (2 + (24 - phase) / 12);
                }

                const auto& queens = board.getPieceList(color, PIECE_QUEEN);
                for (int i = 0; i < queens.count(); ++i) {
                    const uint64_t attacks = (bishopAttacks(queens[i], slidingOcc) | rookAttacks(queens[i], slidingOcc)) & ~ownOcc;
                    kingAttackPressure += popCount(attacks & kingRing);
                    score += mobilityScore(attacks) * (1 + (24 - phase) / 8);
                }

                const int ownKingSq = getKingSquare(color);
                if (ownKingSq >= 0) {
                    const uint64_t kingMoves = PrecomputedMoveData::getKingMoves(ownKingSq) & ~ownOcc;
                    // Encourage king activity mostly in endgames.
                    score += popCount(kingMoves) * ((24 - phase) / 6);
                }

                // Similar to Oli's katt scaling: increase king pressure bonus as material comes off.
                score += kingAttackPressure * (2 + (24 - phase) / 6);

                return score;
            };

            return sideMob(COLOR_WHITE) - sideMob(COLOR_BLACK);
        }

        int getGamePhase() const {
            int phase = 0;

            for (int color = 0; color < 2; ++color) {
                for (int pieceType = PIECE_KING; pieceType <= PIECE_QUEEN; ++pieceType) {
                    const int inc = getPhaseIncrement(pieceType);
                    if (inc == 0) continue;
                    const auto& list = board.getPieceList(color, pieceType);
                    phase += list.count() * inc;
                }
            }

            return std::clamp(phase, 0, 24);
        }

        std::array<int, 64> getPieceSqTable(int pieceType, int color, bool isEndGame) const {
            std::array<int, 64> table{};
            const std::array<int, 64>* base = nullptr;

            switch (pieceType) {
            case PIECE_PAWN: base = isEndGame ? &EG_PAWN_TABLE : &MG_PAWN_TABLE; break;
            case PIECE_KNIGHT: base = isEndGame ? &EG_KNIGHT_TABLE : &MG_KNIGHT_TABLE; break;
            case PIECE_BISHOP: base = isEndGame ? &EG_BISHOP_TABLE : &MG_BISHOP_TABLE; break;
            case PIECE_ROOK: base = isEndGame ? &EG_ROOK_TABLE : &MG_ROOK_TABLE; break;
            case PIECE_QUEEN: base = isEndGame ? &EG_QUEEN_TABLE : &MG_QUEEN_TABLE; break;
            case PIECE_KING: base = isEndGame ? &EG_KING_TABLE : &MG_KING_TABLE; break;
            default: return table;
            }

            for (int sq = 0; sq < 64; ++sq) {
                const int idx = toTableIndex(sq, color);
                table[sq] = (*base)[idx];
            }

            return table;
        }

        std::array<int, 64> getInterpolatedPieceTable(int pieceType, int color, int phase) const {
            std::array<int, 64> table{};
            const std::array<int, 64>* mgBase = nullptr;
            const std::array<int, 64>* egBase = nullptr;

            switch (pieceType) {
            case PIECE_PAWN: mgBase = &MG_PAWN_TABLE; egBase = &EG_PAWN_TABLE; break;
            case PIECE_KNIGHT: mgBase = &MG_KNIGHT_TABLE; egBase = &EG_KNIGHT_TABLE; break;
            case PIECE_BISHOP: mgBase = &MG_BISHOP_TABLE; egBase = &EG_BISHOP_TABLE; break;
            case PIECE_ROOK: mgBase = &MG_ROOK_TABLE; egBase = &EG_ROOK_TABLE; break;
            case PIECE_QUEEN: mgBase = &MG_QUEEN_TABLE; egBase = &EG_QUEEN_TABLE; break;
            case PIECE_KING: mgBase = &MG_KING_TABLE; egBase = &EG_KING_TABLE; break;
            default: return table;
            }

            const int mgPhase = std::clamp(phase, 0, 24);
            const int egPhase = 24 - mgPhase;

            for (int sq = 0; sq < 64; ++sq) {
                const int idx = toTableIndex(sq, color);
                const int mg = (*mgBase)[idx];
                const int eg = (*egBase)[idx];
                table[sq] = (mg * mgPhase + eg * egPhase) / 24;
            }

            return table;
        }

        void setOptions(const EvaluationOptions& newOptions) {
            options = newOptions;
        }

        const EvaluationOptions& getOptions() const {
            return options;
        }

    private:
        const BoardState& board;
        EvaluationOptions options;


        uint64_t pawnAttackMap(int color) const {
            uint64_t attacks = 0ULL;
            const auto& pawns = board.getPieceList(color, PIECE_PAWN);
            for (int i = 0; i < pawns.count(); ++i) {
                attacks |= PrecomputedMoveData::getPawnAttackBitboard(color, pawns[i]);
            }
            return attacks;
        }

        uint64_t mobilityMask(int color, uint64_t occ) const {
            const int opponent = color ^ 1;
            const uint64_t ownPawns = board.getPieceBoards()[color][PIECE_PAWN];
            const uint64_t blockedPawns = (color == COLOR_WHITE)
                ? (ownPawns & (occ >> 8))
                : (ownPawns & (occ << 8));
            const uint64_t enemyPawnAttacks = pawnAttackMap(opponent);
            return ~(blockedPawns | enemyPawnAttacks);
        }


        // These functions for Attacks and temporary
        static uint64_t bishopAttacks(int square, uint64_t occ) {
            MagicEntry m = bishopMagics[square];
            const uint64_t blocking = occ | ~m.mask;
            const uint64_t index = (blocking * m.magic) >> m.shift;
            uint64_t attacks = m.ptr[index];

            return attacks;
        }

        static uint64_t rookAttacks(int square, uint64_t occ) {
            MagicEntry m = rookMagics[square];
            const uint64_t blocking = occ | ~m.mask;
            const uint64_t index = (blocking * m.magic) >> m.shift;
            uint64_t attacks = m.ptr[index];

            return attacks;
        }

        static bool isPassedPawn(int square, int color, uint64_t enemyPawns) {
            const int file = BoardRepresentation::FileIndex(square);
            const int rank = BoardRepresentation::RankIndex(square);
        
            uint64_t fileMask = BoardRepresentation::fileMask(file);
            uint64_t fileMaskLeft = (file > 0) ? BoardRepresentation::fileMask(file - 1) : 0;
            uint64_t fileMaskRight = (file < 7) ? BoardRepresentation::fileMask(file + 1) : 0;
            uint64_t blockingMask = fileMask | fileMaskLeft | fileMaskRight;
        
            if (color == COLOR_WHITE) {
                blockingMask &= (0xFFFFFFFFFFFFFFFFULL << (8 * (rank + 1)));
            } else {
                blockingMask &= (0xFFFFFFFFFFFFFFFFULL >> (8 * (7 - rank)));
            }
        
            return (blockingMask & enemyPawns) == 0;
        }

        bool isPawnProtected(int square, int color) const {
            const uint64_t ownAttacks = board.getAttackTable(color);
            return (ownAttacks & (1ULL << square)) != 0;
        }

        uint64_t getPawnAttackSpan(int square, int color) const {
            // Get the attack span of a single pawn using precomputed data
            // This is all squares the pawn can defend along its diagonals
            uint64_t attacks = 0ULL;
            
            if (color == COLOR_WHITE) {
                const int file = BoardRepresentation::FileIndex(square);
                const int rank = BoardRepresentation::RankIndex(square);
                
                // Add all pawn attacks from ranks ahead
                for (int r = rank + 1; r < 8; ++r) {
                    const int sq = r * 8 + file;
                    attacks |= PrecomputedMoveData::getPawnAttackBitboard(COLOR_WHITE, sq);
                }
            } else {
                const int file = BoardRepresentation::FileIndex(square);
                const int rank = BoardRepresentation::RankIndex(square);
                
                // Add all pawn attacks from ranks behind
                for (int r = rank - 1; r >= 0; --r) {
                    const int sq = r * 8 + file;
                    attacks |= PrecomputedMoveData::getPawnAttackBitboard(COLOR_BLACK, sq);
                }
            }
            
            return attacks;
        }

        uint64_t getOpenPawns(int color) const {
            // Open pawns: not blocked by enemy pawn front spans
            const uint64_t ownPawns = board.getPieceBoards()[color][PIECE_PAWN];
            const int opponent = color ^ 1;
            
            // Get enemy front span (all squares attacked by enemy pawns going forward)
            uint64_t enemyFrontSpan = 0ULL;
            const auto& oppPawnList = board.getPieceList(opponent, PIECE_PAWN);
            for (int i = 0; i < oppPawnList.count(); ++i) {
                const int pawnSq = oppPawnList[i];
                enemyFrontSpan |= getPawnAttackSpan(pawnSq, opponent);
            }
            
            return ownPawns & ~enemyFrontSpan;
        }

        uint64_t getStragglerPawns(int color) const {
            // Straggler pawns: backward AND open AND on early ranks (rank 2-3 for white, rank 6-5 for black)
            const uint64_t pawns = board.getPieceBoards()[color][PIECE_PAWN];
            const int opponent = color ^ 1;
            
            if (pawns == 0) return 0ULL;
            
            // Get backward pawns
            uint64_t stops = (color == COLOR_WHITE) 
                ? (pawns << 8) 
                : (pawns >> 8);
            
            uint64_t ownAttackSpans = 0ULL;
            const auto& pawnList = board.getPieceList(color, PIECE_PAWN);
            for (int i = 0; i < pawnList.count(); ++i) {
                const int pawnSq = pawnList[i];
                ownAttackSpans |= getPawnAttackSpan(pawnSq, color);
            }
            
            uint64_t oppPawnAttacks = pawnAttackMap(opponent);
            uint64_t backwardPawns = (color == COLOR_WHITE)
                ? ((stops & oppPawnAttacks & ~ownAttackSpans) >> 8)
                : ((stops & oppPawnAttacks & ~ownAttackSpans) << 8);
            
            // Get open pawns
            uint64_t openPawns = getOpenPawns(color);
            
            // Early ranks: rank 2-3 for white (0x00FF0000), rank 6-5 for black (0x0000FF00)
            uint64_t earlyRankMask = (color == COLOR_WHITE) 
                ? 0x0000FF00ULL  // ranks 1-2 (bits 8-15)
                : 0xFF000000ULL; // ranks 6-7 (bits 48-55)
            
            return backwardPawns & openPawns & earlyRankMask;
        }

        uint64_t getIsolatedPawns(int color) const {
            const uint64_t pawns = board.getPieceBoards()[color][PIECE_PAWN];
            uint64_t isolated = 0ULL;

            const auto& pawnList = board.getPieceList(color, PIECE_PAWN);
            for (int i = 0; i < pawnList.count(); ++i) {
                const int sq = pawnList[i];
                const int file = BoardRepresentation::FileIndex(sq);

                uint64_t neighborFiles = 0ULL;
                if (file > 0) neighborFiles |= BoardRepresentation::fileMask(file - 1);
                if (file < 7) neighborFiles |= BoardRepresentation::fileMask(file + 1);

                if ((neighborFiles & pawns) == 0ULL) {
                    isolated |= (1ULL << sq);
                }
            }

            return isolated;
        }

        uint64_t getHalfIsolatedPawns(int color) const {
            const uint64_t pawns = board.getPieceBoards()[color][PIECE_PAWN];
            uint64_t halfIsolated = 0ULL;

            const auto& pawnList = board.getPieceList(color, PIECE_PAWN);
            for (int i = 0; i < pawnList.count(); ++i) {
                const int sq = pawnList[i];
                const int file = BoardRepresentation::FileIndex(sq);

                const bool hasWestNeighbor = (file > 0) && ((BoardRepresentation::fileMask(file - 1) & pawns) != 0ULL);
                const bool hasEastNeighbor = (file < 7) && ((BoardRepresentation::fileMask(file + 1) & pawns) != 0ULL);

                if (hasWestNeighbor ^ hasEastNeighbor) {
                    halfIsolated |= (1ULL << sq);
                }
            }

            return halfIsolated;
        }

        uint64_t getDoubledPawns(int color) const {
            const uint64_t pawns = board.getPieceBoards()[color][PIECE_PAWN];
            uint64_t doubled = 0ULL;

            for (int file = 0; file < 8; ++file) {
                const uint64_t onFile = pawns & BoardRepresentation::fileMask(file);
                if (popCount(onFile) >= 2) {
                    doubled |= onFile;
                }
            }

            return doubled;
        }

        static uint64_t getPawnDuos(uint64_t pawns) {
            // Exclusive duo set (pairs, excluding trio/quart core members)
            const uint64_t withWestNeighbors = pawnsWithWestNeighbors(pawns);
            const uint64_t withEastNeighbors = withWestNeighbors >> 1;

            const uint64_t withOneExclusiveNeighbor = withWestNeighbors ^ withEastNeighbors;
            const uint64_t withExclusiveWestNeighbor = withWestNeighbors & withOneExclusiveNeighbor;
            const uint64_t withExclusiveEastNeighbor = withEastNeighbors & withOneExclusiveNeighbor;

            const uint64_t duoWestOne = withExclusiveEastNeighbor & (withExclusiveWestNeighbor >> 1);
            const uint64_t duoEastOne = duoWestOne << 1;
            return duoWestOne | duoEastOne;
        }

        uint64_t getDefendedOpenPawns(int color) const {
            const uint64_t openPawns = getOpenPawns(color);
            const uint64_t ownPawnAttacks = pawnAttackMap(color);
            return openPawns & ownPawnAttacks;
        }

        uint64_t getPawnRams(int color) const {
            const uint64_t wpawns = board.getPieceBoards()[COLOR_WHITE][PIECE_PAWN];
            const uint64_t bpawns = board.getPieceBoards()[COLOR_BLACK][PIECE_PAWN];
            if (color == COLOR_WHITE) return southOne(bpawns) & wpawns;
            return northOne(wpawns) & bpawns;
        }

        static uint64_t northOne(uint64_t bb) {
            return bb << 8;
        }

        static uint64_t southOne(uint64_t bb) {
            return bb >> 8;
        }

        static uint64_t eastOne(uint64_t bb) {
            return (bb << 1) & ~FILE_A;
        }

        static uint64_t westOne(uint64_t bb) {
            return (bb >> 1) & ~FILE_H;
        }

        static uint64_t pawnsWithEastNeighbors(uint64_t pawns) {
            return pawns & westOne(pawns);
        }

        static uint64_t pawnsWithWestNeighbors(uint64_t pawns) {
            return pawns & eastOne(pawns);
        }

        bool isSquareInPawnFrontSpan(int color, int square) const {
            const auto& pawns = board.getPieceList(color, PIECE_PAWN);
            const uint64_t sqMask = (1ULL << square);
            for (int i = 0; i < pawns.count(); ++i) {
                if (getPawnAttackSpan(pawns[i], color) & sqMask) return true;
            }
            return false;
        }

        static bool isLightSquare(int square) {
            const int file = BoardRepresentation::FileIndex(square);
            const int rank = BoardRepresentation::RankIndex(square);
            return BoardRepresentation::LightSquare(file, rank);
        }

        static int countPawnsOnColor(uint64_t pawns, bool lightSquares) {
            int count = 0;
            while (pawns) {
                const int sq = popLSB(pawns);
                if (isLightSquare(sq) == lightSquares) ++count;
            }
            return count;
        }

        static bool isFianchettoBishop(int square, int color, uint64_t ownPawns) {
            const int file = BoardRepresentation::FileIndex(square);
            const int rank = BoardRepresentation::RankIndex(square);

            if (color == COLOR_WHITE) {
                // Bg2 with g3 pawn OR Bb2 with b3 pawn
                if (file == 6 && rank == 1) return (ownPawns & (1ULL << (2 * 8 + 6))) != 0ULL;
                if (file == 1 && rank == 1) return (ownPawns & (1ULL << (2 * 8 + 1))) != 0ULL;
            } else {
                // Bg7 with g6 pawn OR Bb7 with b6 pawn
                if (file == 6 && rank == 6) return (ownPawns & (1ULL << (5 * 8 + 6))) != 0ULL;
                if (file == 1 && rank == 6) return (ownPawns & (1ULL << (5 * 8 + 1))) != 0ULL;
            }

            return false;
        }

        int toTableIndex(int square, int color) const {
            const int file = BoardRepresentation::FileIndex(square);
            const int rank = BoardRepresentation::RankIndex(square);
            const int whiteTableIndex = (7 - rank) * 8 + file;
            const int blackTableIndex = rank * 8 + file;
            return color == COLOR_WHITE ? whiteTableIndex : blackTableIndex;
        }
        
        int getKingSquare(int color) const {
            const auto& kingList = board.getPieceList(color, PIECE_KING);
            if (kingList.count() == 0) return -1;
            return kingList[0];
        }

        int getMaterialWithoutKing(int color) const {
            int material = 0;
            for (int pieceType = PIECE_PAWN; pieceType <= PIECE_QUEEN; ++pieceType) {
                const auto& list = board.getPieceList(color, pieceType);
                material += list.count() * getPieceValue(pieceType);
            }
            return material;
        }

        int getNonPawnMaterial(int color) const {
            int material = 0;
            for (int pieceType = PIECE_KNIGHT; pieceType <= PIECE_QUEEN; ++pieceType) {
                const auto& list = board.getPieceList(color, pieceType);
                material += list.count() * getPieceValue(pieceType);
            }
            return material;
        }

        int getTotalPawnCount() const {
            return popCount(board.getPieceBoards()[COLOR_WHITE][PIECE_PAWN]
                | board.getPieceBoards()[COLOR_BLACK][PIECE_PAWN]);
        }

        int getCentralRamCount() const {
            return popCount((getPawnRams(COLOR_WHITE) | getPawnRams(COLOR_BLACK)) & EXTENDED_CENTER_MASK);
        }

        static int getPhaseIncrement(int pieceType) {
            switch (pieceType) {
            case PIECE_PAWN: return 0;
            case PIECE_KNIGHT: return 1;
            case PIECE_BISHOP: return 1;
            case PIECE_ROOK: return 2;
            case PIECE_QUEEN: return 4;
            case PIECE_KING: return 0;
            default: return 0;
            }
        }

        inline static constexpr uint64_t EXTENDED_CENTER_MASK = (0x3C3C3C3CULL << 8); // c3-f6

        inline static constexpr int KING_SHIELD_MISSING_NEAR_PAWN_PENALTY = 12;
        inline static constexpr int KING_SHIELD_MISSING_FAR_PAWN_PENALTY = 4;
        inline static constexpr int KING_OPEN_FILE_PENALTY = 18;
        inline static constexpr int KING_SEMI_OPEN_FILE_PENALTY = 12;
        inline static constexpr int KING_PAWN_STORM_NEAR_PENALTY = 12;
        inline static constexpr int KING_PAWN_STORM_MID_PENALTY = 8;
        inline static constexpr int KING_PAWN_STORM_FAR_PENALTY = 4;
        inline static constexpr int VIRTUAL_KING_MOBILITY_DIVISOR = 2;

        inline static constexpr int PAWN_BACKWARD_PENALTY = 10;
        inline static constexpr int PAWN_OPEN_PENALTY = 16;
        inline static constexpr int PAWN_STRAGGLER_PENALTY = 15;
        inline static constexpr int PAWN_ISOLATED_PENALTY = 8;
        inline static constexpr int PAWN_HALF_ISOLATED_PENALTY = 4;
        inline static constexpr int PAWN_DOUBLED_PENALTY = 6;
        inline static constexpr int PAWN_DUO_BONUS = 6;
        inline static constexpr int PAWN_DEFENDED_OPEN_BONUS = 4;

        inline static constexpr int MINOR_UNDEFENDED_BY_PAWN_PENALTY = 6;
        inline static constexpr int KNIGHT_OUTPOST_BONUS = 14;
        inline static constexpr int KNIGHT_PAWN_SCALE_BASE = 8;
        inline static constexpr int KNIGHT_CLOSED_CENTER_BONUS_PER_RAM = 2;

        inline static constexpr int BISHOP_PAIR_BONUS = 30;
        inline static constexpr int BAD_BISHOP_PENALTY = 14;
        inline static constexpr int BAD_BISHOP_MOBILITY_THRESHOLD = 4;
        inline static constexpr int BAD_BISHOP_SAME_COLOR_PAWN_THRESHOLD = 4;
        inline static constexpr int BISHOP_BLOCKED_CENTER_PENALTY_PER_RAM = 2;
        inline static constexpr int FIANCHETTO_BISHOP_BONUS = 8;

        inline static constexpr int ROOK_OPEN_FILE_BONUS = 14;
        inline static constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 8;
        inline static constexpr int ROOK_SEVENTH_RANK_BONUS = 12;
        inline static constexpr int ROOK_PAWN_SCALE_BASE = 16;

        inline static constexpr std::array<int, 64> MG_PAWN_TABLE = {
              0,   0,   0,   0,   0,   0,   0,   0,
             98, 134,  61,  95,  68, 126,  34, -11,
             -6,   7,  26,  31,  65,  56,  25, -20,
            -14,  13,   6,  21,  23,  12,  17, -23,
            -27,  -2,  -5,  12,  17,   6,  10, -25,
            -26,  -4,  -4, -10,   3,   3,  33, -12,
            -35,  -1, -20, -23, -15,  24,  38, -22,
              0,   0,   0,   0,   0,   0,   0,   0
        };
        inline static constexpr std::array<int, 64> EG_PAWN_TABLE = {
              0,   0,   0,   0,   0,   0,   0,   0,
            178, 173, 158, 134, 147, 132, 165, 187,
             94, 100,  85,  67,  56,  53,  82,  84,
             32,  24,  13,   5,  -2,   4,  17,  17,
             13,   9,  -3,  -7,  -7,  -8,   3,  -1,
              4,   7,  -6,   1,   0,  -5,  -1,  -8,
             13,   8,   8,  10,  13,   0,   2,  -7,
              0,   0,   0,   0,   0,   0,   0,   0
        };
        inline static constexpr std::array<int, 64> MG_KNIGHT_TABLE = {
            -167, -89, -34, -49,  61, -97, -15, -107,
             -73, -41,  72,  36,  23,  62,   7,  -17,
             -47,  60,  37,  65,  84, 129,  73,   44,
              -9,  17,  19,  53,  37,  69,  18,   22,
             -13,   4,  16,  13,  28,  19,  21,   -8,
             -23,  -9,  12,  10,  19,  17,  25,  -16,
             -29, -53, -12,  -3,  -1,  18, -14,  -19,
            -105, -21, -58, -33, -17, -28, -19,  -23
        };
        inline static constexpr std::array<int, 64> EG_KNIGHT_TABLE = {
            -58, -38, -13, -28, -31, -27, -63, -99,
            -25,  -8, -25,  -2,  -9, -25, -24, -52,
            -24, -20,  10,   9,  -1,  -9, -19, -41,
            -17,   3,  22,  22,  22,  11,   8, -18,
            -18,  -6,  16,  25,  16,  17,   4, -18,
            -23,  -3,  -1,  15,  10,  -3, -20, -22,
            -42, -20, -10,  -5,  -2, -20, -23, -44,
            -29, -51, -23, -15, -22, -18, -50, -64
        };
        inline static constexpr std::array<int, 64> MG_BISHOP_TABLE = {
            -29,   4, -82, -37, -25, -42,   7,  -8,
            -26,  16, -18, -13,  30,  59,  18, -47,
            -16,  37,  43,  40,  35,  50,  37,  -2,
             -4,   5,  19,  50,  37,  37,   7,  -2,
             -6,  13,  13,  26,  34,  12,  10,   4,
              0,  15,  15,  15,  14,  27,  18,  10,
              4,  15,  16,   0,   7,  21,  33,   1,
            -33,  -3, -14, -21, -13, -12, -39, -21
        };
        inline static constexpr std::array<int, 64> EG_BISHOP_TABLE = {
            -14, -21, -11,  -8,  -7,  -9, -17, -24,
             -8,  -4,   7, -12,  -3, -13,  -4, -14,
              2,  -8,   0,  -1,  -2,   6,   0,   4,
             -3,   9,  12,   9,  14,  10,   3,   2,
             -6,   3,  13,  19,   7,  10,  -3,  -9,
            -12,  -3,   8,  10,  13,   3,  -7, -15,
            -14, -18,  -7,  -1,   4,  -9, -15, -27,
            -23,  -9, -23,  -5,  -9, -16,  -5, -17
        };
        inline static constexpr std::array<int, 64> MG_ROOK_TABLE = {
             32,  42,  32,  51,  63,   9,  31,  43,
             27,  32,  58,  62,  80,  67,  26,  44,
             -5,  19,  26,  36,  17,  45,  61,  16,
            -24, -11,   7,  26,  24,  35,  -8, -20,
            -36, -26, -12,  -1,   9,  -7,   6, -23,
            -45, -25, -16, -17,   3,   0,  -5, -33,
            -44, -16, -20,  -9,  -1,  11,  -6, -71,
            -19, -13,   1,  17,  16,   7, -37, -26
        };
        inline static constexpr std::array<int, 64> EG_ROOK_TABLE = {
             13, 10, 18, 15, 12,  12,   8,   5,
             11, 13, 13, 11, -3,   3,   8,   3,
              7,  7,  7,  5,  4,  -3,  -5,  -3,
              4,  3, 13,  1,  2,   1,  -1,   2,
              3,  5,  8,  4, -5,  -6,  -8, -11,
             -4,  0, -5, -1, -7, -12,  -8, -16,
             -6, -6,  0,  2, -9,  -9, -11,  -3,
             -9,  2,  3, -1, -5, -13,   4, -20
        };
        inline static constexpr std::array<int, 64> MG_QUEEN_TABLE = {
            -28,   0,  29,  12,  59,  44,  43,  45,
            -24, -39,  -5,   1, -16,  57,  28,  54,
            -13, -17,   7,   8,  29,  56,  47,  57,
            -27, -27, -16, -16,  -1,  17,  -2,   1,
             -9, -26,  -9, -10,  -2,  -4,   3,  -3,
            -14,   2, -11,  -2,  -5,   2,  14,   5,
            -35,  -8,  11,   2,   8,  15,  -3,   1,
             -1, -18,  -9,  10, -15, -25, -31, -50
        };
        inline static constexpr std::array<int, 64> EG_QUEEN_TABLE = {
             -9,  22,  22,  27,  27,  19,  10,  20,
            -17,  20,  32,  41,  58,  25,  30,   0,
            -20,   6,   9,  49,  47,  35,  19,   9,
              3,  22,  24,  45,  57,  40,  57,  36,
            -18,  28,  19,  47,  31,  34,  39,  23,
            -16, -27,  15,   6,   9,  17,  10,   5,
            -22, -23, -30, -16, -16, -23, -36, -32,
            -33, -28, -22, -43,  -5, -32, -20, -41
        };
        inline static constexpr std::array<int, 64> MG_KING_TABLE = {
            -65,  23,  16, -15, -56, -34,   2,  13,
             29,  -1, -20,  -7,  -8,  -4, -38, -29,
             -9,  24,   2, -16, -20,   6,  22, -22,
            -17, -20, -12, -27, -30, -25, -14, -36,
            -49,  -1, -27, -39, -46, -44, -33, -51,
            -14, -14, -22, -46, -44, -30, -15, -27,
              1,   7,  -8, -64, -43, -16,   9,   8,
            -15,  36,  12, -54,   8, -28,  24,  14
        };
        inline static constexpr std::array<int, 64> EG_KING_TABLE = {
            -74, -35, -18, -18, -11,  15,   4, -17,
            -12,  17,  14,  17,  17,  38,  23,  11,
             10,  17,  23,  15,  20,  45,  44,  13,
             -8,  22,  24,  27,  26,  33,  26,   3,
            -18,  -4,  21,  24,  27,  23,   9, -11,
            -19,  -3,  11,  21,  23,  16,   7,  -9,
            -27, -11,   4,  13,  14,   4,  -5, -17,
            -53, -34, -21, -11, -28, -14, -24, -43
        };
    };
}  // namespace Chess
#endif // EVALUATION_H