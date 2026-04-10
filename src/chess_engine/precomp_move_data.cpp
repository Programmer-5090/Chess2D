#include "precomp_move_data.h"
#include "pieces.h"
#include <algorithm>
#include <bitboard_util.h>

namespace Chess {

	std::array<std::vector<uint8_t>, 64> PrecomputedMoveData::knightMoves{};
	std::array<std::vector<uint8_t>, 64> PrecomputedMoveData::kingMoves{};

	std::array<uint64_t, 64> PrecomputedMoveData::queenMovesBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::rookMovesBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::bishopMoveBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::rookBlockerMasks{};
	std::array<uint64_t, 64> PrecomputedMoveData::bishopBlockerMasks{};
	std::array<int, 64> PrecomputedMoveData::rookMagicShifts{};
	std::array<int, 64> PrecomputedMoveData::bishopMagicShifts{};

	std::array<std::vector<int>, 64> PrecomputedMoveData::pawnAttacksWhite{};
	std::array<std::vector<int>, 64> PrecomputedMoveData::pawnAttacksBlack{};

	std::array<uint64_t, 64> PrecomputedMoveData::kingAttackBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::knightAttackBitboards{};
	std::array<std::array<uint64_t, 64>, 2> PrecomputedMoveData::pawnAttackBitboards{};

	std::array<std::array<int, 64>, 64> PrecomputedMoveData::orthogonalDistance{};
	std::array<std::array<int, 64>, 64> PrecomputedMoveData::kingDistance{};

	std::array<int, 64> PrecomputedMoveData::centerManhattanDistance{};
	std::array<std::array<uint64_t, 64>, 64> PrecomputedMoveData::betweenBitboards{};
	std::array<std::array<uint64_t, 64>, 64> PrecomputedMoveData::lineBitboards{};

	// Private static members
	bool PrecomputedMoveData::initialized = false;

	const std::array<int, 8> PrecomputedMoveData::dirOffsets = { 8, -8, -1, 1, 7, -7, 9, -9 };
	const std::array<int, 8> PrecomputedMoveData::allKnightJumps = { 15, 17, -17, -15, 10, -6, 6, -10 };
	std::array<std::array<int, 8>, 64> PrecomputedMoveData::squareEdges{};

	std::array<int, 127> PrecomputedMoveData::directionLookup{};

	const std::array<std::array<uint8_t, 2>, 2> PrecomputedMoveData::pawnAttackDirections = {
		std::array<uint8_t, 2>{ 4, 6 },  // White: NW, NE
		std::array<uint8_t, 2>{ 7, 5 }   // Black: SW, SE
	};

	const std::vector<uint8_t>& PrecomputedMoveData::getKnightMoves(int square) {
		static const std::vector<uint8_t> empty;
		if (!isValidSquare(square)) return empty;
		return knightMoves[square];
	}

	const std::vector<uint8_t>& PrecomputedMoveData::getKingMovesVector(int square) {
		static const std::vector<uint8_t> empty;
		if (!isValidSquare(square)) return empty;
		return kingMoves[square];
	}

	uint64_t PrecomputedMoveData::getRookMoves(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return rookMovesBitboards[square];
	}

	uint64_t PrecomputedMoveData::getBishopMoves(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return bishopMoveBitboards[square];
	}

	uint64_t PrecomputedMoveData::getQueenMoves(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return queenMovesBitboards[square];
	}

	uint64_t PrecomputedMoveData::getRookBlockerMask(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return rookBlockerMasks[square];
	}

	uint64_t PrecomputedMoveData::getBishopBlockerMask(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return bishopBlockerMasks[square];
	}

	int PrecomputedMoveData::getRookMagicShift(int square) {
		if (!isValidSquare(square)) return 64;
		return rookMagicShifts[square];
	}

	int PrecomputedMoveData::getBishopMagicShift(int square) {
		if (!isValidSquare(square)) return 64;
		return bishopMagicShifts[square];
	}

	uint64_t PrecomputedMoveData::getKingMoves(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return kingAttackBitboards[square];
	}

	uint64_t PrecomputedMoveData::getKnightAttacks(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return knightAttackBitboards[square];
	}

	const std::vector<int>& PrecomputedMoveData::getPawnAttacksWhite(int square) {
		static const std::vector<int> empty;
		if (!isValidSquare(square)) return empty;
		return pawnAttacksWhite[square];
	}

	const std::vector<int>& PrecomputedMoveData::getPawnAttacksBlack(int square) {
		static const std::vector<int> empty;
		if (!isValidSquare(square)) return empty;
		return pawnAttacksBlack[square];
	}

	uint64_t PrecomputedMoveData::getPawnAttackBitboard(int color, int square) {
		if (!isValidSquare(square) || (color != 0 && color != 1)) return 0ULL;
		return pawnAttackBitboards[color][square];
	}

	int PrecomputedMoveData::getOrthogonalDistance(int squareA, int squareB) {
		if (!isValidSquare(squareA) || !isValidSquare(squareB)) return -1;
		return orthogonalDistance[squareA][squareB];
	}

	int PrecomputedMoveData::getKingDistance(int squareA, int squareB) {
		if (!isValidSquare(squareA) || !isValidSquare(squareB)) return -1;
		return kingDistance[squareA][squareB];
	}

	int PrecomputedMoveData::getCenterManhattanDistance(int square) {
		if (!isValidSquare(square)) return -1;
		return centerManhattanDistance[square];
	}

	uint64_t PrecomputedMoveData::getBetweenBitboard(int fromSquare, int toSquare) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare)) return 0ULL;
		return betweenBitboards[fromSquare][toSquare];
	}

	uint64_t PrecomputedMoveData::getLineBitboard(int fromSquare, int toSquare) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare)) return 0ULL;
		return lineBitboards[fromSquare][toSquare];
	}

	bool PrecomputedMoveData::isDirectionalMove(int fromSquare, int toSquare, int direction) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare) || direction < 0 || direction >= 8) {
			return false;
		}

		int diff = toSquare - fromSquare;
		int dirOffset = dirOffsets[direction];
		if (dirOffset == 0) return false;
		if (diff == 0) return false;
		if (dirOffset != 0 && diff % dirOffset != 0) return false;

		int fromFile = fromSquare % 8;
		int toFile = toSquare % 8;
		int fromRank = fromSquare / 8;
		int toRank = toSquare / 8;

		if (direction == WEST || direction == EAST) {
			return fromRank == toRank;
		}
		if (direction == NORTH || direction == SOUTH) {
			return fromFile == toFile;
		}
		if (direction >= NORTH_WEST) {
			int fileDiff = std::abs(toFile - fromFile);
			int rankDiff = std::abs(toRank - fromRank);
			return fileDiff == rankDiff && fileDiff > 0;
		}

		return false;
	}

	int PrecomputedMoveData::getDirectionOffset(int fromSquare, int toSquare) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare) || fromSquare == toSquare) {
			return 0;
		}

		const int fromFile = fromSquare % 8;
		const int fromRank = fromSquare / 8;
		const int toFile = toSquare % 8;
		const int toRank = toSquare / 8;

		const int fileDiff = toFile - fromFile;
		const int rankDiff = toRank - fromRank;

		if (fileDiff == 0) {
			return (rankDiff > 0) ? 8 : -8;
		}

		if (rankDiff == 0) {
			return (fileDiff > 0) ? 1 : -1;
		}

		if (std::abs(fileDiff) == std::abs(rankDiff)) {
			if (fileDiff > 0 && rankDiff > 0) return 9;
			if (fileDiff < 0 && rankDiff > 0) return 7;
			if (fileDiff > 0 && rankDiff < 0) return -7;
			return -9;
		}

		return 0;
	}

	int PrecomputedMoveData::getDirection(int fromSquare, int toSquare) {
		int offset = getDirectionOffset(fromSquare, toSquare);
		if (offset == 0) return -1;

		switch (offset) {
			case  8: return NORTH;
			case -8: return SOUTH;
			case -1: return WEST;
			case  1: return EAST;
			case  7: return NORTH_WEST;
			case -7: return SOUTH_EAST;
			case  9: return NORTH_EAST;
			case -9: return SOUTH_WEST;
			default: return -1;
		}
	}

	bool PrecomputedMoveData::isValidSquare(int square) {
		return square >= 0 && square < 64;
	}

	bool PrecomputedMoveData::isValidKnightMove(int fromSquare, int toSquare) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare)) return false;
		if (fromSquare == toSquare) return false;

		const auto& moves = knightMoves[fromSquare];
		return std::find(moves.begin(), moves.end(), static_cast<uint8_t>(toSquare)) != moves.end();
	}

	bool PrecomputedMoveData::isValidKingMove(int fromSquare, int toSquare) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare)) return false;
		if (fromSquare == toSquare) return false;

		const auto& moves = kingMoves[fromSquare];
		return std::find(moves.begin(), moves.end(), static_cast<uint8_t>(toSquare)) != moves.end();
	}

	bool PrecomputedMoveData::isValidPawnAttack(int fromSquare, int toSquare, int color) {
		if (!isValidSquare(fromSquare) || !isValidSquare(toSquare) || (color != 0 && color != 1)) {
			return false;
		}
		if (fromSquare == toSquare) return false;

		const auto& attacks = (color == 0) ? pawnAttacksWhite[fromSquare] : pawnAttacksBlack[fromSquare];
		return std::find(attacks.begin(), attacks.end(), toSquare) != attacks.end();
	}

	void PrecomputedMoveData::initialize() {
		if (initialized) return;

		initializeSquareAndAttackTables();
		initializeDirectionLookupTable();
		initializeDistanceTables();
		initializeLineAndBetweenTables();

		initialized = true;
	}

	void PrecomputedMoveData::initializeLineAndBetweenTables() {
		for (int from = 0; from < 64; ++from) {
			for (int to = 0; to < 64; ++to) {
				betweenBitboards[from][to] = 0ULL;
				lineBitboards[from][to] = 0ULL;

				if (from == to) {
					lineBitboards[from][to] = (1ULL << from);
					continue;
				}

				const int dir = getDirectionOffset(from, to);
				if (dir == 0) {
					continue;
				}

				uint64_t between = 0ULL;
				int sq = from + dir;
				while (sq != to) {
					between |= (1ULL << sq);
					sq += dir;
				}

				betweenBitboards[from][to] = between;
				lineBitboards[from][to] = between | (1ULL << from) | (1ULL << to);
			}
		}
	}

	void PrecomputedMoveData::initializeSquareAndAttackTables() {
		for (int square = 0; square < 64; square++) {
			int y = square / 8;
			int x = square - y * 8;

			initializeSquareEdges(square, x, y);
			initializeKnightData(square, x, y);
			initializeKingData(square, x, y);
			initializePawnData(square, x, y);
			initializeSlidingData(square);
		}
	}

	void PrecomputedMoveData::initializeSquareEdges(int square, int x, int y) {
			int north = 7 - y;      // Squares until top edge
			int south = y;          // Squares until bottom edge
			int west = x;           // Squares until left edge
			int east = 7 - x;       // Squares until right edge

			squareEdges[square][NORTH] = north;
			squareEdges[square][SOUTH] = south;
			squareEdges[square][WEST] = west;
			squareEdges[square][EAST] = east;
			squareEdges[square][NORTH_WEST] = std::min(north, west);
			squareEdges[square][SOUTH_EAST] = std::min(south, east);
			squareEdges[square][NORTH_EAST] = std::min(north, east);
			squareEdges[square][SOUTH_WEST] = std::min(south, west);
	}

	void PrecomputedMoveData::initializeKnightData(int square, int x, int y) {
		std::vector<uint8_t> legalKnightJumps;
		for (const auto& jump : allKnightJumps) {
			int jumpSquare = square + jump;
			if (jumpSquare >= 0 && jumpSquare < 64) {
				int knightSquareY = jumpSquare / 8;
				int knightSquareX = jumpSquare - knightSquareY * 8;
				int maxCoordMoveDst = std::max(std::abs(x - knightSquareX), std::abs(y - knightSquareY));
				if (maxCoordMoveDst == 2) {
					legalKnightJumps.push_back(static_cast<uint8_t>(jumpSquare));
					knightAttackBitboards[square] |= 1ULL << jumpSquare;
				}
			}
		}
		knightMoves[square] = legalKnightJumps;
	}

	void PrecomputedMoveData::initializeKingData(int square, int x, int y) {
		std::vector<uint8_t> legalKingMoves;
		for (const auto& move : dirOffsets) {
			int moveSquare = square + move;
			if (moveSquare >= 0 && moveSquare < 64) {
				int kingSquareY = moveSquare / 8;
				int kingSquareX = moveSquare - kingSquareY * 8;
				int maxCoordMoveDst = std::max(std::abs(x - kingSquareX), std::abs(y - kingSquareY));
				if (maxCoordMoveDst == 1) {
					legalKingMoves.push_back(static_cast<uint8_t>(moveSquare));
					kingAttackBitboards[square] |= 1ULL << moveSquare;
				}
			}
		}
		kingMoves[square] = legalKingMoves;
	}

	void PrecomputedMoveData::initializePawnData(int square, int x, int y) {
		std::vector<int> pawnCapturesWhite;
		std::vector<int> pawnCapturesBlack;

		if (x > 0 && y < 7) {
			pawnCapturesWhite.push_back(square + 7);
			pawnAttackBitboards[COLOR_WHITE][square] |= 1ULL << (square + 7);
		}
		if (x < 7 && y < 7) {
			pawnCapturesWhite.push_back(square + 9);
			pawnAttackBitboards[COLOR_WHITE][square] |= 1ULL << (square + 9);
		}

		if (x > 0 && y > 0) {
			pawnCapturesBlack.push_back(square - 9);
			pawnAttackBitboards[COLOR_BLACK][square] |= 1ULL << (square - 9);
		}
		if (x < 7 && y > 0) {
			pawnCapturesBlack.push_back(square - 7);
			pawnAttackBitboards[COLOR_BLACK][square] |= 1ULL << (square - 7);
		}

		pawnAttacksWhite[square] = pawnCapturesWhite;
		pawnAttacksBlack[square] = pawnCapturesBlack;
	}

	void PrecomputedMoveData::initializeSlidingData(int square) {
		for (int directionIndex = 0; directionIndex < 4; directionIndex++) {
			int currentDirOffset = dirOffsets[directionIndex];
			for (int n = 0; n < squareEdges[square][directionIndex]; n++) {
				int targetSquare = square + currentDirOffset * (n + 1);
				rookMovesBitboards[square] |= 1ULL << targetSquare;
			}
			for (int n = 1; n < squareEdges[square][directionIndex]; n++) {
				int blockerSquare = square + currentDirOffset * n;
				rookBlockerMasks[square] |= 1ULL << blockerSquare;
			}
		}

		for (int directionIndex = 4; directionIndex < 8; directionIndex++) {
			int currentDirOffset = dirOffsets[directionIndex];
			for (int n = 0; n < squareEdges[square][directionIndex]; n++) {
				int targetSquare = square + currentDirOffset * (n + 1);
				bishopMoveBitboards[square] |= 1ULL << targetSquare;
			}
			for (int n = 1; n < squareEdges[square][directionIndex]; n++) {
				int blockerSquare = square + currentDirOffset * n;
				bishopBlockerMasks[square] |= 1ULL << blockerSquare;
			}
		}

		queenMovesBitboards[square] = rookMovesBitboards[square] | bishopMoveBitboards[square];
		rookMagicShifts[square] = 64 - (popCount(rookBlockerMasks[square]));
		bishopMagicShifts[square] = 64 - (popCount(bishopBlockerMasks[square]));
	}

	void PrecomputedMoveData::initializeDirectionLookupTable() {
		for (int i = 0; i < 127; i++) {
			int offset = i - 63;
			int absOffset = std::abs(offset);
			int absDir = 1;
			if (absOffset % 9 == 0) {
				absDir = 9;
			}
			else if (absOffset % 8 == 0) {
				absDir = 8;
			}
			else if (absOffset % 7 == 0) {
				absDir = 7;
			}

			directionLookup[i] = absDir * (offset > 0 ? 1 : -1);
		}
	}

	void PrecomputedMoveData::initializeDistanceTables() {
		for (int squareA = 0; squareA < 64; squareA++) {
			int yA = squareA / 8;
			int xA = squareA - yA * 8;

			// Center Manhattan distance
			int fileDstFromCenter = std::max(3 - xA, xA - 4);
			int rankDstFromCenter = std::max(3 - yA, yA - 4);
			centerManhattanDistance[squareA] = fileDstFromCenter + rankDstFromCenter;

			for (int squareB = 0; squareB < 64; squareB++) {
				int yB = squareB / 8;
				int xB = squareB - yB * 8;

				int rankDistance = std::abs(yA - yB);
				int fileDistance = std::abs(xA - xB);

				orthogonalDistance[squareA][squareB] = fileDistance + rankDistance;
				kingDistance[squareA][squareB] = std::max(fileDistance, rankDistance);
			}
		}
	}
}  // namespace Chess