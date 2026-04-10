#include "move_generator.h"
#include "profiler.h"
#include "magics.h"
#include "bitboard_util.h"

namespace Chess {

    void MoveGenerator::init() {
        PrecomputedMoveData::initialize();
        initialize_magics();
    }

    void MoveGenerator::generateLegalMoves(BoardState& board, bool genQuiet=true) {
        this->board = &board;
        genQuiets = genQuiet;

        isWhiteToMove = board.isWhiteToMove();
        friendlyColour = isWhiteToMove ? COLOR_WHITE : COLOR_BLACK;
        opponentColour = isWhiteToMove ? COLOR_BLACK : COLOR_WHITE;
        friendlyColourIndex = isWhiteToMove ? 0 : 1;
        opponentColourIndex = 1 - friendlyColourIndex;

        const PieceList& friendlyKingList = board.getPieceList(friendlyColour, PIECE_KING);
        if (friendlyKingList.count() == 0) return;
        friendlyKingSquare = friendlyKingList[0];

        friendlyOccupancy = board.getOccupancy(friendlyColour);
        opponentOccupancy = board.getOccupancy(opponentColour);
        allOccupancy = board.getMainBoard();

        calculateOpponentAttackData();
        
        board.setAttackTable(opponentColour, opponentAttackMap);
        
        clearMoves();
        generateLegalMovesConstrained(board, genQuiet);

        if (debugTrackPseudoLegal) {
            pseudoLegalCount = moveCount;
            for (int i = 0; i < moveCount; ++i) {
                pseudoLegalMoves[i] = moveList[i];
            }
        }
    }

    void MoveGenerator::generateLegalMovesConstrained(const BoardState& board, bool genQuiet) {
        genQuiets = genQuiet;
        generateKingMoves();

        if (inDoubleCheck) return;

        generateSlidingMoves();
        generateKnightMoves();
        generatePawnMoves();
    }

    void MoveGenerator::addMove(const Move& move) {
        if (moveCount < 256) {
            moveList[moveCount++] = move;
        }
    }

    void MoveGenerator::clearMoves() {
        moveCount = 0;
    }

    int MoveGenerator::getLegalMoveCount() const {
        return moveCount;
    }

    std::vector<Move> MoveGenerator::getPieceMoves(int square, const BoardState* board) {
        std::vector<Move> pieceMoves;
        if (!board) return pieceMoves;

        // initialize generator state for this board
        this->board = board;
        isWhiteToMove = board->isWhiteToMove();
        friendlyColour = isWhiteToMove ? COLOR_WHITE : COLOR_BLACK;
        opponentColour = isWhiteToMove ? COLOR_BLACK : COLOR_WHITE;
        friendlyColourIndex = isWhiteToMove ? 0 : 1;
        opponentColourIndex = 1 - friendlyColourIndex;

        const int pieceType = board->getPieceTypeAt(square);
        if (pieceType < 0) return pieceMoves;

        const int pieceColor = board->getColorAt(square);
        // only generate moves for side to move
        if (pieceColor != (board->isWhiteToMove() ? COLOR_WHITE : COLOR_BLACK)) return pieceMoves;

        const PieceList& friendlyKingList = board->getPieceList(friendlyColour, PIECE_KING);
        if (friendlyKingList.count() == 0) return pieceMoves;
        friendlyKingSquare = friendlyKingList[0];

        friendlyOccupancy = board->getOccupancy(friendlyColour);
        opponentOccupancy = board->getOccupancy(opponentColour);
        allOccupancy = board->getMainBoard();

        // calculate attack/check/pin data
        calculateOpponentAttackData();

        const bool isPinned = isPinnedFunc(square);
        const uint64_t pinLine = isPinned ? pinLineBitmaskBySquare[square] : 0ULL;

        switch (pieceType) {
        case PIECE_KING: {
            const int kingSquare = square;
            const auto& kingMoveList = PrecomputedMoveData::getKingMovesVector(kingSquare);
            for (uint8_t toSquare : kingMoveList) {
                uint64_t toMask = (1ULL << toSquare);
                if ((toSquare == kingSquare + 2) || (toSquare == kingSquare - 2)) {
                    continue;
                }
                if (friendlyOccupancy & toMask) continue;
                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (!isCapture && !true) continue; // genQuiets true for UI
                if (!squareIsAttacked(toSquare)) {
                    pieceMoves.emplace_back(kingSquare, toSquare);
                }
            }

            if (!inCheck) {
                // castling
                if (hasKingsideCastleRight()) {
                    const int f_square = kingSquare + 1;
                    const int g_square = kingSquare + 2;
                    const uint64_t f_mask = (1ULL << f_square);
                    const uint64_t g_mask = (1ULL << g_square);
                    if (!(allOccupancy & f_mask) && !(allOccupancy & g_mask)) {
                        if (!squareIsAttacked(f_square) && !squareIsAttacked(g_square)) {
                            pieceMoves.emplace_back(kingSquare, g_square, Move::Flag::Castling);
                        }
                    }
                }
                if (hasQueensideCastleRight()) {
                    const int b_square = kingSquare - 3;
                    const int c_square = kingSquare - 2;
                    const int d_square = kingSquare - 1;
                    const uint64_t b_mask = (1ULL << b_square);
                    const uint64_t c_mask = (1ULL << c_square);
                    const uint64_t d_mask = (1ULL << d_square);
                    if (!(allOccupancy & b_mask) && !(allOccupancy & c_mask) && !(allOccupancy & d_mask)) {
                        if (!squareIsAttacked(d_square) && !squareIsAttacked(c_square)) {
                            pieceMoves.emplace_back(kingSquare, c_square, Move::Flag::Castling);
                        }
                    }
                }
            }
            break;
        }
        case PIECE_ROOK:
        case PIECE_BISHOP:
        case PIECE_QUEEN: {
            uint64_t attacks = 0ULL;
            if (pieceType == PIECE_ROOK) {
                const MagicEntry& m = rookMagics[square];
                const uint64_t blockers = allOccupancy | ~m.mask;
                const uint64_t index = (blockers * m.magic) >> m.shift;
                attacks = m.ptr[index] & ~friendlyOccupancy;
            } else if (pieceType == PIECE_BISHOP) {
                const MagicEntry& m = bishopMagics[square];
                const uint64_t blockers = allOccupancy | ~m.mask;
                const uint64_t index = (blockers * m.magic) >> m.shift;
                attacks = m.ptr[index] & ~friendlyOccupancy;
            } else {
                const MagicEntry& rookMagic = rookMagics[square];
                const MagicEntry& bishopMagic = bishopMagics[square];
                const uint64_t rookBlockers = allOccupancy | ~rookMagic.mask;
                const uint64_t bishopBlockers = allOccupancy | ~bishopMagic.mask;
                const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
                const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
                attacks = (rookMagic.ptr[rookIndex] | bishopMagic.ptr[bishopIndex]) & ~friendlyOccupancy;
            }

            while (attacks) {
                const int toSquare = popLSB(attacks);
                if (isPinned && (pinLine & (1ULL << toSquare)) == 0ULL) continue;
                if (inCheck && !squareIsInCheckRay(toSquare)) continue;
                pieceMoves.emplace_back(square, toSquare);
            }
            break;
        }
        case PIECE_KNIGHT: {
            if (isPinned) break; // knight pinned can't move
            const auto& knightMoveList = PrecomputedMoveData::getKnightMoves(square);
            for (uint8_t toSquare : knightMoveList) {
                uint64_t toMask = (1ULL << toSquare);
                if (friendlyOccupancy & toMask) continue;
                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (inCheck && !squareIsInCheckRay(toSquare)) continue;
                pieceMoves.emplace_back(square, toSquare);
            }
            break;
        }
        case PIECE_PAWN: {
            const int pawnSquare = square;
            const int pushDir = (friendlyColour == COLOR_WHITE) ? 8 : -8;
            const int pawnRank = BoardRepresentation::RankIndex(pawnSquare);
            const int captureRank = (friendlyColour == COLOR_WHITE) ? 6 : 1;
            const int startRank = (friendlyColour == COLOR_WHITE) ? 1 : 6;
            const bool oneStepFromPromotion = (pawnRank == captureRank);

            // forward moves
            const int pushSquare = pawnSquare + pushDir;
            if (pushSquare >= 0 && pushSquare < 64) {
                uint64_t pushMask = (1ULL << pushSquare);
                if (!(allOccupancy & pushMask)) {
                    if (!isPinned || (pinLine & pushMask)) {
                        if (!inCheck || squareIsInCheckRay(pushSquare)) {
                            if (oneStepFromPromotion) {
                                pieceMoves.emplace_back(pawnSquare, pushSquare, Move::Flag::PromoteToQueen);
                                pieceMoves.emplace_back(pawnSquare, pushSquare, Move::Flag::PromoteToRook);
                                pieceMoves.emplace_back(pawnSquare, pushSquare, Move::Flag::PromoteToKnight);
                                pieceMoves.emplace_back(pawnSquare, pushSquare, Move::Flag::PromoteToBishop);
                            } else {
                                pieceMoves.emplace_back(pawnSquare, pushSquare);
                            }
                        }

                        if (pawnRank == startRank) {
                            const int doubleSquare = pawnSquare + 2 * pushDir;
                            if (doubleSquare >= 0 && doubleSquare < 64) {
                                uint64_t doubleMask = (1ULL << doubleSquare);
                                if (!(allOccupancy & doubleMask)) {
                                    if (!inCheck || squareIsInCheckRay(doubleSquare)) {
                                        pieceMoves.emplace_back(pawnSquare, doubleSquare, Move::Flag::PawnTwoForward);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // captures
            const auto& pawnAttacks = (friendlyColour == COLOR_WHITE) ?
                PrecomputedMoveData::getPawnAttacksWhite(pawnSquare) :
                PrecomputedMoveData::getPawnAttacksBlack(pawnSquare);

            for (int captureSquare : pawnAttacks) {
                if (isPinned && (pinLine & (1ULL << captureSquare)) == 0ULL) continue;
                uint64_t captureMask = (1ULL << captureSquare);
                if (opponentOccupancy & captureMask) {
                    if (inCheck && !squareIsInCheckRay(captureSquare)) continue;
                    if (oneStepFromPromotion) {
                        pieceMoves.emplace_back(pawnSquare, captureSquare, Move::Flag::PromoteToQueen);
                        pieceMoves.emplace_back(pawnSquare, captureSquare, Move::Flag::PromoteToRook);
                        pieceMoves.emplace_back(pawnSquare, captureSquare, Move::Flag::PromoteToKnight);
                        pieceMoves.emplace_back(pawnSquare, captureSquare, Move::Flag::PromoteToBishop);
                    } else {
                        pieceMoves.emplace_back(pawnSquare, captureSquare);
                    }
                }

                // en-passant
                if (board->getEnPas() == captureSquare) {
                    const int epCapturedPawnSquare = captureSquare + ((friendlyColour == COLOR_WHITE) ? -8 : 8);
                    if (!inCheckAfterEnPassant(pawnSquare, captureSquare, epCapturedPawnSquare)) {
                        pieceMoves.emplace_back(pawnSquare, captureSquare, Move::Flag::EnPassantCapture);
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        return pieceMoves;
    }


    void MoveGenerator::calculateOpponentAttackData() {
        opponentKnightAttacks = 0ULL;
        opponentPawnAttackMap = 0ULL;
        opponentAttackMapNoPawns = 0ULL;
        opponentSlidingAttackMap = 0ULL;
        opponentAttackMap = 0ULL;
        inCheck = false;
        inDoubleCheck = false;
        pinsExistInPosition = false;
        checkRayBitmask = 0ULL;
        pinRayBitmask = 0ULL;
        pinLineBitmaskBySquare.fill(0ULL);

        const auto& pieceBoards = board->getPieceBoards();
        const uint64_t opponentPawnsBb = pieceBoards[opponentColour][PIECE_PAWN];

        const PieceList& opponentKnights = board->getPieceList(opponentColour, PIECE_KNIGHT);

        for (int i = 0; i < opponentKnights.count(); ++i) {
            const int knightSq = opponentKnights[i];
            const uint64_t attacks = PrecomputedMoveData::getKnightAttacks(knightSq);
            opponentKnightAttacks |= attacks;
            if (attacks & (1ULL << friendlyKingSquare)) {
                inDoubleCheck = inCheck;
                inCheck = true;
                checkRayBitmask |= (1ULL << knightSq);
            }
        }

        if (opponentColour == COLOR_WHITE) {
            opponentPawnAttackMap = ((opponentPawnsBb & ~FILE_A) << 7) | ((opponentPawnsBb & ~FILE_H) << 9);
        } else {
            opponentPawnAttackMap = ((opponentPawnsBb & ~FILE_H) >> 7) | ((opponentPawnsBb & ~FILE_A) >> 9);
        }

        const uint64_t checkingPawns =
            PrecomputedMoveData::getPawnAttackBitboard(opponentColour ^ 1, friendlyKingSquare) & opponentPawnsBb;
        if (checkingPawns) {
            inDoubleCheck = inCheck;
            inCheck = true;
            checkRayBitmask |= checkingPawns;
        }

        genSlidingAttackMap();

        opponentAttackMapNoPawns = opponentKnightAttacks | opponentSlidingAttackMap;
        opponentAttackMap = opponentAttackMapNoPawns | opponentPawnAttackMap;

        const PieceList& opponentKingList = board->getPieceList(opponentColour, PIECE_KING);
        if (opponentKingList.count() > 0) {
            opponentAttackMap |= PrecomputedMoveData::getKingMoves(opponentKingList[0]);
            opponentAttackMapNoPawns |= PrecomputedMoveData::getKingMoves(opponentKingList[0]);
        }
    }

    void MoveGenerator::genSlidingAttackMap() {
        opponentSlidingAttackMap = 0ULL;

        const PieceList& opponentRooks = board->getPieceList(opponentColour, PIECE_ROOK);
        const PieceList& opponentBishops = board->getPieceList(opponentColour, PIECE_BISHOP);
        const PieceList& opponentQueens = board->getPieceList(opponentColour, PIECE_QUEEN);

        auto processCheckPin = [&](int attackerSquare, bool diagonalOnly) {
            const int dir = PrecomputedMoveData::getDirectionOffset(friendlyKingSquare, attackerSquare);
            if (dir == 0) return;

            const bool isDiagonalDir = (dir == 7 || dir == -7 || dir == 9 || dir == -9);
            if (diagonalOnly != isDiagonalDir) return;

            const uint64_t between = PrecomputedMoveData::getBetweenBitboard(friendlyKingSquare, attackerSquare);
            const uint64_t blockers = between & allOccupancy;
            if (blockers == 0ULL) {
                inDoubleCheck = inCheck;
                inCheck = true;
                checkRayBitmask |= between | (1ULL << attackerSquare);
                return;
            }

            if ((blockers & ~friendlyOccupancy) == 0ULL && popCount(blockers) == 1) {
                pinsExistInPosition = true;
                const int pinnedSq = getLSB(blockers);
                pinRayBitmask |= (1ULL << pinnedSq);
                pinLineBitmaskBySquare[pinnedSq] = PrecomputedMoveData::getLineBitboard(friendlyKingSquare, attackerSquare);
            }
        };

        for (int i = 0; i < opponentRooks.count(); ++i) {
            updateSlidingAttackPiece(opponentRooks[i], 0, 4);
            processCheckPin(opponentRooks[i], false);
        }
        for (int i = 0; i < opponentQueens.count(); ++i) {
            updateSlidingAttackPiece(opponentQueens[i], 0, 8);
            const int qsq = opponentQueens[i];
            const int dir = PrecomputedMoveData::getDirectionOffset(friendlyKingSquare, qsq);
            if (dir != 0) {
                const bool diagonal = (dir == 7 || dir == -7 || dir == 9 || dir == -9);
                processCheckPin(qsq, diagonal);
            }
        }
        for (int i = 0; i < opponentBishops.count(); ++i) {
            updateSlidingAttackPiece(opponentBishops[i], 4, 8);
            processCheckPin(opponentBishops[i], true);
        }
    }

    void MoveGenerator::updateSlidingAttackPiece(int startSquare, int startDirIndex, int endDirIndex) {
        uint64_t attacks = 0ULL;
        const uint64_t occupancyNoFriendlyKing = board->getMainBoard() & ~(1ULL << friendlyKingSquare);

        if (startDirIndex == 0 && endDirIndex == 4) {
            const MagicEntry& rookMagic = rookMagics[startSquare];
            const uint64_t blockers = occupancyNoFriendlyKing | ~rookMagic.mask;
            const uint64_t index = (blockers * rookMagic.magic) >> rookMagic.shift;
            attacks = rookMagic.ptr[index];
        } else if (startDirIndex == 4 && endDirIndex == 8) {
            const MagicEntry& bishopMagic = bishopMagics[startSquare];
            const uint64_t blockers = occupancyNoFriendlyKing | ~bishopMagic.mask;
            const uint64_t index = (blockers * bishopMagic.magic) >> bishopMagic.shift;
            attacks = bishopMagic.ptr[index];
        } else {
            const MagicEntry& rookMagic = rookMagics[startSquare];
            const MagicEntry& bishopMagic = bishopMagics[startSquare];
            const uint64_t rookBlockers = occupancyNoFriendlyKing | ~rookMagic.mask;
            const uint64_t bishopBlockers = occupancyNoFriendlyKing | ~bishopMagic.mask;
            const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
            const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
            attacks = rookMagic.ptr[rookIndex] | bishopMagic.ptr[bishopIndex];
        }

        opponentSlidingAttackMap |= attacks;
    }

    void MoveGenerator::generateKingMoves() {
        const auto& kingSquares = board->getPieceList(friendlyColour, PIECE_KING);
        if (kingSquares.count() == 0) return;

        const int kingSquare = kingSquares[0];
        const auto& kingMoveList = PrecomputedMoveData::getKingMovesVector(kingSquare);

        for (uint8_t toSquare : kingMoveList) {
            uint64_t toMask = (1ULL << toSquare);

            if ((toSquare == kingSquare + 2) || (toSquare == kingSquare - 2)) continue;

            if (friendlyOccupancy & toMask) continue;

            const bool isCapture = (opponentOccupancy & toMask) != 0;
            if (!isCapture && !genQuiets) continue;

            if (!squareIsAttacked(toSquare)) {
                if (isCapture) addCaptureMove(*board, kingSquare, toSquare);
                else addQuietMove(*board, kingSquare, toSquare);
            }
        }

        if (genQuiets && !inCheck) {
            if (hasKingsideCastleRight()) {
                const int f_square = kingSquare + 1;
                const int g_square = kingSquare + 2;
                const uint64_t f_mask = (1ULL << f_square);
                const uint64_t g_mask = (1ULL << g_square);
                if (!(allOccupancy & f_mask) && !(allOccupancy & g_mask)) {
                    if (!squareIsAttacked(f_square) && !squareIsAttacked(g_square)) {
                        addMove(Move(kingSquare, g_square, Move::Flag::Castling));
                    }
                }
            }
            if (hasQueensideCastleRight()) {
                const int b_square = kingSquare - 3;
                const int c_square = kingSquare - 2;
                const int d_square = kingSquare - 1;
                const uint64_t b_mask = (1ULL << b_square);
                const uint64_t c_mask = (1ULL << c_square);
                const uint64_t d_mask = (1ULL << d_square);
                if (!(allOccupancy & b_mask) && !(allOccupancy & c_mask) && !(allOccupancy & d_mask)) {
                    if (!squareIsAttacked(d_square) && !squareIsAttacked(c_square)) {
                        addMove(Move(kingSquare, c_square, Move::Flag::Castling));
                    }
                }
            }
        }
    }

    void MoveGenerator::generateSlidingMoves() {
        generateRookMoves();
        generateBishopMoves();
        generateQueenMoves();
    }

    void MoveGenerator::generateRookMoves() {
        const auto& rookSquares = board->getPieceList(friendlyColour, PIECE_ROOK);
        const bool unconstrained = !inCheck && !pinsExistInPosition;

        if (unconstrained) {
            for (int i = 0; i < rookSquares.count(); ++i) {
                const int fromSquare = rookSquares[i];
                const MagicEntry& m = rookMagics[fromSquare];
                const uint64_t blockers = allOccupancy | ~m.mask;
                const uint64_t index = (blockers * m.magic) >> m.shift;
                uint64_t attacks = m.ptr[index] & ~friendlyOccupancy;

                while (attacks) {
                    const int toSquare = popLSB(attacks);
                    const uint64_t toMask = (1ULL << toSquare);
                    if (opponentOccupancy & toMask) addCaptureMove(*board, fromSquare, toSquare);
                    else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
                }
            }
            return;
        }

        for (int i = 0; i < rookSquares.count(); ++i) {
            const int fromSquare = rookSquares[i];
            const bool isPinned = isPinnedFunc(fromSquare);
            const uint64_t pinLine = isPinned ? pinLineBitmaskBySquare[fromSquare] : 0ULL;
            if (inCheck && isPinned) continue;

            const MagicEntry& m = rookMagics[fromSquare];
            const uint64_t blockers = allOccupancy | ~m.mask;
            const uint64_t index = (blockers * m.magic) >> m.shift;
            uint64_t attacks = m.ptr[index] & ~friendlyOccupancy;

            while (attacks) {
                const int toSquare = popLSB(attacks);
                if (isPinned && (pinLine & (1ULL << toSquare)) == 0ULL) continue;
                if (inCheck && !squareIsInCheckRay(toSquare)) continue;

                const uint64_t toMask = (1ULL << toSquare);
                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (isCapture) addCaptureMove(*board, fromSquare, toSquare);
                else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
            }
        }
    }

    void MoveGenerator::generateBishopMoves() {
        const auto& bishopSquares = board->getPieceList(friendlyColour, PIECE_BISHOP);
        const bool unconstrained = !inCheck && !pinsExistInPosition;

        if (unconstrained) {
            for (int i = 0; i < bishopSquares.count(); ++i) {
                const int fromSquare = bishopSquares[i];
                const MagicEntry& m = bishopMagics[fromSquare];
                const uint64_t blockers = allOccupancy | ~m.mask;
                const uint64_t index = (blockers * m.magic) >> m.shift;
                uint64_t attacks = m.ptr[index] & ~friendlyOccupancy;

                while (attacks) {
                    const int toSquare = popLSB(attacks);
                    const uint64_t toMask = (1ULL << toSquare);
                    if (opponentOccupancy & toMask) addCaptureMove(*board, fromSquare, toSquare);
                    else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
                }
            }
            return;
        }

        for (int i = 0; i < bishopSquares.count(); ++i) {
            const int fromSquare = bishopSquares[i];
            const bool isPinned = isPinnedFunc(fromSquare);
            const uint64_t pinLine = isPinned ? pinLineBitmaskBySquare[fromSquare] : 0ULL;
            if (inCheck && isPinned) continue;

            const MagicEntry& m = bishopMagics[fromSquare];
            const uint64_t blockers = allOccupancy | ~m.mask;
            const uint64_t index = (blockers * m.magic) >> m.shift;
            uint64_t attacks = m.ptr[index] & ~friendlyOccupancy;

            while (attacks) {
                const int toSquare = popLSB(attacks);
                if (isPinned && (pinLine & (1ULL << toSquare)) == 0ULL) continue;
                if (inCheck && !squareIsInCheckRay(toSquare)) continue;

                const uint64_t toMask = (1ULL << toSquare);
                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (isCapture) addCaptureMove(*board, fromSquare, toSquare);
                else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
            }
        }
    }

    void MoveGenerator::generateQueenMoves() {
        const auto& queenSquares = board->getPieceList(friendlyColour, PIECE_QUEEN);
        const bool unconstrained = !inCheck && !pinsExistInPosition;

        if (unconstrained) {
            for (int i = 0; i < queenSquares.count(); ++i) {
                const int fromSquare = queenSquares[i];
                const MagicEntry& rookMagic = rookMagics[fromSquare];
                const MagicEntry& bishopMagic = bishopMagics[fromSquare];
                const uint64_t rookBlockers = allOccupancy | ~rookMagic.mask;
                const uint64_t bishopBlockers = allOccupancy | ~bishopMagic.mask;
                const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
                const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
                uint64_t attacks = (rookMagic.ptr[rookIndex] | bishopMagic.ptr[bishopIndex]) & ~friendlyOccupancy;

                while (attacks) {
                    const int toSquare = popLSB(attacks);
                    const uint64_t toMask = (1ULL << toSquare);
                    if (opponentOccupancy & toMask) addCaptureMove(*board, fromSquare, toSquare);
                    else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
                }
            }
            return;
        }

        for (int i = 0; i < queenSquares.count(); ++i) {
            const int fromSquare = queenSquares[i];
            const bool isPinned = isPinnedFunc(fromSquare);
            const uint64_t pinLine = isPinned ? pinLineBitmaskBySquare[fromSquare] : 0ULL;
            if (inCheck && isPinned) continue;

            const MagicEntry& rookMagic = rookMagics[fromSquare];
            const MagicEntry& bishopMagic = bishopMagics[fromSquare];
            const uint64_t rookBlockers = allOccupancy | ~rookMagic.mask;
            const uint64_t bishopBlockers = allOccupancy | ~bishopMagic.mask;
            const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
            const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
            uint64_t attacks = (rookMagic.ptr[rookIndex] | bishopMagic.ptr[bishopIndex]) & ~friendlyOccupancy;

            while (attacks) {
                const int toSquare = popLSB(attacks);
                if (isPinned && (pinLine & (1ULL << toSquare)) == 0ULL) continue;
                if (inCheck && !squareIsInCheckRay(toSquare)) continue;

                const uint64_t toMask = (1ULL << toSquare);
                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (isCapture) addCaptureMove(*board, fromSquare, toSquare);
                else if (genQuiets) addQuietMove(*board, fromSquare, toSquare);
            }
        }
    }

    void MoveGenerator::generateKnightMoves() {
        const auto& knightSquares = board->getPieceList(friendlyColour, PIECE_KNIGHT);
        const bool unconstrained = !inCheck && !pinsExistInPosition;

        if (unconstrained) {
            for (int i = 0; i < knightSquares.count(); ++i) {
                const int knightSquare = knightSquares[i];
                const auto& knightMoveList = PrecomputedMoveData::getKnightMoves(knightSquare);
                for (uint8_t toSquare : knightMoveList) {
                    const uint64_t toMask = (1ULL << toSquare);
                    if (friendlyOccupancy & toMask) continue;
                    const bool isCapture = (opponentOccupancy & toMask) != 0;
                    if (!genQuiets && !isCapture) continue;
                    if (isCapture) addCaptureMove(*board, knightSquare, toSquare);
                    else addQuietMove(*board, knightSquare, toSquare);
                }
            }
            return;
        }

        for (int i = 0; i < knightSquares.count(); ++i) {
            const int knightSquare = knightSquares[i];
            if (isPinnedFunc(knightSquare)) continue;

            const auto& knightMoveList = PrecomputedMoveData::getKnightMoves(knightSquare);
            for (uint8_t toSquare : knightMoveList) {
                uint64_t toMask = (1ULL << toSquare);

                if (friendlyOccupancy & toMask) continue;

                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (!genQuiets && !isCapture) continue;

                if (inCheck && !squareIsInCheckRay(toSquare)) continue;

                if (isCapture) addCaptureMove(*board, knightSquare, toSquare);
                else addQuietMove(*board, knightSquare, toSquare);
            }
        }
    }

    void MoveGenerator::generatePawnMoves() {
        const auto& pawnSquares = board->getPieceList(friendlyColour, PIECE_PAWN);
        const int pushDir = (friendlyColour == COLOR_WHITE) ? 8 : -8;
        const int captureRank = (friendlyColour == COLOR_WHITE) ? 6 : 1;
        const int startRank = (friendlyColour == COLOR_WHITE) ? 1 : 6;
        const bool unconstrained = !inCheck && !pinsExistInPosition;

        if (unconstrained) {
            for (int i = 0; i < pawnSquares.count(); ++i) {
                const int pawnSquare = pawnSquares[i];
                const int pawnRank = BoardRepresentation::RankIndex(pawnSquare);
                const bool oneStepFromPromotion = (pawnRank == captureRank);

                if (genQuiets) {
                    const int pushSquare = pawnSquare + pushDir;
                    if (pushSquare >= 0 && pushSquare < 64) {
                        const uint64_t pushMask = (1ULL << pushSquare);
                        if (!(allOccupancy & pushMask)) {
                            if (oneStepFromPromotion) {
                                makePromotionMoves(pawnSquare, pushSquare);
                            } else {
                                addPawnMove(*board, pawnSquare, pushSquare);
                            }

                            if (pawnRank == startRank) {
                                const int doubleSquare = pawnSquare + 2 * pushDir;
                                const uint64_t doubleMask = (1ULL << doubleSquare);
                                if (!(allOccupancy & doubleMask)) {
                                    addMove(Move(pawnSquare, doubleSquare, Move::Flag::PawnTwoForward));
                                }
                            }
                        }
                    }
                }

                const auto& pawnAttacks = (friendlyColour == COLOR_WHITE) ?
                    PrecomputedMoveData::getPawnAttacksWhite(pawnSquare) :
                    PrecomputedMoveData::getPawnAttacksBlack(pawnSquare);

                for (int captureSquare : pawnAttacks) {
                    const uint64_t captureMask = (1ULL << captureSquare);

                    if (opponentOccupancy & captureMask) {
                        if (oneStepFromPromotion) {
                            makePromotionMoves(pawnSquare, captureSquare);
                        } else {
                            addPawnCaptureMove(*board, pawnSquare, captureSquare, board->getPieceTypeAt(captureSquare));
                        }
                    }

                    if (board->getEnPas() == captureSquare) {
                        const int epCapturedPawnSquare = captureSquare + ((friendlyColour == COLOR_WHITE) ? -8 : 8);
                        if (!inCheckAfterEnPassant(pawnSquare, captureSquare, epCapturedPawnSquare)) {
                            addEnPassantMove(*board, pawnSquare, captureSquare);
                        }
                    }
                }
            }
            return;
        }

        for (int i = 0; i < pawnSquares.count(); ++i) {
            const int pawnSquare = pawnSquares[i];
            const int pawnRank = BoardRepresentation::RankIndex(pawnSquare);
            const bool oneStepFromPromotion = (pawnRank == captureRank);
            const bool isPinned = isPinnedFunc(pawnSquare);
            const uint64_t pinLine = isPinned ? pinLineBitmaskBySquare[pawnSquare] : 0ULL;

            if (genQuiets) {
                const int pushSquare = pawnSquare + pushDir;
                if (pushSquare >= 0 && pushSquare < 64) {
                    const uint64_t pushMask = (1ULL << pushSquare);
                    if (!(allOccupancy & pushMask)) {
                        if (!isPinned || (pinLine & pushMask)) {
                            if (!inCheck || squareIsInCheckRay(pushSquare)) {
                                if (oneStepFromPromotion) {
                                    makePromotionMoves(pawnSquare, pushSquare);
                                } else {
                                    addPawnMove(*board, pawnSquare, pushSquare);
                                }
                            }

                            if (pawnRank == startRank) {
                                const int doubleSquare = pawnSquare + 2 * pushDir;
                                const uint64_t doubleMask = (1ULL << doubleSquare);
                                if (!(allOccupancy & doubleMask)) {
                                    if (!inCheck || squareIsInCheckRay(doubleSquare)) {
                                        addMove(Move(pawnSquare, doubleSquare, Move::Flag::PawnTwoForward));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            const auto& pawnAttacks = (friendlyColour == COLOR_WHITE) ?
                PrecomputedMoveData::getPawnAttacksWhite(pawnSquare) :
                PrecomputedMoveData::getPawnAttacksBlack(pawnSquare);

            for (int captureSquare : pawnAttacks) {
                if (isPinned && (pinLine & (1ULL << captureSquare)) == 0ULL) {
                    continue;
                }

                uint64_t captureMask = (1ULL << captureSquare);

                if (opponentOccupancy & captureMask) {
                    if (inCheck && !squareIsInCheckRay(captureSquare)) {
                        continue;
                    }

                    if (oneStepFromPromotion) makePromotionMoves(pawnSquare, captureSquare);
                    else addPawnCaptureMove(*board, pawnSquare, captureSquare, board->getPieceTypeAt(captureSquare));
                }

                if (board->getEnPas() == captureSquare) {
                    const int epCapturedPawnSquare = captureSquare + ((friendlyColour == COLOR_WHITE) ? -8 : 8);
                    if (!inCheckAfterEnPassant(pawnSquare, captureSquare, epCapturedPawnSquare)) {
                        addEnPassantMove(*board, pawnSquare, captureSquare);
                    }
                }
            }
        }
    }

    void MoveGenerator::makePromotionMoves(int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToQueen));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToRook));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToKnight));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToBishop));
    }

    bool MoveGenerator::isPinnedFunc(int square) {
        if (!pinsExistInPosition) return false;
        return (pinRayBitmask & (1ULL << square)) != 0;
    }

    bool MoveGenerator::squareIsInCheckRay(int square) {
        if (!inCheck) return false;
        return (checkRayBitmask & (1ULL << square)) != 0;
    }

    bool MoveGenerator::hasKingsideCastleRight() {
        if (friendlyColour == COLOR_WHITE) {
            return (board->getCastleRights() & 0x01) != 0;
        } else {
            return (board->getCastleRights() & 0x04) != 0;
        }
    }

    bool MoveGenerator::hasQueensideCastleRight() {
        if (friendlyColour == COLOR_WHITE) {
            return (board->getCastleRights() & 0x02) != 0;
        } else {
            return (board->getCastleRights() & 0x08) != 0;
        }
    }

    bool MoveGenerator::squareIsAttacked(int square) {
        if (square < 0 || square >= 64) return false;

        if (opponentPawnAttackMap & (1ULL << square)) return true;
        if (opponentKnightAttacks & (1ULL << square)) return true;

        const PieceList& opponentKingList = board->getPieceList(opponentColour, PIECE_KING);
        if (opponentKingList.count() > 0) {
            const int opponentKingSquare = opponentKingList[0];
            const uint64_t opponentKingAttacks = PrecomputedMoveData::getKingMoves(opponentKingSquare);
            if (opponentKingAttacks & (1ULL << square)) return true;
        }

        if (opponentAttackMapNoPawns & (1ULL << square)) return true;

        return false;
    }

    bool MoveGenerator::inCheckAfterEnPassant(int startSquare, int targetSquare, int epCapturedPawnSquare) {
        const int capturingColor = board->getSide();
        const int opponentColor = capturingColor ^ 1;
        const int kingSq = friendlyKingSquare;

        uint64_t occ = allOccupancy;
        occ &= ~(1ULL << startSquare);
        occ &= ~(1ULL << epCapturedPawnSquare);
        occ |= (1ULL << targetSquare);

        const auto& pieces = board->getPieceBoards();
        uint64_t opponentPawns = pieces[opponentColor][PIECE_PAWN] & ~(1ULL << epCapturedPawnSquare);
        const uint64_t opponentKnights = pieces[opponentColor][PIECE_KNIGHT];
        const uint64_t opponentBishops = pieces[opponentColor][PIECE_BISHOP];
        const uint64_t opponentRooks = pieces[opponentColor][PIECE_ROOK];
        const uint64_t opponentQueens = pieces[opponentColor][PIECE_QUEEN];
        const uint64_t opponentKing = pieces[opponentColor][PIECE_KING];

        const uint64_t pawnAttackers = PrecomputedMoveData::getPawnAttackBitboard(opponentColor ^ 1, kingSq);
        if (pawnAttackers & opponentPawns) return true;
        if (PrecomputedMoveData::getKnightAttacks(kingSq) & opponentKnights) return true;
        if (PrecomputedMoveData::getKingMoves(kingSq) & opponentKing) return true;

        const MagicEntry& rookMagic = rookMagics[kingSq];
        const uint64_t rookBlockers = occ | ~rookMagic.mask;
        const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
        if (rookMagic.ptr[rookIndex] & (opponentRooks | opponentQueens)) return true;

        const MagicEntry& bishopMagic = bishopMagics[kingSq];
        const uint64_t bishopBlockers = occ | ~bishopMagic.mask;
        const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
        if (bishopMagic.ptr[bishopIndex] & (opponentBishops | opponentQueens)) return true;

        return false;
    }

    bool MoveGenerator::isSquareAttackedByColor(int square, int opponentColor, const BoardState& board) {
        const auto& pieceBoards = board.getPieceBoards();
        uint64_t opponentPawns = pieceBoards[opponentColor][PIECE_PAWN];
        uint64_t opponentKnights = pieceBoards[opponentColor][PIECE_KNIGHT];
        uint64_t opponentBishops = pieceBoards[opponentColor][PIECE_BISHOP];
        uint64_t opponentRooks = pieceBoards[opponentColor][PIECE_ROOK];
        uint64_t opponentQueens = pieceBoards[opponentColor][PIECE_QUEEN];
        uint64_t opponentKing = pieceBoards[opponentColor][PIECE_KING];

        uint64_t pawnAttackBitboard = PrecomputedMoveData::getPawnAttackBitboard(opponentColor ^ 1, square);
        if (pawnAttackBitboard & opponentPawns) return true;

        uint64_t knightAttackBitboard = PrecomputedMoveData::getKnightAttacks(square);
        if (knightAttackBitboard & opponentKnights) return true;

        uint64_t kingAttackBitboard = PrecomputedMoveData::getKingMoves(square);
        if (kingAttackBitboard & opponentKing) return true;

        const uint64_t occupancy = board.getMainBoard();

        const MagicEntry& rookMagic = rookMagics[square];
        const uint64_t rookBlockers = occupancy | ~rookMagic.mask;
        const uint64_t rookIndex = (rookBlockers * rookMagic.magic) >> rookMagic.shift;
        const uint64_t rookAttackSet = rookMagic.ptr[rookIndex];
        if (rookAttackSet & (opponentRooks | opponentQueens)) {
            return true;
        }

        const MagicEntry& bishopMagic = bishopMagics[square];
        const uint64_t bishopBlockers = occupancy | ~bishopMagic.mask;
        const uint64_t bishopIndex = (bishopBlockers * bishopMagic.magic) >> bishopMagic.shift;
        const uint64_t bishopAttackSet = bishopMagic.ptr[bishopIndex];
        if (bishopAttackSet & (opponentBishops | opponentQueens)) {
            return true;
        }

        return false;
    }

    void MoveGenerator::addQuietMove(const BoardState& board, int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::None));
    }

    void MoveGenerator::addCaptureMove(const BoardState& board, int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::None));
    }

    void MoveGenerator::addEnPassantMove(const BoardState& board, int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::EnPassantCapture));
    }

    void MoveGenerator::addPawnMove(const BoardState& board, int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::None));
    }

    void MoveGenerator::addPawnCaptureMove(const BoardState& board, int fromSquare, int toSquare, int capturedPieceType) {
        addMove(Move(fromSquare, toSquare, Move::Flag::None));
    }

    bool MoveGenerator::getInCheck() const {
        return inCheck;
    }
}  // namespace Chess
