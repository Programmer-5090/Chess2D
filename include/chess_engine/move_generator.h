#ifndef MOVE_GENERATOR_H
#define MOVE_GENERATOR_H

#include <array>
#include "precomp_move_data.h"
#include "board_rep.h"
#include "board_state.h"
#include "move.h"
#include "pieces.h"

namespace Chess {

/**
 * @class MoveGenerator
 * @brief Generates legal chess moves for a given position
 * 
 * This class implements a legal move generator with the following features:
 * - Direct legal move generation (check/pin constrained)
 * - Check and pin detection
 * - Double-check handling (king moves only)
 * - Quiescence search support (capture-only moves)
 * - Debug mode for capturing generated move snapshots
 * 
 * Usage:
 *   MoveGenerator gen;
 *   gen.init();
 *   gen.debugTrackPseudoLegal = true;  // Optional: snapshot generated moves
 *   gen.generateLegalMoves(board, true);
 *   for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
 *       Move m = gen.moveList[i];
 *   }
 */
class MoveGenerator {
public:
    std::array<Move, 256> moveList{};
    int moveCount = 0;

    // Debug: Store pseudo-legal moves for analysis
    std::array<Move, 256> pseudoLegalMoves{};
    int pseudoLegalCount = 0;
    bool debugTrackPseudoLegal = false;

    MoveGenerator() = default;

    /**
     * Initialize the move generator
     * Must be called once before generating any moves
     */
    void init();

    /**
     * Generate all legal moves for the current position
     * 
     * @param board The chess position to generate moves from
     * @param genQuiet If true, generate quiet moves; if false, only captures
     * 
     * This function:
     * 1. Sets up position state and calculates attack/check/pin data
     * 2. Generates legal moves directly using check/pin constraints
     * 3. Optionally stores generated moves for debugging
     */
    void generateLegalMoves(const BoardState& board, bool genQuiet);

    /**
     * Generate legal moves using precomputed check/pin constraints.
     *
     * @param board The chess position
     * @param genQuiet If true, generate quiet moves; if false, only captures
     */
    void generateLegalMovesConstrained(const BoardState& board, bool genQuiet);

    std::vector<Move> getPieceMoves(int square, const BoardState* boardState);

    /**
     * Add a move to the move list (with bounds checking)
     */
    void addMove(const Move& move);

    /**
     * Clear the move list and reset counter
     */
    void clearMoves();

    /**
     * @return Number of legal moves generated
     */
    int getLegalMoveCount() const;

    // Query whether the side to move is in check (valid after generateLegalMoves)
    bool getInCheck() const;


private:
    const BoardState* board = nullptr;

    // Position tracking
    bool isWhiteToMove = false;
    int friendlyColour = -1;
    int opponentColour = -1;
    int friendlyKingSquare = -1;
    int friendlyColourIndex = -1;
    int opponentColourIndex = -1;

    // Attack state
    bool inCheck = false;
    bool inDoubleCheck = false;
    bool pinsExistInPosition = false;
    uint64_t checkRayBitmask = 0ULL;
    uint64_t pinRayBitmask = 0ULL;

    // Attack maps
    uint64_t opponentKnightAttacks = 0ULL;
    uint64_t opponentAttackMapNoPawns = 0ULL;
    uint64_t opponentAttackMap = 0ULL;
    uint64_t opponentPawnAttackMap = 0ULL;
    uint64_t opponentSlidingAttackMap = 0ULL;

    // Generation flags
    bool genQuiets = true;

    /**
     * Calculate all opponent attack data for the position
     * 
     * Builds:
     * - Pawn attack map
     * - Knight attack map
     * - Sliding piece attack map
     * - Composite attack maps
     * - Check and pin information
     */
    void calculateOpponentAttackData();

    /**
     * Generate sliding piece attack map and detect checks/pins
     * 
     * Analyzes all opponent rooks, bishops, and queens to:
     * - Build sliding attack bitboard
     * - Detect direct checks
     * - Detect pinned pieces
     */
    void genSlidingAttackMap();

    void detectSlidingChecksAndPins();

    /**
     * Update attack data for a single sliding piece
     * 
     * Detects:
     * - Direct checks (ray hits friendly king)
     * - Pin rays (ray through friendly piece to king)
     * - Attack contributions to sliding attack map
     * 
     * @param startSquare The square of the sliding piece
     * @param startDirIndex First direction to analyze (0=N, 4=NW, etc.)
     * @param endDirIndex End of direction range (exclusive)
     */
    void updateSlidingAttackPiece(int startSquare, int startDirIndex, int endDirIndex);

    void generateKingMoves();

    void generateSlidingMoves();

    void generateSlidingDirections(int startDirIndex, int endDirIndex);

    void generateSlidingPieceMoves(int startSquare, int startDirIndex, int endDirIndex);

    void generateKnightMoves();

    void generatePawnMoves();

    void makePromotionMoves(int fromSquare, int toSquare);


    bool isMovingAlongRay(int rayDir, int startSquare, int targetSquare);
    
    bool isPinnedFunc(int square);
    
    bool squareIsInCheckRay(int square);
    
    bool hasKingsideCastleRight();
    
    bool hasQueensideCastleRight();

    bool squareIsAttacked(int square);
    
    bool inCheckAfterEnPassant(int startSquare, int targetSquare, int epCapturedPawnSquare);

    bool isSquareAttackedByColor(int square, int opponentColor, const BoardState& testBoard);

    bool isPathClear(int fromSquare, int toSquare, const BoardState& testBoard);

    void applyEnPassantMove(BoardState& testBoard, int startSquare, int targetSquare, int epCapturedPawnSquare);


    void addQuietMove(const BoardState& board, int fromSquare, int toSquare);
    
    void addCaptureMove(const BoardState& board, int fromSquare, int toSquare);
    
    void addEnPassantMove(const BoardState& board, int fromSquare, int toSquare);
    
    void addPawnMove(const BoardState& board, int fromSquare, int toSquare);
    
    void addPawnCaptureMove(const BoardState& board, int fromSquare, int toSquare, int capturedPieceType);
};

}  // namespace Chess

#endif // MOVE_GENERATOR_H