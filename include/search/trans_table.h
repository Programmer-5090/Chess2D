#include <cstddef>
#include <numeric>
#include <cstdint>
#include <algorithm>
#include <limits>

#include "board_state.h"
#include "move.h"

namespace Chess
{
    static constexpr int EXACT = 0;
    static constexpr int LOWER = 1;
    static constexpr int UPPER = 2;

    struct TTEntry {
        uint64_t key = 0;
        int32_t score = 0;
        uint8_t depth = 0;
        uint8_t boundType = 0; // 0: exact, 1: lower, 2: upper
        Move move = Move::invalid();
        uint8_t age = 0;
    };

    class TranspositionTable {
    public:
        explicit TranspositionTable(int sizeMB = 50) {
            resize(sizeMB);
        }

        ~TranspositionTable() {
            delete[] table;
        }

        std::size_t getIndex(uint64_t key) const {
            return key % tableSize;
        }

        int lookupEval(int alpha, int beta, int depth, int plys, const BoardState& board) const {
            const TTEntry& entry = table[getIndex(board.getPosKey())];

            if (entry.key != board.getPosKey() || entry.depth < depth) {
                return lookupFailed;
            }

            const int score = fromTTScore(entry.score, plys);

            if (entry.boundType == EXACT) {
                return score;
            }

            if (entry.boundType == LOWER && score >= beta) {
                return score;
            }

            if (entry.boundType == UPPER && score <= alpha) {
                return score;
            }

            return lookupFailed;
        }

        void storeEval(int score, int depth, int plys, int boundType, const BoardState& board, Move move = Move::invalid()) {
            const std::size_t index = getIndex(board.getPosKey());
            TTEntry& entry = table[index];

            if (entry.key != board.getPosKey() || depth >= entry.depth) {
                entry.key = board.getPosKey();
                entry.score = toTTScore(score, plys);
                entry.depth = static_cast<uint8_t>(std::clamp(depth, 0, 255));
                entry.boundType = static_cast<uint8_t>(boundType);
                entry.move = move;
                entry.age = currentAge;
            }
        }

        Move lookupMove(const BoardState& board) const {
            const TTEntry& entry = table[getIndex(board.getPosKey())];
            if (entry.key == board.getPosKey()) {
                return entry.move;
            }
            return Move::invalid();
        }

        void clear() {
            for (std::size_t i = 0; i < tableSize; ++i) {
                table[i] = TTEntry{};
            }
            currentAge = 0;
        }

        void newSearch() {
            ++currentAge;
        }

        void resize(int sizeMB) {
            const std::size_t requestedMB = sizeMB > 0 ? static_cast<std::size_t>(sizeMB) : 1;
            const std::size_t bytes = requestedMB * 1024ULL * 1024ULL;
            const std::size_t entries = std::max<std::size_t>(1, bytes / sizeof(TTEntry));

            delete[] table;
            tableSize = entries;
            table = new TTEntry[tableSize]{};
            currentAge = 0;
        }

        int getSize() { return static_cast<int>(tableSize); }

        static constexpr int getLookupFailedValue() {
            return std::numeric_limits<int>::min();
        }

    private:
        static int toTTScore(int score, int plys) {
            if (score > MATE_SCORE - 1000) return score + plys;
            if (score < -MATE_SCORE + 1000) return score - plys;
            return score;
        }

        static int fromTTScore(int score, int plys) {
            if (score > MATE_SCORE - 1000) return score - plys;
            if (score < -MATE_SCORE + 1000) return score + plys;
            return score;
        }
        static constexpr int MATE_SCORE = 30000;

        std::size_t tableSize = 1;
        TTEntry* table = nullptr;
        uint8_t currentAge = 0;

        static constexpr int lookupFailed = std::numeric_limits<int>::min();
    };
}