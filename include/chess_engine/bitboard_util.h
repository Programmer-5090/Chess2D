/*
 * Bitboard Utilities
 * 
 * This header provides efficient bitboard operations for chess.
 * A bitboard is a 64-bit unsigned integer where each bit represents a square on the chess board.
 * 
 * Bitboard Layout (standard orientation):
 *   Rank 7 (Black's back rank): bits 56-63
 *   Rank 6: bits 48-55
 *   Rank 5: bits 40-47
 *   Rank 4: bits 32-39
 *   Rank 3: bits 24-31
 *   Rank 2: bits 16-23
 *   Rank 1 (White's back rank): bits 8-15
 *   Rank 0: bits 0-7
 * 
 *   File A (queenside): bits 0, 8, 16, 24, 32, 40, 48, 56
 *   File H (kingside): bits 7, 15, 23, 31, 39, 47, 55, 63
 * 
 * Example:
 *   uint64_t whitePawns = 0x000000000000FF00ULL;  // Pawns on rank 1
 *   uint64_t e4Square = 0x0000000010000000ULL;    // e4 = square 28
 * 
 * Why Bitboards?
 * - O(1) operations for checking piece presence
 * - Fast bitwise operations: AND, OR, XOR for move generation
 * - Parallel operations: check multiple squares in one CPU instruction
 * - Cache-friendly: entire board state in one 64-bit value
 * - Performance: essential for high-speed chess engines
 */

#ifndef BITBOARD_UTILS_H
#define BITBOARD_UTILS_H

#include <cstdint>
#include <bit>

namespace Chess {

    /*
     * Count the number of set bits (population count)
     * 
     * @param bb The bitboard to analyze
     * @return The number of '1' bits in the bitboard
     * 
     * Use case: Count how many pieces of a type are on the board
     * Time complexity: O(1) - hardware instruction
     * 
     * Example:
     *   uint64_t pawns = 0x000000000000FF00ULL;  // 8 white pawns
     *   int count = popCount(pawns);             // Returns 8
     */
    inline int popCount(uint64_t bb) {
        return std::popcount(bb);
    }

    /*
     * Get the position of the Least Significant Bit (LSB)
     * 
     * @param bb The bitboard to analyze
     * @return The bit position (0-63) of the first '1' bit, or 64 if empty
     * 
     * Use case: Find the first piece on a bitboard
     * Time complexity: O(1) - hardware instruction
     * 
     * Example:
     *   uint64_t bb = 0x0000000000000100ULL;  // Only bit 8 set
     *   int square = getLSB(bb);              // Returns 8
     * 
     * Note: This is the first step in bitboard iteration
     */
    inline int getLSB(uint64_t bb) {
        return std::countr_zero(bb);
    }

    /*
     * Pop the Least Significant Bit - get LSB position and clear it
     * 
     * @param bb Reference to bitboard (modified in place)
     * @return The bit position (0-63) of the cleared bit
     * 
     * Use case: Iterate through all set bits in a bitboard
     * Time complexity: O(1) per bit, O(n) for all bits where n = popCount(bb)
     * 
     * Example - Iterate through all pieces:
     *   uint64_t pieces = whitePawns;
     *   while (pieces) {
     *       int square = popLSB(pieces);  // Get square and remove it
     *       // Process piece at square...
     *   }
     * 
     * This is the fastest way to iterate through a bitboard
     */
    inline int popLSB(uint64_t& bb) {
        int lsb = getLSB(bb);
        bb &= bb - 1;  // Clear the LSB using bit manipulation trick
        return lsb;
    }

    /*
     * Check if a specific bit is set (non-destructive)
     * 
     * @param bb The bitboard to check
     * @param square The bit position to test (0-63)
     * @return True if the bit at 'square' is set, false otherwise
     * 
     * Use case: Check if a piece occupies a specific square
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t whitePieces = 0x00000000000000FFULL;  // All pieces on rank 0
     *   if (getBit(whitePieces, 4)) {  // Check e1 (square 4)
     *       // White has a piece on e1
     *   }
     */
    inline bool getBit(uint64_t bb, int square) {
        return (bb & (1ULL << square)) != 0;
    }

    /*
     * Set a specific bit to 1 (additive)
     * 
     * @param bb Reference to bitboard (modified in place)
     * @param square The bit position to set (0-63)
     * 
     * Use case: Place a piece on the board
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t whitePawns = 0x0000000000000000ULL;
     *   setBit(whitePawns, 8);   // Add pawn to a2
     *   setBit(whitePawns, 12);  // Add pawn to c2
     */
    inline void setBit(uint64_t& bb, int square) {
        bb |= (1ULL << square);
    }

    /*
     * Clear a specific bit to 0 (subtractive)
     * 
     * @param bb Reference to bitboard (modified in place)
     * @param square The bit position to clear (0-63)
     * 
     * Use case: Remove a piece from the board (capture or move source)
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t whitePawns = 0x000000000000FF00ULL;  // Pawns on rank 1
     *   clearBit(whitePawns, 12);  // Remove pawn from e2
     */
    inline void clearBit(uint64_t& bb, int square) {
        bb &= ~(1ULL << square);
    }

    /*
     * Toggle a specific bit (flip 0->1 or 1->0)
     * 
     * @param bb Reference to bitboard (modified in place)
     * @param square The bit position to toggle (0-63)
     * 
     * Use case: When a piece moves or is captured, toggle source and destination
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t pieces = 0x0000000000000000ULL;
     *   toggleBit(pieces, 12);  // Set bit 12 to 1
     *   toggleBit(pieces, 12);  // Set bit 12 back to 0
     */
    inline void toggleBit(uint64_t& bb, int square) {
        bb ^= (1ULL << square);
    }

    /*
     * Create a bitboard with a single bit set
     * 
     * @param square The bit position (0-63)
     * @return A bitboard with only the specified bit set
     * 
     * Use case: Generate masks for bitwise operations
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t mask = bit(28);  // Create mask for e4
     *   if (occupancy & mask) {   // Check if e4 is occupied
     *       // ...
     *   }
     */
    inline uint64_t bit(int square) {
        return 1ULL << square;
    }

    /*
     * Check if a bitboard has any bits set (non-zero)
     * 
     * @param bb The bitboard to check
     * @return True if any bit is set, false if empty (zero)
     * 
     * Use case: Check if a bitboard represents an empty set
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t blackPieces = 0x00FF000000000000ULL;
     *   if (any(blackPieces)) {
     *       // Black has pieces on the board
     *   }
     */
    inline bool any(uint64_t bb) {
        return bb != 0;
    }

    /*
     * Check if a bitboard is empty (all bits zero)
     * 
     * @param bb The bitboard to check
     * @return True if all bits are zero, false if any bit is set
     * 
     * Use case: Check if all pieces of a type have been removed (e.g., all pawns captured)
     * Time complexity: O(1)
     * 
     * Example:
     *   uint64_t whitePawns = 0x0000000000000000ULL;  // All pawns removed
     *   if (none(whitePawns)) {
     *       // White is out of pawns
     *   }
     */
    inline bool none(uint64_t bb) {
        return bb == 0;
    }

}  // namespace Chess

#endif  // BITBOARD_UTILS_H
