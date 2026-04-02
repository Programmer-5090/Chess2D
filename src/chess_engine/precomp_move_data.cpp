#include "precomp_move_data.h"
#include "pieces.h"
#include <algorithm>

namespace Chess {

	std::array<std::vector<uint8_t>, 64> PrecomputedMoveData::knightMoves{};
	std::array<std::vector<uint8_t>, 64> PrecomputedMoveData::kingMoves{};

	std::array<uint64_t, 64> PrecomputedMoveData::queenMovesBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::rookMovesBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::bishopMoveBitboards{};

	std::array<std::vector<int>, 64> PrecomputedMoveData::pawnAttacksWhite{};
	std::array<std::vector<int>, 64> PrecomputedMoveData::pawnAttacksBlack{};

	std::array<uint64_t, 64> PrecomputedMoveData::kingAttackBitboards{};
	std::array<uint64_t, 64> PrecomputedMoveData::knightAttackBitboards{};
	std::array<std::array<uint64_t, 64>, 2> PrecomputedMoveData::pawnAttackBitboards{};

	std::array<std::array<int, 64>, 64> PrecomputedMoveData::orthogonalDistance{};
	std::array<std::array<int, 64>, 64> PrecomputedMoveData::kingDistance{};

	std::array<int, 64> PrecomputedMoveData::centreManhattanDistance{};

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

	std::vector<uint8_t> PrecomputedMoveData::getKnightMoves(int square) {
		if (!isValidSquare(square)) return {};
		return knightMoves[square];
	}

	std::vector<uint8_t> PrecomputedMoveData::getKingMovesVector(int square) {
		if (!isValidSquare(square)) return {};
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

	uint64_t PrecomputedMoveData::getKingMoves(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return kingAttackBitboards[square];
	}

	uint64_t PrecomputedMoveData::getKnightAttacks(int square) {
		if (!isValidSquare(square)) return 0ULL;
		return knightAttackBitboards[square];
	}

	std::vector<int> PrecomputedMoveData::getPawnAttacksWhite(int square) {
		if (!isValidSquare(square)) return {};
		return pawnAttacksWhite[square];
	}

	std::vector<int> PrecomputedMoveData::getPawnAttacksBlack(int square) {
		if (!isValidSquare(square)) return {};
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

	int PrecomputedMoveData::getCentreManhattanDistance(int square) {
		if (!isValidSquare(square)) return -1;
		return centreManhattanDistance[square];
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

		int offset = toSquare - fromSquare;
		if (offset < -63 || offset > 63) {
			return 0;
		}

		int index = offset + 63;
		if (index >= 0 && index < 127) {
			return directionLookup[index];
		}

		return 0;
	}

	int PrecomputedMoveData::getDirection(int fromSquare, int toSquare) {
		int offset = getDirectionOffset(fromSquare, toSquare);
		if (offset == 0) return -1;

		int absOffset = std::abs(offset);
		for (int dir = 0; dir < 8; ++dir) {
			if (std::abs(dirOffsets[dir]) == absOffset) {
				return dir;
			}
		}
		return -1;
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

		for (int square = 0; square < 64; square++) {
			int y = square / 8;
			int x = square - y * 8;

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

			// Knight moves
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

			// King moves
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

			// Pawn attacks
			// White pawns move up (rank increases, +8 offset per rank)
			// Black pawns move down (rank decreases, -8 offset per rank)
			std::vector<int> pawnCapturesWhite;
			std::vector<int> pawnCapturesBlack;
			
			// White pawn attacks: moving up diagonally
			if (x > 0 && y < 7) {
				pawnCapturesWhite.push_back(square + 7);
				pawnAttackBitboards[COLOR_WHITE][square] |= 1ULL << (square + 7);
			}
			if (x < 7 && y < 7) {
				pawnCapturesWhite.push_back(square + 9);
				pawnAttackBitboards[COLOR_WHITE][square] |= 1ULL << (square + 9);
			}
			
			// Black pawn attacks: moving down diagonally
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

			// Rook moves (orthogonal: N, S, W, E)
			for (int directionIndex = 0; directionIndex < 4; directionIndex++) {
				int currentDirOffset = dirOffsets[directionIndex];
				for (int n = 0; n < squareEdges[square][directionIndex]; n++) {
					int targetSquare = square + currentDirOffset * (n + 1);
					rookMovesBitboards[square] |= 1ULL << targetSquare;
				}
			}

			// Bishop moves (diagonal: NW, SE, NE, SW)
			for (int directionIndex = 4; directionIndex < 8; directionIndex++) {
				int currentDirOffset = dirOffsets[directionIndex];
				for (int n = 0; n < squareEdges[square][directionIndex]; n++) {
					int targetSquare = square + currentDirOffset * (n + 1);
					bishopMoveBitboards[square] |= 1ULL << targetSquare;
				}
			}

			// Queen moves
			queenMovesBitboards[square] = rookMovesBitboards[square] | bishopMoveBitboards[square];
		}

		// Direction lookup
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

		// Distance lookups
		for (int squareA = 0; squareA < 64; squareA++) {
			int yA = squareA / 8;
			int xA = squareA - yA * 8;

			// Center Manhattan distance
			int fileDstFromCentre = std::max(3 - xA, xA - 4);
			int rankDstFromCentre = std::max(3 - yA, yA - 4);
			centreManhattanDistance[squareA] = fileDstFromCentre + rankDstFromCentre;

			for (int squareB = 0; squareB < 64; squareB++) {
				int yB = squareB / 8;
				int xB = squareB - yB * 8;

				int rankDistance = std::abs(yA - yB);
				int fileDistance = std::abs(xA - xB);

				orthogonalDistance[squareA][squareB] = fileDistance + rankDistance;
				kingDistance[squareA][squareB] = std::max(fileDistance, rankDistance);
			}
		}

		initialized = true;
	}
} // namespace Chess
