/*
 * Piece List
 * 
 * Maintains a list of squares occupied by pieces of a particular type and color.
 * This allows for efficient iteration over all pieces of a given type without
 * needing to iterate through the entire bitboard.
 */

#ifndef PIECE_LIST_H
#define PIECE_LIST_H

#include <vector>
#include <algorithm>

namespace Chess {

    class PieceList {
    private:
        std::vector<int> squares;

    public:
        PieceList() = default;

        // Get the number of pieces in this list
        int count() const;

        // Add a piece at the given square
        void add(int square);

        // Remove a piece from the given square
        void remove(int square);

        // Move a piece from one square to another
        void move(int fromSquare, int toSquare);
        
        bool contains(int square) const;

        // Clear all pieces from this list
        void clear();

        // Get the underlying vector (for iteration)
        const std::vector<int>& getSquares() const;

        // Direct access to squares by index
        int operator[](int index) const;
    };

}  // namespace Chess

#endif  // PIECE_LIST_H
