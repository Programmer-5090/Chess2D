#include "board_state.h"
#include "fen_util.h"

namespace Chess {

    void BoardState::updateCastlingRights(int pieceType, int fromFile, int fromRank,
                                         int capturedType, int captureFile, int captureRank) {
        if (pieceType == PIECE_KING) {
            if (side == COLOR_WHITE) {
                castleRights &= ~0x03;
            } else {
                castleRights &= ~0x0C;
            }
        }

        if (pieceType == PIECE_ROOK) {
            if (fromFile == 0 && fromRank == 0) castleRights &= ~0x02;
            if (fromFile == 7 && fromRank == 0) castleRights &= ~0x01;
            if (fromFile == 0 && fromRank == 7) castleRights &= ~0x08;
            if (fromFile == 7 && fromRank == 7) castleRights &= ~0x04;
        }

        if (capturedType == PIECE_ROOK) {
            if (captureFile == 0 && captureRank == 0) castleRights &= ~0x02;
            if (captureFile == 7 && captureRank == 0) castleRights &= ~0x01;
            if (captureFile == 0 && captureRank == 7) castleRights &= ~0x08;
            if (captureFile == 7 && captureRank == 7) castleRights &= ~0x04;
        }
    }

    void BoardState::updateEnPassantSquare(int fromSquare, int toSquare, int pieceType) {
        enPas = -1;
        if (pieceType != PIECE_PAWN) return;

        const int fromRank = BoardRepresentation::RankIndex(fromSquare);
        const int toRank = BoardRepresentation::RankIndex(toSquare);
        const int file = BoardRepresentation::FileIndex(fromSquare);

        if (side == COLOR_WHITE && fromRank == 1 && toRank == 3) {
            enPas = BoardRepresentation::IndexFromCoord(file, 2);
        }
        else if (side == COLOR_BLACK && fromRank == 6 && toRank == 4) {
            enPas = BoardRepresentation::IndexFromCoord(file, 5);
        }
    }

    BoardState::BoardState() {
        if (!zobristKeys.isInitialized()) {
            zobristKeys.init();
        }
        reset();
        moveHistory.reserve(2084);
    }

    void BoardState::init(const std::string& fen) {
        if (fen.empty()) {
            reset();
        } else {
            loadFEN(fen);
        }
    }

    void BoardState::reset() {
        side = COLOR_WHITE;
        enPas = -1;
        fiftyMove = 0;
        hisPly = 0;
        castleRights = 0x0F;

        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                pieceBoards[color][type] = 0;
            }
        }

        pieceBoards[COLOR_WHITE][PIECE_PAWN] = 0x000000000000FF00ULL;
        pieceBoards[COLOR_WHITE][PIECE_ROOK] = 0x0000000000000081ULL;
        pieceBoards[COLOR_WHITE][PIECE_KNIGHT] = 0x0000000000000042ULL;
        pieceBoards[COLOR_WHITE][PIECE_BISHOP] = 0x0000000000000024ULL;
        pieceBoards[COLOR_WHITE][PIECE_QUEEN] = 0x0000000000000008ULL;
        pieceBoards[COLOR_WHITE][PIECE_KING] = 0x0000000000000010ULL;

        pieceBoards[COLOR_BLACK][PIECE_PAWN] = 0x00FF000000000000ULL;
        pieceBoards[COLOR_BLACK][PIECE_ROOK] = 0x8100000000000000ULL;
        pieceBoards[COLOR_BLACK][PIECE_KNIGHT] = 0x4200000000000000ULL;
        pieceBoards[COLOR_BLACK][PIECE_BISHOP] = 0x2400000000000000ULL;
        pieceBoards[COLOR_BLACK][PIECE_QUEEN] = 0x0800000000000000ULL;
        pieceBoards[COLOR_BLACK][PIECE_KING] = 0x1000000000000000ULL;

        rebuildOccupancy();
        rebuildMailbox();
        rebuildPieceLists();
        posKey = generatePosKey();
    }

    void BoardState::updateOccupancy(int square64, int color, bool add) {
        const uint64_t mask = (1ULL << square64);
        if (add) {
            if (color == COLOR_WHITE) {
                whitePieces |= mask;
            } else {
                blackPieces |= mask;
            }
        } else {
            if (color == COLOR_WHITE) {
                whitePieces &= ~mask;
            } else {
                blackPieces &= ~mask;
            }
        }
        mainBoard = whitePieces | blackPieces;
    }

    void BoardState::updateMailbox(int square64, int pieceType) {
        const int index120 = toMailboxIndex(square64);
        mailbox[index120] = pieceType;
    }

    void BoardState::clearMailboxSquare(int square64) {
        const int index120 = toMailboxIndex(square64);
        mailbox[index120] = -1;
    }

    void BoardState::rebuildOccupancy() {
        whitePieces = 0;
        blackPieces = 0;

        for (int type = 0; type < 6; ++type) {
            whitePieces |= pieceBoards[COLOR_WHITE][type];
        }

        for (int type = 0; type < 6; ++type) {
            blackPieces |= pieceBoards[COLOR_BLACK][type];
        }

        mainBoard = whitePieces | blackPieces;
    }

    void BoardState::rebuildMailbox() {
        mailbox.fill(-1);

        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                uint64_t pieceBoard = pieceBoards[color][type];

                while (pieceBoard) {
                    const int lowestSetBit = static_cast<int>(getLSB(pieceBoard));
                    const int mailboxIndex = toMailboxIndex(lowestSetBit);
                    mailbox[mailboxIndex] = type;
                    pieceBoard &= (pieceBoard - 1);
                }
            }
        }
    }

    void BoardState::rebuildPieceLists() {
        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                pieceLists[color][type].clear();
            }
        }

        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                uint64_t pieceBoard = pieceBoards[color][type];

                while (pieceBoard) {
                    const int square = static_cast<int>(getLSB(pieceBoard));
                    pieceLists[color][type].add(square);
                    pieceBoard &= (pieceBoard - 1);
                }
            }
        }
    }

    void BoardState::updatePieceList(int color, int pieceType, int fromSquare, int toSquare) {
        pieceLists[color][pieceType].move(fromSquare, toSquare);
    }

    void BoardState::removePieceFromList(int color, int pieceType, int square) {
        pieceLists[color][pieceType].remove(square);
    }

    void BoardState::addPieceToList(int color, int pieceType, int square) {
        pieceLists[color][pieceType].add(square);
    }

    void BoardState::makeMove(Move move) {
        const int fromSquare = move.startSquare();
        const int toSquare = move.targetSquare();

        const uint64_t fromMask = (1ULL << fromSquare);
        const uint64_t toMask = (1ULL << toSquare);
        const int toIndex120 = toMailboxIndex(toSquare);

        const int pieceType = getPieceTypeAt(fromSquare);
        if (pieceType < 0) {
            return;
        }

        int capturedType = -1;
        int capturedColor = -1;
        int capturedSquare = toSquare;

        const bool isEnPassant = (move.flag() == Move::Flag::EnPassantCapture);
        if (isEnPassant && pieceType == PIECE_PAWN) {
            capturedSquare = toSquare + ((side == COLOR_WHITE) ? -8 : 8);
            const int capType = getPieceTypeAt(capturedSquare);
            const int capColor = getColorAt(capturedSquare);
            if (capType == PIECE_PAWN && capColor == (side ^ 1)) {
                capturedType = capType;
                capturedColor = capColor;
            }
        } else if (mainBoard & toMask) {
            capturedType = mailbox[toIndex120];
            capturedColor = getColorAt(toSquare);
        }

        MoveHistoryEntry historyEntry;
        historyEntry.move = move;
        historyEntry.capturedPieceType = capturedType;
        historyEntry.capturedPieceColor = capturedColor;
        historyEntry.capturedSquare = (capturedType != -1) ? capturedSquare : -1;
        historyEntry.previousSide = side;
        historyEntry.previousEnPas = enPas;
        historyEntry.previousFiftyMove = fiftyMove;
        historyEntry.previousCastleRights = castleRights;

        const int fromFile = BoardRepresentation::FileIndex(fromSquare);
        const int toFile = BoardRepresentation::FileIndex(toSquare);
        const int fromRank = BoardRepresentation::RankIndex(fromSquare);
        const int toRank = BoardRepresentation::RankIndex(toSquare);

        if (capturedType != -1) {
            const uint64_t captureMask = (1ULL << capturedSquare);
            pieceBoards[capturedColor][capturedType] &= ~captureMask;
            updateOccupancy(capturedSquare, capturedColor, false);
            clearMailboxSquare(capturedSquare);
            removePieceFromList(capturedColor, capturedType, capturedSquare);
        }

        int finalPieceType = pieceType;
        if (move.isPromotion()) {
            finalPieceType = move.promotionPieceType();
        }

        pieceBoards[side][pieceType] &= ~fromMask;
        pieceBoards[side][finalPieceType] |= toMask;

        updateOccupancy(fromSquare, side, false);
        updateOccupancy(toSquare, side, true);

        clearMailboxSquare(fromSquare);
        updateMailbox(toSquare, finalPieceType);

        updateCastlingRights(pieceType, fromFile, fromRank, capturedType, toFile, toRank);
        updateEnPassantSquare(fromSquare, toSquare, pieceType);

        side ^= 1;
        hisPly++;

        if (move.isPromotion()) {
            removePieceFromList(side ^ 1, pieceType, fromSquare);
            addPieceToList(side ^ 1, finalPieceType, toSquare);
        } else {
            updatePieceList(side ^ 1, pieceType, fromSquare, toSquare);
        }

        if (pieceType == PIECE_KING) {
            if (toFile - fromFile == 2) {
                const int rookFromSq = BoardRepresentation::IndexFromCoord(7, fromRank);
                const int rookToSq = BoardRepresentation::IndexFromCoord(5, fromRank);
                const uint64_t rookFromMask = (1ULL << rookFromSq);
                const uint64_t rookToMask = (1ULL << rookToSq);
                const int rookColor = side ^ 1;
                pieceBoards[rookColor][PIECE_ROOK] &= ~rookFromMask;
                pieceBoards[rookColor][PIECE_ROOK] |= rookToMask;
                updateOccupancy(rookFromSq, rookColor, false);
                updateOccupancy(rookToSq, rookColor, true);
                clearMailboxSquare(rookFromSq);
                updateMailbox(rookToSq, PIECE_ROOK);
                updatePieceList(rookColor, PIECE_ROOK, rookFromSq, rookToSq);
            } else if (fromFile - toFile == 2) {
                const int rookFromSq = BoardRepresentation::IndexFromCoord(0, fromRank);
                const int rookToSq = BoardRepresentation::IndexFromCoord(3, fromRank);
                const uint64_t rookFromMask = (1ULL << rookFromSq);
                const uint64_t rookToMask = (1ULL << rookToSq);
                const int rookColor = side ^ 1;
                pieceBoards[rookColor][PIECE_ROOK] &= ~rookFromMask;
                pieceBoards[rookColor][PIECE_ROOK] |= rookToMask;
                updateOccupancy(rookFromSq, rookColor, false);
                updateOccupancy(rookToSq, rookColor, true);
                clearMailboxSquare(rookFromSq);
                updateMailbox(rookToSq, PIECE_ROOK);
                updatePieceList(rookColor, PIECE_ROOK, rookFromSq, rookToSq);
            }
        }

        posKey = generatePosKey();
        moveHistory.push_back(historyEntry);
    }

    bool BoardState::unmakeMove() {
        if (moveHistory.empty()) {
            std::cout << "BoardState::unmakeMove: No moves to undo\n";
            return false;
        }

        const MoveHistoryEntry& historyEntry = moveHistory.back();
        const Move move = historyEntry.move;

        const int fromSquare = move.startSquare();
        const int toSquare = move.targetSquare();
        const uint64_t fromMask = (1ULL << fromSquare);
        const uint64_t toMask = (1ULL << toSquare);

        const int movingSide = historyEntry.previousSide;

        const int pieceAtDestination = getPieceTypeAt(toSquare);
        if (pieceAtDestination < 0) {
            std::cout << "BoardState::unmakeMove: No piece at destination square\n";
            return false;
        }

        const int originalPieceType = move.isPromotion() ? PIECE_PAWN : pieceAtDestination;

        pieceBoards[movingSide][pieceAtDestination] &= ~toMask;
        pieceBoards[movingSide][originalPieceType] |= fromMask;

        updateOccupancy(toSquare, movingSide, false);
        updateOccupancy(fromSquare, movingSide, true);

        clearMailboxSquare(toSquare);
        updateMailbox(fromSquare, originalPieceType);

        if (historyEntry.capturedPieceType != -1) {
            const int capturedColor = historyEntry.capturedPieceColor;
            const int capturedType = historyEntry.capturedPieceType;
            const int capturedSquare = historyEntry.capturedSquare;
            const uint64_t capturedMask = (1ULL << capturedSquare);

            pieceBoards[capturedColor][capturedType] |= capturedMask;
            updateOccupancy(capturedSquare, capturedColor, true);
            updateMailbox(capturedSquare, capturedType);
            addPieceToList(capturedColor, capturedType, capturedSquare);
        }

        if (move.isPromotion()) {
            removePieceFromList(movingSide, pieceAtDestination, toSquare);
            addPieceToList(movingSide, originalPieceType, fromSquare);
        } else {
            updatePieceList(movingSide, originalPieceType, toSquare, fromSquare);
        }

        if (originalPieceType == PIECE_KING) {
            const int fromFile = BoardRepresentation::FileIndex(fromSquare);
            const int toFile = BoardRepresentation::FileIndex(toSquare);
            const int fromRank = BoardRepresentation::RankIndex(fromSquare);

            if (toFile - fromFile == 2) {
                const int rookFromSq = BoardRepresentation::IndexFromCoord(5, fromRank);
                const int rookToSq = BoardRepresentation::IndexFromCoord(7, fromRank);
                const uint64_t rookFromMask = (1ULL << rookFromSq);
                const uint64_t rookToMask = (1ULL << rookToSq);

                pieceBoards[movingSide][PIECE_ROOK] &= ~rookFromMask;
                pieceBoards[movingSide][PIECE_ROOK] |= rookToMask;
                updateOccupancy(rookFromSq, movingSide, false);
                updateOccupancy(rookToSq, movingSide, true);
                clearMailboxSquare(rookFromSq);
                updateMailbox(rookToSq, PIECE_ROOK);
                updatePieceList(movingSide, PIECE_ROOK, rookFromSq, rookToSq);
            } else if (fromFile - toFile == 2) {
                const int rookFromSq = BoardRepresentation::IndexFromCoord(3, fromRank);
                const int rookToSq = BoardRepresentation::IndexFromCoord(0, fromRank);
                const uint64_t rookFromMask = (1ULL << rookFromSq);
                const uint64_t rookToMask = (1ULL << rookToSq);

                pieceBoards[movingSide][PIECE_ROOK] &= ~rookFromMask;
                pieceBoards[movingSide][PIECE_ROOK] |= rookToMask;
                updateOccupancy(rookFromSq, movingSide, false);
                updateOccupancy(rookToSq, movingSide, true);
                clearMailboxSquare(rookFromSq);
                updateMailbox(rookToSq, PIECE_ROOK);
                updatePieceList(movingSide, PIECE_ROOK, rookFromSq, rookToSq);
            }
        }

        side = historyEntry.previousSide;
        enPas = historyEntry.previousEnPas;
        fiftyMove = historyEntry.previousFiftyMove;
        castleRights = historyEntry.previousCastleRights;
        hisPly--;

        posKey = generatePosKey();
        moveHistory.pop_back();

        return true;
    }

    uint64_t BoardState::generatePosKey() const {
        uint64_t finalKey = 0ULL;

        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                uint64_t board = pieceBoards[color][type];
                const int pieceIndex = color * 6 + type + 1;

                while (board) {
                    const int sq64 = static_cast<int>(getLSB(board));
                    const int sq120 = toMailboxIndex(sq64);
                    finalKey ^= zobristKeys.getPieceKey(pieceIndex, sq120);
                    board &= (board - 1);
                }
            }
        }

        if (side == COLOR_WHITE) {
            finalKey ^= zobristKeys.getSideKey();
        }

        finalKey ^= zobristKeys.getCastleKey(castleRights);

        if (enPas >= 0 && enPas < 64) {
            const int epFile = BoardRepresentation::FileIndex(enPas);
            finalKey ^= zobristKeys.getPieceKey(0, epFile);
        }

        return finalKey;
    }

    bool BoardState::checkBoard() const {
        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                uint64_t board = pieceBoards[color][type];
                while (board) {
                    const int sq64 = static_cast<int>(getLSB(board));
                    const int sq120 = toMailboxIndex(sq64);
                    if (mailbox[sq120] != type) {
                        return false;
                    }
                    board &= (board - 1);
                }
            }
        }

        uint64_t calcWhite = 0, calcBlack = 0;
        for (int type = 0; type < 6; ++type) {
            calcWhite |= pieceBoards[COLOR_WHITE][type];
            calcBlack |= pieceBoards[COLOR_BLACK][type];
        }
        if (calcWhite != whitePieces || calcBlack != blackPieces) {
            return false;
        }
        if ((whitePieces | blackPieces) != mainBoard) {
            return false;
        }

        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                const PieceList& list = pieceLists[color][type];
                uint64_t board = pieceBoards[color][type];

                if (popCount(board) != list.count()) {
                    return false;
                }

                while (board) {
                    const int sq64 = static_cast<int>(getLSB(board));
                    if (!list.contains(sq64)) {
                        return false;
                    }
                    board &= (board - 1);
                }
            }
        }

        if (whitePieces & blackPieces) {
            return false;
        }

        return true;
    }

    void BoardState::loadFEN(const std::string& fen) {
        loadFENUtil(*this, fen);
    }

    std::string BoardState::getFEN() const {
        return toFENUtil(*this);
    }
}  // namespace Chess
