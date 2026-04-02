#include "move_generator.h"

namespace Chess {

    void MoveGenerator::init() {
        PrecomputedMoveData::initialize();
    }

    void MoveGenerator::generateLegalMoves(const BoardState& board, bool genQuiet) {
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

        calculateOpponentAttackData();
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

        if (inDoubleCheck) {
            return;
        }

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

        // calculate attack/check/pin data
        calculateOpponentAttackData();

        const uint64_t friendlyOccupancy = board->getOccupancy(friendlyColour);
        const uint64_t opponentOccupancy = board->getOccupancy(opponentColour);
        const uint64_t allOccupancy = board->getMainBoard();

        const bool isPinned = isPinnedFunc(square);

        switch (pieceType) {
        case PIECE_KING: {
            const int kingSquare = square;
            const auto kingMoveList = PrecomputedMoveData::getKingMovesVector(kingSquare);
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
            int startDir = 0, endDir = 8;
            if (pieceType == PIECE_ROOK) { startDir = 0; endDir = 4; }
            else if (pieceType == PIECE_BISHOP) { startDir = 4; endDir = 8; }
            else { startDir = 0; endDir = 8; }

            for (int dir = startDir; dir < endDir; ++dir) {
                int offset = PrecomputedMoveData::dirOffsets[dir];
                if (isPinned && !isMovingAlongRay(offset, friendlyKingSquare, square)) continue;

                int s = square + offset;
                while (s >= 0 && s < 64) {
                    if (!PrecomputedMoveData::isDirectionalMove(square, s, dir)) break;
                    uint64_t mask = (1ULL << s);
                    if (friendlyOccupancy & mask) break;
                    const bool isCapture = (opponentOccupancy & mask) != 0;
                    const bool movePreventsCheck = squareIsInCheckRay(s);
                    if (!inCheck || movePreventsCheck) {
                        if (isCapture) pieceMoves.emplace_back(square, s); else pieceMoves.emplace_back(square, s);
                    }
                    if (isCapture || movePreventsCheck) break;
                    s += offset;
                }
            }
            break;
        }
        case PIECE_KNIGHT: {
            if (isPinned) break; // knight pinned can't move
            const auto knightMoveList = PrecomputedMoveData::getKnightMoves(square);
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
                    if (!isPinned || isMovingAlongRay(pushDir, pawnSquare, friendlyKingSquare)) {
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
                const int captureOffset = captureSquare - pawnSquare;
                if (isPinned && !isMovingAlongRay(captureOffset, friendlyKingSquare, pawnSquare)) continue;
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

        const PieceList& opponentKnights = board->getPieceList(opponentColour, PIECE_KNIGHT);
        const PieceList& opponentPawns = board->getPieceList(opponentColour, PIECE_PAWN);

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

        for (int i = 0; i < opponentPawns.count(); ++i) {
            const int pawnSquare = opponentPawns[i];
            uint64_t pawnAttackBitboard = PrecomputedMoveData::getPawnAttackBitboard(opponentColour, pawnSquare);
            opponentPawnAttackMap |= pawnAttackBitboard;
            if (pawnAttackBitboard & (1ULL << friendlyKingSquare)) {
                inDoubleCheck = inCheck;
                inCheck = true;
                checkRayBitmask |= (1ULL << pawnSquare);
            }
        }

        genSlidingAttackMap();
        detectSlidingChecksAndPins();

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

        for (int i = 0; i < opponentRooks.count(); ++i) {
            updateSlidingAttackPiece(opponentRooks[i], 0, 4);
        }
        for (int i = 0; i < opponentQueens.count(); ++i) {
            updateSlidingAttackPiece(opponentQueens[i], 0, 8);
        }
        for (int i = 0; i < opponentBishops.count(); ++i) {
            updateSlidingAttackPiece(opponentBishops[i], 4, 8);
        }
    }

    void MoveGenerator::detectSlidingChecksAndPins() {
        for (int dir = 0; dir < 8; ++dir) {
            const bool isDiagonal = dir > 3;
            const int offset = PrecomputedMoveData::dirOffsets[dir];
            int square = friendlyKingSquare + offset;
            bool seenFriendlyBlocker = false;
            int blockerSquare = -1;
            uint64_t rayMask = 0ULL;

            while (square >= 0 && square < 64) {
                if (!PrecomputedMoveData::isDirectionalMove(friendlyKingSquare, square, dir)) {
                    break;
                }

                rayMask |= (1ULL << square);
                const int pieceType = board->getPieceTypeAt(square);
                if (pieceType == -1) {
                    square += offset;
                    continue;
                }

                const int color = board->getColorAt(square);
                if (color == friendlyColour) {
                    if (!seenFriendlyBlocker) {
                        seenFriendlyBlocker = true;
                        blockerSquare = square;
                    } else {
                        break;
                    }
                } else {
                    const bool sliderMatches = isDiagonal ?
                        (pieceType == PIECE_BISHOP || pieceType == PIECE_QUEEN) :
                        (pieceType == PIECE_ROOK || pieceType == PIECE_QUEEN);

                    if (sliderMatches) {
                        if (seenFriendlyBlocker) {
                            pinsExistInPosition = true;
                            if (blockerSquare >= 0) {
                                pinRayBitmask |= (1ULL << blockerSquare);
                            }
                        } else {
                            inDoubleCheck = inCheck;
                            inCheck = true;
                            checkRayBitmask |= rayMask;
                        }
                    }
                    break;
                }

                square += offset;
            }

            if (inDoubleCheck) {
                break;
            }
        }
    }

    void MoveGenerator::updateSlidingAttackPiece(int startSquare, int startDirIndex, int endDirIndex) {
        const uint64_t allOccupancy = board->getMainBoard();

        for (int dir = startDirIndex; dir < endDirIndex; ++dir) {
            int offset = PrecomputedMoveData::dirOffsets[dir];
            int square = startSquare + offset;

            while (square >= 0 && square < 64) {
                if (!PrecomputedMoveData::isDirectionalMove(startSquare, square, dir)) {
                    break;
                }

                opponentSlidingAttackMap |= (1ULL << square);

                const uint64_t sqMask = (1ULL << square);
                if (square != friendlyKingSquare && (allOccupancy & sqMask)) {
                    break;
                }

                square += offset;
            }
        }
    }

    void MoveGenerator::generateKingMoves() {
        uint64_t friendlyOccupancy = board->getOccupancy(friendlyColour);
        uint64_t opponentOccupancy = board->getOccupancy(opponentColour);
        uint64_t allOccupancy = board->getMainBoard();

        const auto& kingSquares = board->getPieceList(friendlyColour, PIECE_KING);
        if (kingSquares.count() == 0) return;

        const int kingSquare = kingSquares[0];
        const auto kingMoveList = PrecomputedMoveData::getKingMovesVector(kingSquare);

        for (uint8_t toSquare : kingMoveList) {
            uint64_t toMask = (1ULL << toSquare);

            if ((toSquare == kingSquare + 2) || (toSquare == kingSquare - 2)) {
                continue;
            }

            if (friendlyOccupancy & toMask) continue;

            const bool isCapture = (opponentOccupancy & toMask) != 0;
            if (!isCapture && !genQuiets) {
                continue;
            }

            if (!squareIsAttacked(toSquare)) {
                if (isCapture) {
                    addCaptureMove(*board, kingSquare, toSquare);
                } else {
                    addQuietMove(*board, kingSquare, toSquare);
                }
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
        generateSlidingDirections(0, 4);
        generateSlidingDirections(4, 8);
        generateSlidingDirections(0, 8);
    }

    void MoveGenerator::generateSlidingDirections(int startDirIndex, int endDirIndex) {
        int pieceType = -1;
        if (startDirIndex == 0 && endDirIndex == 4) {
            pieceType = PIECE_ROOK;
        } else if (startDirIndex == 4 && endDirIndex == 8) {
            pieceType = PIECE_BISHOP;
        } else if (startDirIndex == 0 && endDirIndex == 8) {
            pieceType = PIECE_QUEEN;
        }

        if (pieceType < 0) return;

        const auto& pieceSquares = board->getPieceList(friendlyColour, pieceType);
        for (int i = 0; i < pieceSquares.count(); ++i) {
            generateSlidingPieceMoves(pieceSquares[i], startDirIndex, endDirIndex);
        }
    }

    void MoveGenerator::generateSlidingPieceMoves(int startSquare, int startDirIndex, int endDirIndex) {
        uint64_t friendlyOccupancy = board->getOccupancy(friendlyColour);
        uint64_t opponentOccupancy = board->getOccupancy(opponentColour);

        const bool isPinned = isPinnedFunc(startSquare);
        if (inCheck && isPinned) {
            return;
        }

        for (int dir = startDirIndex; dir < endDirIndex; ++dir) {
            int offset = PrecomputedMoveData::dirOffsets[dir];

            if (isPinned && !isMovingAlongRay(offset, friendlyKingSquare, startSquare)) {
                continue;
            }

            int square = startSquare + offset;
            while (square >= 0 && square < 64) {
                if (!PrecomputedMoveData::isDirectionalMove(startSquare, square, dir)) {
                    break;
                }

                uint64_t squareMask = (1ULL << square);

                if (friendlyOccupancy & squareMask) break;

                const bool isCapture = (opponentOccupancy & squareMask) != 0;
                const bool movePreventsCheck = squareIsInCheckRay(square);

                if (!inCheck || movePreventsCheck) {
                    if (isCapture) {
                        addCaptureMove(*board, startSquare, square);
                    } else if (genQuiets) {
                        addQuietMove(*board, startSquare, square);
                    }
                }

                if (isCapture || movePreventsCheck) {
                    break;
                }

                square += offset;
            }
        }
    }

    void MoveGenerator::generateKnightMoves() {
        uint64_t friendlyOccupancy = board->getOccupancy(friendlyColour);
        uint64_t opponentOccupancy = board->getOccupancy(opponentColour);

        const auto& knightSquares = board->getPieceList(friendlyColour, PIECE_KNIGHT);

        for (int i = 0; i < knightSquares.count(); ++i) {
            const int knightSquare = knightSquares[i];
            if (isPinnedFunc(knightSquare)) {
                continue;
            }

            const auto knightMoveList = PrecomputedMoveData::getKnightMoves(knightSquare);
            for (uint8_t toSquare : knightMoveList) {
                uint64_t toMask = (1ULL << toSquare);

                if (friendlyOccupancy & toMask) continue;

                const bool isCapture = (opponentOccupancy & toMask) != 0;
                if (!genQuiets && !isCapture) {
                    continue;
                }

                if (inCheck && !squareIsInCheckRay(toSquare)) {
                    continue;
                }

                if (isCapture) {
                    addCaptureMove(*board, knightSquare, toSquare);
                } else {
                    addQuietMove(*board, knightSquare, toSquare);
                }
            }
        }
    }

    void MoveGenerator::generatePawnMoves() {
        uint64_t allOccupancy = board->getMainBoard();
        uint64_t opponentOccupancy = board->getOccupancy(opponentColour);

        const auto& pawnSquares = board->getPieceList(friendlyColour, PIECE_PAWN);
        const int pushDir = (friendlyColour == COLOR_WHITE) ? 8 : -8;
        const int captureRank = (friendlyColour == COLOR_WHITE) ? 6 : 1;
        const int startRank = (friendlyColour == COLOR_WHITE) ? 1 : 6;

        for (int i = 0; i < pawnSquares.count(); ++i) {
            const int pawnSquare = pawnSquares[i];
            const int pawnRank = BoardRepresentation::RankIndex(pawnSquare);
            const bool oneStepFromPromotion = (pawnRank == captureRank);
            const bool isPinned = isPinnedFunc(pawnSquare);

            if (genQuiets) {
                const int pushSquare = pawnSquare + pushDir;
                if (pushSquare >= 0 && pushSquare < 64) {
                    const uint64_t pushMask = (1ULL << pushSquare);
                    if (!(allOccupancy & pushMask)) {
                        if (!isPinned || isMovingAlongRay(pushDir, pawnSquare, friendlyKingSquare)) {
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
                const int captureOffset = captureSquare - pawnSquare;
                if (isPinned && !isMovingAlongRay(captureOffset, friendlyKingSquare, pawnSquare)) {
                    continue;
                }

                uint64_t captureMask = (1ULL << captureSquare);

                if (opponentOccupancy & captureMask) {
                    if (inCheck && !squareIsInCheckRay(captureSquare)) {
                        continue;
                    }

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
    }

    void MoveGenerator::makePromotionMoves(int fromSquare, int toSquare) {
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToQueen));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToRook));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToKnight));
        addMove(Move(fromSquare, toSquare, Move::Flag::PromoteToBishop));
    }

    bool MoveGenerator::isMovingAlongRay(int rayDir, int startSquare, int targetSquare) {
        const int moveDir = PrecomputedMoveData::getDirectionOffset(startSquare, targetSquare);
        return (moveDir == rayDir || moveDir == -rayDir);
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
        BoardState testBoard = *board;
        applyEnPassantMove(testBoard, startSquare, targetSquare, epCapturedPawnSquare);

        const int capturingColor = board->getSide();
        const int opponentColor = capturingColor ^ 1;
        const int friendlyKingSq = testBoard.getPieceList(capturingColor, PIECE_KING)[0];

        uint64_t opponentKing = testBoard.getPieceBoards()[opponentColor][PIECE_KING];
        if (PrecomputedMoveData::getKingMoves(friendlyKingSq) & opponentKing) return true;

        return isSquareAttackedByColor(friendlyKingSq, opponentColor, testBoard);
    }

    bool MoveGenerator::isSquareAttackedByColor(int square, int opponentColor, const BoardState& testBoard) {
        const auto& pieceBoards = testBoard.getPieceBoards();
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

        uint64_t slidingPieces = opponentBishops | opponentRooks | opponentQueens;
        while (slidingPieces) {
            const int attackerSq = static_cast<int>(getLSB(slidingPieces));
            const int piece = testBoard.getPieceTypeAt(attackerSq);

            if ((piece == PIECE_BISHOP || piece == PIECE_QUEEN)) {
                if (PrecomputedMoveData::getBishopMoves(attackerSq) & (1ULL << square)) {
                    if (isPathClear(attackerSq, square, testBoard)) return true;
                }
            }

            if ((piece == PIECE_ROOK || piece == PIECE_QUEEN)) {
                if (PrecomputedMoveData::getRookMoves(attackerSq) & (1ULL << square)) {
                    if (isPathClear(attackerSq, square, testBoard)) return true;
                }
            }

            slidingPieces &= (slidingPieces - 1);
        }

        return false;
    }

    bool MoveGenerator::isPathClear(int fromSquare, int toSquare, const BoardState& testBoard) {
        int dirOffset = PrecomputedMoveData::getDirectionOffset(fromSquare, toSquare);
        int currentSq = fromSquare + dirOffset;
        while (currentSq != toSquare) {
            if (testBoard.getPieceTypeAt(currentSq) != -1) return false;
            currentSq += dirOffset;
        }
        return true;
    }

    void MoveGenerator::applyEnPassantMove(BoardState& testBoard, int startSquare, int targetSquare, int epCapturedPawnSquare) {
        const int capturingColor = testBoard.getSide();
        const int opponentColor = capturingColor ^ 1;

        testBoard.getPieceBoards()[opponentColor][PIECE_PAWN] &= ~(1ULL << epCapturedPawnSquare);
        testBoard.updateOccupancy(epCapturedPawnSquare, opponentColor, false);
        testBoard.clearMailboxSquare(epCapturedPawnSquare);
        testBoard.removePieceFromList(opponentColor, PIECE_PAWN, epCapturedPawnSquare);

        Move epMove(startSquare, targetSquare, Move::Flag::EnPassantCapture);
        testBoard.makeMove(epMove);
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
