/*
 * Move Representation
 * 
 * To preserve memory during search, moves are stored as 16 bit unsigned integers.
 * The bit layout is as follows:
 * - Bits 0-5:   from square (0-63)
 * - Bits 6-11:  to square (0-63)
 * - Bits 12-15: move flags
 */

#ifndef MOVE_H
#define MOVE_H

#include <cstdint>
#include <string>
#include "board_rep.h"

namespace Chess {

    class Move {
    public:
        // Move flag constants
        struct Flag {
            static constexpr int None = 0;
            static constexpr int EnPassantCapture = 1;
            static constexpr int Castling = 2;
            static constexpr int PromoteToQueen = 3;
            static constexpr int PromoteToKnight = 4;
            static constexpr int PromoteToRook = 5;
            static constexpr int PromoteToBishop = 6;
            static constexpr int PawnTwoForward = 7;
        };

    private:
        // Bit masks for extracting components from the move value
        static constexpr uint16_t START_SQUARE_MASK = 0b0000000000111111;  // bits 0-5
        static constexpr uint16_t TARGET_SQUARE_MASK = 0b0000111111000000; // bits 6-11
        static constexpr uint16_t FLAG_MASK = 0b1111000000000000;          // bits 12-15

        uint16_t moveValue;

    public:
        // Default constructor - creates an invalid move
        Move() : moveValue(0) {}

        // Constructor from raw move value
        explicit Move(uint16_t value) : moveValue(value) {}

        // Constructor from start and target squares
        Move(int startSquare, int targetSquare) 
            : moveValue(static_cast<uint16_t>(startSquare | (targetSquare << 6))) {}

        // Constructor from start square, target square, and flag
        Move(int startSquare, int targetSquare, int flag)
            : moveValue(static_cast<uint16_t>(startSquare | (targetSquare << 6) | (flag << 12))) {}

        // Extract the starting square (0-63)
        int startSquare() const {
            return moveValue & START_SQUARE_MASK;
        }

        // Extract the target square (0-63)
        int targetSquare() const {
            return (moveValue & TARGET_SQUARE_MASK) >> 6;
        }

        // Extract the move flag
        int flag() const {
            return (moveValue & FLAG_MASK) >> 12;
        }

        // Check if this move is a promotion
        bool isPromotion() const {
            int moveFlag = flag();
            return moveFlag == Flag::PromoteToQueen || 
                   moveFlag == Flag::PromoteToRook || 
                   moveFlag == Flag::PromoteToKnight || 
                   moveFlag == Flag::PromoteToBishop;
        }

        // Get the piece type that the pawn is promoted to
        int promotionPieceType() const {
            switch (flag()) {
                case Flag::PromoteToRook:
                    return 4;    // PIECE_ROOK
                case Flag::PromoteToKnight:
                    return 2;    // PIECE_KNIGHT
                case Flag::PromoteToBishop:
                    return 3;    // PIECE_BISHOP
                case Flag::PromoteToQueen:
                    return 5;    // PIECE_QUEEN
                default:
                    return -1;   // No promotion
            }
        }

        // Get the raw 16-bit move value
        uint16_t value() const {
            return moveValue;
        }

        // Check if this is an invalid move
        bool isInvalid() const {
            return moveValue == 0;
        }

        // Check if this is a valid move
        bool isValid() const {
            return moveValue != 0;
        }

        // Get the string representation of the move (e.g., "e2-e4")
        std::string toString() const {
            return BoardRepresentation::SquareNameFromIndex(startSquare()) + "-" + 
                   BoardRepresentation::SquareNameFromIndex(targetSquare());
        }

        // Equality comparison
        bool operator==(const Move& other) const {
            return moveValue == other.moveValue;
        }

        // Inequality comparison
        bool operator!=(const Move& other) const {
            return moveValue != other.moveValue;
        }

        // Create an invalid move
        static Move invalid() {
            return Move(0);
        }

        // Check if two moves are the same
        static bool same(const Move& a, const Move& b) {
            return a.moveValue == b.moveValue;
        }
    };

}  // namespace Chess

#endif  // MOVE_H