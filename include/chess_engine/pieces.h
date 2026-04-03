#ifndef PIECE_CONST_H
#define PIECE_CONST_H

#include <cstdint>
#include <algorithm>

namespace Chess {
    // Piece type constants (occupies bits 0-2)
    // Values are chosen such that sliding piece types have bit 2 set (BISHOP=5, ROOK=6, QUEEN=7)
    constexpr int PIECE_KING = 0;
    constexpr int PIECE_PAWN = 1;
    constexpr int PIECE_KNIGHT = 2;
    constexpr int PIECE_BISHOP = 3;    // bit pattern: 0b101
    constexpr int PIECE_ROOK = 4;      // bit pattern: 0b110
    constexpr int PIECE_QUEEN = 5;     // bit pattern: 0b111
    constexpr int PIECE_NONE = 6;

    // Color constants (occupies bits 3-4)
    // WHITE = 0b01000 (bit 3), BLACK = 0b10000 (bit 4)
    constexpr int COLOR_BLACK = 0;
    constexpr int COLOR_WHITE = 1; 

	static int getPieceValue(int pieceType) {
		switch (pieceType) {
		case PIECE_KING:
			return 20000;
		case PIECE_QUEEN:
			return 900;
		case PIECE_ROOK:
			return 500;
		case PIECE_BISHOP:
			return 330;
		case PIECE_KNIGHT:
			return 320;
		case PIECE_PAWN:
			return 100;
		default:
			return 0;
		}
	}
}
#endif 

