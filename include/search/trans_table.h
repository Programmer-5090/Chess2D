#include <cstddef>
#include <numeric>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <array>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <vector>

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
            const std::size_t index = getIndex(board.getPosKey());
            std::shared_lock<std::shared_mutex> lock(ttStripeMutexes[getStripeIndex(index)]);
            const TTEntry& entry = table[index];

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
            std::unique_lock<std::shared_mutex> lock(ttStripeMutexes[getStripeIndex(index)]);
            TTEntry& entry = table[index];

            if (entry.key != board.getPosKey() || depth >= entry.depth) {
                entry.key = board.getPosKey();
                entry.score = toTTScore(score, plys);
                entry.depth = static_cast<uint8_t>(std::clamp(depth, 0, 255));
                entry.boundType = static_cast<uint8_t>(boundType);
                entry.move = move;
                entry.age = currentAge.load(std::memory_order_relaxed);
            }
        }

        Move lookupMove(const BoardState& board) const {
            const std::size_t index = getIndex(board.getPosKey());
            std::shared_lock<std::shared_mutex> lock(ttStripeMutexes[getStripeIndex(index)]);
            const TTEntry& entry = table[index];
            if (entry.key == board.getPosKey()) {
                return entry.move;
            }
            return Move::invalid();
        }

        void clear() {
            auto stripeLocks = lockAllStripes();
            for (std::size_t i = 0; i < tableSize; ++i) {
                table[i] = TTEntry{};
            }
            currentAge.store(0, std::memory_order_relaxed);
        }

        void newSearch() {
            currentAge.fetch_add(1, std::memory_order_relaxed);
        }

        void resize(int sizeMB) {
            auto stripeLocks = lockAllStripes();
            const std::size_t requestedMB = sizeMB > 0 ? static_cast<std::size_t>(sizeMB) : 1;
            const std::size_t bytes = requestedMB * 1024ULL * 1024ULL;
            const std::size_t entries = std::max<std::size_t>(1, bytes / sizeof(TTEntry));

            delete[] table;
            tableSize = entries;
            table = new TTEntry[tableSize]{};
            currentAge.store(0, std::memory_order_relaxed);
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
        

        static constexpr std::size_t getStripeIndex(std::size_t tableIndex) {
            return tableIndex & (TT_LOCK_STRIPES - 1);
        }

        std::vector<std::unique_lock<std::shared_mutex>> lockAllStripes() const {
            std::vector<std::unique_lock<std::shared_mutex>> locks;
            locks.reserve(TT_LOCK_STRIPES);
            for (auto& mtx : ttStripeMutexes) {
                locks.emplace_back(mtx);
            }
            return locks;
        }

        static constexpr std::size_t TT_LOCK_STRIPES = 2048;
        static_assert((TT_LOCK_STRIPES& (TT_LOCK_STRIPES - 1)) == 0, "TT_LOCK_STRIPES must be power of two");

        static constexpr int MATE_SCORE = 30000;

        std::size_t tableSize = 1;
        TTEntry* table = nullptr;
        std::atomic<uint8_t> currentAge{ 0 };
        mutable std::array<std::shared_mutex, TT_LOCK_STRIPES> ttStripeMutexes{};

        static constexpr int lookupFailed = std::numeric_limits<int>::min();
    };
}  // namespace Chess