#include "hash_keys.h"

namespace Chess {

    ZobristKeys g_zobristKeys;

    uint64_t ZobristKeys::random64() {
        return (static_cast<uint64_t>(std::rand()) << 48) ^
               (static_cast<uint64_t>(std::rand()) << 32) ^
               (static_cast<uint64_t>(std::rand()) << 16) ^
               (static_cast<uint64_t>(std::rand()));
    }

    void ZobristKeys::init() {
        static bool seeded = false;
        if (!seeded) {
            std::srand(static_cast<unsigned>(std::random_device{}()));
            seeded = true;
        }

        for (int i = 0; i < 13; ++i) {
            for (int j = 0; j < ZobristKeys::BOARD_SIZE; ++j) {
                pieceKeys[i][j] = random64();
            }
        }

        sideKey = random64();

        for (int i = 0; i < 16; ++i) {
            castleKeys[i] = random64();
        }

        initialized = true;
    }

    bool ZobristKeys::isInitialized() const {
        return initialized;
    }

    uint64_t ZobristKeys::getPieceKey(int pieceIndex, int square) const {
        return pieceKeys[pieceIndex][square];
    }

    uint64_t ZobristKeys::getSideKey() const {
        return sideKey;
    }

    uint64_t ZobristKeys::getCastleKey(int rights) const {
        return castleKeys[rights];
    }

    const std::array<std::array<uint64_t, ZobristKeys::BOARD_SIZE>, 13>& ZobristKeys::getPieceKeys() const {
        return pieceKeys;
    }

}  // namespace Chess
