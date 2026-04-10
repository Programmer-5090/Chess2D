#include "magics.h"
#include "precomp_move_data.h"
#include "bitboard_util.h"

namespace Chess {
	uint64_t lookupTable[lookupTableSize]{};
	MagicEntry bishopMagics[64]{};
	MagicEntry rookMagics[64]{};

	namespace {
		uint64_t buildRookAttacks(int square, uint64_t blockers) {
			uint64_t attacks = 0ULL;
			const int rank = square / 8;
			const int file = square % 8;

			for (int r = rank + 1; r < 8; ++r) {
				const int sq = r * 8 + file;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int r = rank - 1; r >= 0; --r) {
				const int sq = r * 8 + file;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int f = file + 1; f < 8; ++f) {
				const int sq = rank * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int f = file - 1; f >= 0; --f) {
				const int sq = rank * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}

			return attacks;
		}

		uint64_t buildBishopAttacks(int square, uint64_t blockers) {
			uint64_t attacks = 0ULL;
			const int rank = square / 8;
			const int file = square % 8;

			for (int r = rank + 1, f = file + 1; r < 8 && f < 8; ++r, ++f) {
				const int sq = r * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int r = rank + 1, f = file - 1; r < 8 && f >= 0; ++r, --f) {
				const int sq = r * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int r = rank - 1, f = file + 1; r >= 0 && f < 8; --r, ++f) {
				const int sq = r * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}
			for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; --r, --f) {
				const int sq = r * 8 + f;
				attacks |= (1ULL << sq);
				if (blockers & (1ULL << sq)) break;
			}

			return attacks;
		}

		uint64_t occupancyFromIndex(uint64_t mask, int index) {
			uint64_t occupancy = 0ULL;
			int bit = 0;
			while (mask) {
				const int sq = popLSB(mask);
				if (index & (1 << bit)) {
					occupancy |= (1ULL << sq);
				}
				++bit;
			}
			return occupancy;
		}
	}

	void initialize_magics() {
		static bool initialized = false;
		if (initialized) return;

		Chess::PrecomputedMoveData::initialize();

		for (int sq = 0; sq < 64; ++sq) {
			bishopMagics[sq].ptr = &lookupTable[bishopMagicsRaw[sq].position];
			bishopMagics[sq].magic = bishopMagicsRaw[sq].factor;
			bishopMagics[sq].mask = PrecomputedMoveData::getBishopBlockerMask(sq);
			bishopMagics[sq].shift = 55; // 64 - 9

			rookMagics[sq].ptr = &lookupTable[rookMagicsRaw[sq].position];
			rookMagics[sq].magic = rookMagicsRaw[sq].factor;
			rookMagics[sq].mask = PrecomputedMoveData::getRookBlockerMask(sq);
			rookMagics[sq].shift = 52; // 64 - 12

			const int bishopBits = 64 - bishopMagics[sq].shift;
			const int rookBits = 64 - rookMagics[sq].shift;

			for (int idx = 0; idx < (1 << bishopBits); ++idx) {
				const uint64_t blockers = occupancyFromIndex(bishopMagics[sq].mask, idx);
				const uint64_t blackBlockers = blockers | ~bishopMagics[sq].mask;
				const uint64_t tableIndex = (blackBlockers * bishopMagics[sq].magic) >> bishopMagics[sq].shift;
				bishopMagics[sq].ptr[tableIndex] = buildBishopAttacks(sq, blockers);
			}

			for (int idx = 0; idx < (1 << rookBits); ++idx) {
				const uint64_t blockers = occupancyFromIndex(rookMagics[sq].mask, idx);
				const uint64_t blackBlockers = blockers | ~rookMagics[sq].mask;
				const uint64_t tableIndex = (blackBlockers * rookMagics[sq].magic) >> rookMagics[sq].shift;
				rookMagics[sq].ptr[tableIndex] = buildRookAttacks(sq, blockers);
			}
		}

		initialized = true;
	}
}  // namespace Chess
