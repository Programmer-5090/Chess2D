#ifndef BOARD_STATE_H
#define BOARD_STATE_H

#include "pieces.h"
#include "bitboard_util.h"
#include "board_rep.h"
#include "move.h"
#include "piece_list.h"
#include "hash_keys.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>

namespace Chess {

    /*
     * Bit indices for the mailbox representation
     * Used for quick piece lookup without iterating through bitboards
     */
    static constexpr int BRD_SQ_NUM = 120;

    // Forward declaration for FEN loading function
    // Defined in fen_util.h
    class BoardState;
    
    

    class BoardState {
    private:
        // Zobrist hash keys for position hashing (centralized in ZobristKeys)
        static inline ZobristKeys zobristKeys;

        // Bitboard representation - one bitboard per piece type and color
        // Indexed as: pieceBoards[color][pieceType]
        std::array<std::array<uint64_t, 6>, 2> pieceBoards{};

        // Piece lists for efficient piece iteration
        // Indexed as: pieceLists[color][pieceType]
        std::array<std::array<PieceList, 6>, 2> pieceLists{};

        // Occupancy bitboards for quick piece presence checks
        uint64_t whitePieces = 0;     // All white pieces
        uint64_t blackPieces = 0;     // All black pieces
        uint64_t mainBoard = 0;       // All pieces (white | black)

        // Mailbox representation for O(1) piece lookup by square
        // Maps mailbox index (0-119) to piece type, or -1 if empty
        std::array<int, BRD_SQ_NUM> mailbox{};

        // Game state
        int side = COLOR_WHITE;              // Current side to move
        int enPas = -1;                      // En passant target square (-1 if none)
        int fiftyMove = 0;                   // Half-move clock for 50-move rule
        int hisPly = 0;                      // Full move counter
        int castleRights = 0;                // Bitmask of castling rights

        // Game history - stores complete move information for undo
        // Each entry contains: move + game state before move
        struct MoveHistoryEntry {
            Move move;
            int capturedPieceType = -1;      // Type of captured piece (-1 if none)
            int capturedPieceColor = -1;     // Color of captured piece
            int capturedSquare = -1;         // Square where captured piece was removed (handles en passant)
            int previousSide = COLOR_WHITE;
            int previousEnPas = -1;
            int previousFiftyMove = 0;
            int previousCastleRights = 0;
        };
        std::vector<MoveHistoryEntry> moveHistory;

        // Position hash for transposition tables
        uint64_t posKey = 0;

        /*
         * Convert 64-bit square index to 120-bit mailbox index
         * The mailbox is a 10x12 grid with padding to simplify boundary checks
         * Standard 0x88 representation mapping
         */
        inline static constexpr int toMailboxIndex(int square64) {
            const int file = BoardRepresentation::FileIndex(square64);
            const int rank = BoardRepresentation::RankIndex(square64);
            return 21 + file + rank * 10;
        }

        /*
         * Update castling rights based on piece movement
         * Removes castling rights when the king or rook moves from their starting square
         */
        void updateCastlingRights(int pieceType, int fromFile, int fromRank,
                                  int capturedType, int captureFile, int captureRank);

        /*
         * Update en passant square based on pawn movement
         * Sets the en passant square only if a pawn moves two squares forward
         */
        void updateEnPassantSquare(int fromSquare, int toSquare, int pieceType);

    public:
        /*
         * Initialize the board with Zobrist keys (called once in constructor)
         */
        BoardState();

        /*
         * Initialize the board to the standard chess starting position
         * Or to a specific position given a FEN string
         */
        void init(const std::string& fen = "");

        /*
         * Reset the board to the standard chess starting position
         */
        void reset();

        /*
         * Update occupancy bitboards incrementally for a single square change
         * Called when a piece moves or is captured
         */
        void updateOccupancy(int square64, int color, bool add);

        /*
         * Update mailbox incrementally for a single square
         * Called when a piece moves or is captured
         */
        void updateMailbox(int square64, int pieceType);

        /*
         * Clear mailbox entry for a square
         * Called when a piece is removed
         */
        void clearMailboxSquare(int square64);

        /*
         * Rebuild occupancy bitboards from piece bitboards
         * Combines all pieces into aggregate occupancy boards
         * Only called during reset/initialization
         */
        void rebuildOccupancy();

        /*
         * Rebuild mailbox array from piece bitboards
         * Maps each square to its piece type for O(1) lookup
         * Only called during reset/initialization
         */
        void rebuildMailbox();

        /*
         * Rebuild piece lists from piece bitboards
         * Updates cached lists of pieces for each type and color
         */
        void rebuildPieceLists();

        /*
         * Update a piece's position in its piece list
         */
        void updatePieceList(int color, int pieceType, int fromSquare, int toSquare);

        /*
         * Remove a piece from its piece list
         */
        void removePieceFromList(int color, int pieceType, int square);

        /*
         * Add a piece to its piece list
         */
        void addPieceToList(int color, int pieceType, int square);

        /*
         * Get the piece list for a given piece type and color
         */
        const PieceList& getPieceList(int color, int pieceType) const { return pieceLists[color][pieceType]; }

        /*
         * Get the piece type at a given square
         * Returns -1 if the square is empty
         */
        int getPieceTypeAt(int square64) const {
            const int index120 = toMailboxIndex(square64);
            if (index120 < 0 || index120 >= BRD_SQ_NUM) return -1;
            return mailbox[index120];
        }

        /*
         * Get the color of the piece at a given square
         * Returns -1 if the square is empty
         */
        int getColorAt(int square64) const {
            const uint64_t mask = (1ULL << square64);
            if (whitePieces & mask) return COLOR_WHITE;
            if (blackPieces & mask) return COLOR_BLACK;
            return -1;
        }

        /*
         * Make a move on the board
         * Updates all representations (bitboards, mailbox, piece lists)
         * Parameters: move - Move object with from and to squares
         */
        void makeMove(Move move);

        /*
         * Unmake the last move and revert the board to its previous state
         * Restores all bitboards, occupancy, mailbox, piece lists, and game state
         * Returns true if undo was successful, false if no move history available
         */
        bool unmakeMove();

        /*
         * Generate position hash key using Zobrist hashing
         * XORs together piece keys, side key, castling key, and en passant key
         * Uses the centralized ZobristKeys class for key management
         */
        uint64_t generatePosKey() const;

        /*
         * Validate the integrity of the board state
         * Verifies consistency between all board representations:
         * - Piece bitboards match mailbox entries
         * - Occupancy bitboards correctly combine piece bitboards
         * - Piece lists contain the same pieces as bitboards
         * - No overlapping white and black pieces
         * - All pieces are on valid squares
         * 
         * Returns true if all checks pass, false if any inconsistency is found
         * Used for debugging and asserting board state validity during development
         */
        bool checkBoard() const;

        // Getters and Setters

        const std::array<std::array<uint64_t, 6>, 2>& getPieceBoards() const { return pieceBoards; }
        std::array<std::array<uint64_t, 6>, 2>& getPieceBoards() { return pieceBoards; }

        uint64_t getWhitePieces() const { return whitePieces; }
        uint64_t getBlackPieces() const { return blackPieces; }
        uint64_t getMainBoard() const { return mainBoard; }
        uint64_t getOccupancy(int color) const { return (color == COLOR_WHITE) ? whitePieces : blackPieces; }

        std::string getFEN() const;
        void loadFEN(const std::string& fen);

        int getSide() const { return side; }
        void setSide(int s) { side = s; }

        int getEnPas() const { return enPas; }
        void setEnPas(int ep) { enPas = ep; }

        int getFiftyMove() const { return fiftyMove; }
        void setFiftyMove(int fm) { fiftyMove = fm; }

        int getHisPly() const { return hisPly; }
        void setHisPly(int hp) { hisPly = hp; }

        int getCastleRights() const { return castleRights; }
        void setCastleRights(int cr) { castleRights = cr; }

        uint64_t getPosKey() const { return posKey; }
        void setPosKey(uint64_t pk) { posKey = pk; }

        Move getLastMove() const { return moveHistory.empty() ? Move::invalid() : moveHistory.back().move; }

        bool isWhiteToMove() const { return side == COLOR_WHITE; }
        bool isBlackToMove() const { return side == COLOR_BLACK; }
        
        /*
         * Get the number of moves in the history
         */
        int getMoveCount() const { return static_cast<int>(moveHistory.size()); }

        bool hasMovesToUndo() const { return !moveHistory.empty(); }

        /*
         * Get move at a specific index in the history
         * Returns an invalid move if index is out of bounds
         */
        Move getMoveAt(int index) const {
            if (index < 0 || index >= static_cast<int>(moveHistory.size())) return Move::invalid();
            return moveHistory[index].move;
        }

        /*
         * Clear all move history
         * Useful when starting a new game
         */
        void clearHistory() { moveHistory.clear(); }
    };

}  // namespace Chess

#endif  // BOARD_STATE_H