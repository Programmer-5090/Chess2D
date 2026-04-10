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
		static const std::vector<uint8_t>& getKnightMoves(int square);

		// Get all valid king moves from a square
		static const std::vector<uint8_t>& getKingMovesVector(int square);

		// Get rook move bitboard for a square
		static uint64_t getRookMoves(int square);

		// Get bishop move bitboard for a square
		static uint64_t getBishopMoves(int square);

		// Get queen move bitboard for a square
		static uint64_t getQueenMoves(int square);

		// Get precomputed relevant blocker mask for rook magic on a square
		static uint64_t getRookBlockerMask(int square);

		// Get precomputed relevant blocker mask for bishop magic on a square
		static uint64_t getBishopBlockerMask(int square);

		// Get precomputed shift used in rook magic indexing (64 - relevant bits)
		static int getRookMagicShift(int square);

		// Get precomputed shift used in bishop magic indexing (64 - relevant bits)
		static int getBishopMagicShift(int square);

		// Get king move bitboard for a square
		static uint64_t getKingMoves(int square);

		// Get knight attack bitboard for a square
		static uint64_t getKnightAttacks(int square);

		// Get pawn attack squares for white pawns
		static const std::vector<int>& getPawnAttacksWhite(int square);

		// Get pawn attack squares for black pawns
		static const std::vector<int>& getPawnAttacksBlack(int square);

		// Get pawn attack bitboard for a given color and square
		static uint64_t getPawnAttackBitboard(int color, int square);

		// Get orthogonal (Manhattan/Taxicab) distance between two squares
		static int getOrthogonalDistance(int squareA, int squareB);

		// Get king (Chebyshev) distance between two squares
		static int getKingDistance(int squareA, int squareB);

		// Get Manhattan distance from a square to the board center
		static int getCenterManhattanDistance(int square);

		// Get bitboard of squares strictly between two aligned squares, else 0
		static uint64_t getBetweenBitboard(int fromSquare, int toSquare);

		// Get bitboard of the aligned segment including endpoints, else 0
		static uint64_t getLineBitboard(int fromSquare, int toSquare);

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
		static bool initialized;

		static void initializeSquareAndAttackTables();
		static void initializeDirectionLookupTable();
		static void initializeDistanceTables();
		static void initializeLineAndBetweenTables();
		static void initializeSquareEdges(int square, int x, int y);
		static void initializeKnightData(int square, int x, int y);
		static void initializeKingData(int square, int x, int y);
		static void initializePawnData(int square, int x, int y);
		static void initializeSlidingData(int square);

		static const std::array<int, 8> allKnightJumps;
		static std::array<std::array<int, 8>, 64> squareEdges;

		static std::array<int, 127> directionLookup;

		static std::array<std::vector<uint8_t>, 64> knightMoves;
		static std::array<std::vector<uint8_t>, 64> kingMoves;

		static std::array<uint64_t, 64> queenMovesBitboards;
		static std::array<uint64_t, 64> rookMovesBitboards;
		static std::array<uint64_t, 64> bishopMoveBitboards;

		static std::array<uint64_t, 64> rookBlockerMasks;
		static std::array<uint64_t, 64> bishopBlockerMasks;

		static std::array<int, 64> rookMagicShifts;
		static std::array<int, 64> bishopMagicShifts;

		static const std::array<std::array<uint8_t, 2>, 2> pawnAttackDirections;

		static std::array<std::vector<int>, 64> pawnAttacksWhite;
		static std::array<std::vector<int>, 64> pawnAttacksBlack;

		static std::array<uint64_t, 64> kingAttackBitboards;
		static std::array<uint64_t, 64> knightAttackBitboards;
		static std::array<std::array<uint64_t, 64>, 2> pawnAttackBitboards;

		static std::array<std::array<int, 64>, 64> orthogonalDistance;
		static std::array<std::array<int, 64>, 64> kingDistance;
		static std::array<int, 64> centerManhattanDistance;
		static std::array<std::array<uint64_t, 64>, 64> betweenBitboards;
		static std::array<std::array<uint64_t, 64>, 64> lineBitboards;
	};

} // namespace Chess
#endif // PRECOMPUTED_MOVE_DATA_H
