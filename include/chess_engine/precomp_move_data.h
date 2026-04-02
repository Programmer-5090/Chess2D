#ifndef PRECOMPUTED_MOVE_DATA_H
#define PRECOMPUTED_MOVE_DATA_H

#include <iostream>
#include <cstdint>
#include <array>
#include <vector>
#include <algorithm>


namespace Chess
{
	class PrecomputedMoveData {
		/*
		* 64 Sq Board
			56, 57, 58, 59, 60, 61, 62, 63
			48, 49, 50, 51, 52, 53, 54, 55
			40, 41, 42, 43, 44, 45, 46, 47
			32, 33, 34, 35, 36, 37, 38, 39
			24, 25, 26, 27, 28, 29, 30, 31
			16, 17, 18, 19, 20, 21, 22, 23
			8 , 9 , 10, 11, 12, 13, 14, 15
			0 , 1 , 2 , 3 , 4 , 5 , 6 , 7
		*/
	public:

		enum Directions {
			NORTH = 0, SOUTH = 1, WEST = 2, EAST = 3,
			NORTH_WEST = 4, SOUTH_EAST = 5, NORTH_EAST = 6, SOUTH_WEST = 7
		};

		// Direction offsets: (N, S, W, E, NW, SE, NE, SW)
		static const std::array<int, 8> dirOffsets;

		// Initialize all lookup tables - must be called once before using the class
		static void initialize();


		// Get all valid knight moves from a square
		static std::vector<uint8_t> getKnightMoves(int square);

		// Get all valid king moves from a square
		static std::vector<uint8_t> getKingMovesVector(int square);

		// Get rook move bitboard for a square
		static uint64_t getRookMoves(int square);

		// Get bishop move bitboard for a square
		static uint64_t getBishopMoves(int square);

		// Get queen move bitboard for a square
		static uint64_t getQueenMoves(int square);

		// Get king move bitboard for a square
		static uint64_t getKingMoves(int square);

		// Get knight attack bitboard for a square
		static uint64_t getKnightAttacks(int square);

		// Get pawn attack squares for white pawns
		static std::vector<int> getPawnAttacksWhite(int square);

		// Get pawn attack squares for black pawns
		static std::vector<int> getPawnAttacksBlack(int square);

		// Get pawn attack bitboard for a given color and square
		static uint64_t getPawnAttackBitboard(int color, int square);

		// Get orthogonal (Manhattan/Taxicab) distance between two squares
		static int getOrthogonalDistance(int squareA, int squareB);

		// Get king (Chebyshev) distance between two squares
		static int getKingDistance(int squareA, int squareB);

		// Get Manhattan distance from a square to the board center
		static int getCentreManhattanDistance(int square);

		// Check if a move is in a specific direction from a source square
		static bool isDirectionalMove(int fromSquare, int toSquare, int direction);

		// Get the direction offset between two squares (returns the signed offset)
		static int getDirectionOffset(int fromSquare, int toSquare);

		// Determine which direction (0-7) connects two squares, or -1 if not aligned
		static int getDirection(int fromSquare, int toSquare);

		// Check if a square index is valid (0-63)
		static bool isValidSquare(int square);

		// Check if a move is a valid knight move
		static bool isValidKnightMove(int fromSquare, int toSquare);

		// Check if a move is a valid king move
		static bool isValidKingMove(int fromSquare, int toSquare);

		// Check if a move is a valid pawn attack for the given color
		static bool isValidPawnAttack(int fromSquare, int toSquare, int color);

	private:
		// Static initialization flag
		static bool initialized;

		static const std::array<int, 8> allKnightJumps;
		static std::array<std::array<int, 8>, 64> squareEdges;

		// Direction lookup (for determining direction between two squares)
		static std::array<int, 127> directionLookup;

		// Arrays for fixed-count moves
		static std::array<std::vector<uint8_t>, 64> knightMoves;
		static std::array<std::vector<uint8_t>, 64> kingMoves;

		// Bitboards for sliding pieces (efficient bit operations)
		static std::array<uint64_t, 64> queenMovesBitboards;
		static std::array<uint64_t, 64> rookMovesBitboards;
		static std::array<uint64_t, 64> bishopMoveBitboards;

		// Pawn attack directions for white and black (WHITE(NW, NE); BLACK(SW SE))
		static const std::array<std::array<uint8_t, 2>, 2> pawnAttackDirections;

		// Pawn attack squares
		static std::array<std::vector<int>, 64> pawnAttacksWhite;
		static std::array<std::vector<int>, 64> pawnAttacksBlack;

		// Bitboards for attack patterns
		static std::array<uint64_t, 64> kingAttackBitboards;
		static std::array<uint64_t, 64> knightAttackBitboards;
		static std::array<std::array<uint64_t, 64>, 2> pawnAttackBitboards; // [color][square]

		// Distance lookups
		static std::array<std::array<int, 64>, 64> orthogonalDistance;
		static std::array<std::array<int, 64>, 64> kingDistance;

		// Distance from center (Manhattan/Taxicab)
		// Stores distance from square to the board center squares (35, 36, 27, 28 or d4, e4, d5, e5) 
		static std::array<int, 64> centreManhattanDistance;
	};

} // namespace Chess
#endif // PRECOMPUTED_MOVE_DATA_H
