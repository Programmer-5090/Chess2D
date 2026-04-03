#ifndef SEARCH_H
#define SEARCH_H

#include <vector>
#include <chrono>
#include <future>

#include "thread_pool.h"
#include "move_order_util.h"
#include "trans_table.h"
#include "evaluation.h"
#include "move_generator.h"
#include "board_state.h"
#include "logger.h"

namespace Chess {

	struct SearchSettings {
		int depth;
		bool useIterativeDeepening;
		bool useThreading;
		int searchTime = 1000; // in ms
		bool endlessSearch;
		bool abortSearch;
	};

	class Search {
	public:
		static SearchSettings DefaultSettings() {
			SearchSettings s{};
			s.depth = 6;
			s.useIterativeDeepening = true;
			s.useThreading = false;
			s.searchTime = 1000;
			s.endlessSearch = false;
			s.abortSearch = false;
			return s;
		}

		Search(BoardState& boardRef, int ttSizeMB = 100, SearchSettings settings = DefaultSettings())
			: board(boardRef), evaluation(boardRef), TT_Table(ttSizeMB), settings(settings) {
			gen.init();
		}

		int runSearch(int depth) {
			searchStart = std::chrono::steady_clock::now();
			bestMove = Move::invalid();
			TT_Table.newSearch();
			MoveOrderUtil::clearHeuristics();

			int targetDepth = depth > 0 ? depth : settings.depth;
			if (targetDepth <= 0) targetDepth = 1;

			int bestScore = 0;
			if (settings.useIterativeDeepening || settings.endlessSearch) {
				for (int currentDepth = 1; !shouldStop(); ++currentDepth) {
					if (!settings.endlessSearch && currentDepth > targetDepth) break;
					bestScore = searchRoot(currentDepth);
				}
			} else {
				bestScore = searchRoot(targetDepth);
			}

			return bestScore;
		}

		Move getBestMove() const {
			return bestMove;
		}

		int NegaMax(int alpha, int beta, int depth, int ply) {
			if (shouldStop()) {
				return evaluation.Evaluate();
			}

			if (depth <= 0) {
				return QuiescenceSearch(alpha, beta, ply);
			}

			const int ttScore = TT_Table.lookupEval(alpha, beta, depth, ply, board);
			if (ttScore != TranspositionTable::getLookupFailedValue()) {
				return ttScore;
			}

			gen.generateLegalMoves(board, true);
			std::vector<Move> moves;
			moves.reserve(static_cast<std::size_t>(gen.getLegalMoveCount()));
			for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
				moves.push_back(gen.moveList[i]);
			}

			if (moves.empty()) {
				if (gen.getInCheck()) {
					return -CHECKMATE_VALUE + ply;
				}
				return DRAW_VALUE;
			}

			const Move ttMove = TT_Table.lookupMove(board);
			std::vector<Move> orderedMoves;
			MoveOrderUtil::orderMoves(board, moves, ply, ttMove, orderedMoves);

			const int originalAlpha = alpha;
			Move localBestMove = Move::invalid();

			for (const Move& move : orderedMoves) {
				board.makeMove(move);
				const int score = -NegaMax(-beta, -alpha, depth - 1, ply + 1);
				board.unmakeMove();

				if (score >= beta) {
					if (!MoveOrderUtil::isCaptureMove(board, move)) {
						MoveOrderUtil::updateKiller(move, ply);
						MoveOrderUtil::updateHistory(move, board.getSide(), depth);
					}
					TT_Table.storeEval(score, depth, ply, LOWER, board, move); // LOWER
					return beta;
				}

				if (score > alpha) {
					alpha = score;
					localBestMove = move;
					if (ply == 0) {
						bestMove = move;
					}
				}
			}

			int boundType = UPPER;
			if (alpha > originalAlpha) {
				boundType = EXACT;
			}
			TT_Table.storeEval(alpha, depth, ply, boundType, board, localBestMove);
			return alpha;
		}

		int QuiescenceSearch(int alpha, int beta, int ply) {
			if (shouldStop()) {
				return evaluation.Evaluate();
			}

			gen.generateLegalMoves(board, true);
			const bool inCheck = gen.getInCheck();

			if (!inCheck) {
				const int standPat = evaluation.Evaluate();
				if (standPat >= beta) {
					return beta;
				}
				if (standPat > alpha) {
					alpha = standPat;
				}
			}

			std::vector<Move> moves;
			moves.reserve(static_cast<std::size_t>(gen.getLegalMoveCount()));
			for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
				const Move move = gen.moveList[i];
				if (inCheck || MoveOrderUtil::isCaptureMove(board, move)) {
					moves.push_back(move);
				}
			}

			if (moves.empty()) {
				if (inCheck) {
					return -CHECKMATE_VALUE + ply;
				}
				return alpha;
			}

			std::vector<Move> orderedMoves;
			MoveOrderUtil::orderMoves(board, moves, orderedMoves);

			for (const Move& move : orderedMoves) {
				board.makeMove(move);
				const int score = -QuiescenceSearch(-beta, -alpha, ply + 1);
				board.unmakeMove();

				if (score >= beta) {
					return beta;
				}

				if (score > alpha) {
					alpha = score;
				}
			}

			return alpha;
		}

	private:
		static constexpr int REVERSE_MOVE_PENALTY = 15;
		static constexpr int MAX_ROOT_THREADS = 4;
		static constexpr int MIN_THREADED_ROOT_DEPTH = 5;
		static constexpr int MIN_THREADED_ROOT_MOVES = 6;

		bool isReverseOfLastMove(const Move& move) const {
			if (!board.hasMovesToUndo()) return false;
			const Move last = board.getLastMove();
			if (!last.isValid()) return false;
			return move.startSquare() == last.targetSquare() && move.targetSquare() == last.startSquare();
		}

		int searchRoot(int depth) {
			gen.generateLegalMoves(board, true);
			std::vector<Move> moves;
			moves.reserve(static_cast<std::size_t>(gen.getLegalMoveCount()));
			for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
				moves.push_back(gen.moveList[i]);
			}

			if (moves.empty()) {
				if (gen.getInCheck()) return -CHECKMATE_VALUE;
				return DRAW_VALUE;
			}

			const Move ttMove = TT_Table.lookupMove(board);
			std::vector<Move> orderedMoves;
			MoveOrderUtil::orderMoves(board, moves, 0, ttMove, orderedMoves);

			int alpha = N_INFINITY;
			const int beta = P_INFINITY;
			Move localBestMove = Move::invalid();

			const int poolWorkers = static_cast<int>(threadPool.getThreadCount() > 0 ? threadPool.getThreadCount() - 1 : 0);
			int allowedWorkers = poolWorkers;
			if (allowedWorkers < 0) allowedWorkers = 0;
			if (allowedWorkers > 4) allowedWorkers = 4;

			if (settings.useThreading
				&& depth >= 5
				&& static_cast<int>(orderedMoves.size()) >= 6
				&& allowedWorkers > 0) {
				// Search first move synchronously to get an initial alpha for better move quality.
				const Move firstMove = orderedMoves.front();
				board.makeMove(firstMove);
				alpha = -NegaMax(-beta, -alpha, depth - 1, 1);
				board.unmakeMove();
				localBestMove = firstMove;

				std::vector<std::future<std::pair<int, Move>>> futures;
				int maxWorkers = static_cast<int>(orderedMoves.size() > 1 ? orderedMoves.size() - 1 : 0);
				if (maxWorkers > allowedWorkers) maxWorkers = allowedWorkers;
				futures.reserve(static_cast<std::size_t>(maxWorkers));

				for (std::size_t i = 1; i < orderedMoves.size() && static_cast<int>(futures.size()) < maxWorkers; ++i) {
					const Move move = orderedMoves[i];
					if (shouldStop()) break;
					BoardState boardCopy = board;
					boardCopy.makeMove(move);

					SearchSettings childSettings = settings;
					childSettings.useThreading = false;
					childSettings.useIterativeDeepening = false;
					childSettings.endlessSearch = false;
					childSettings.abortSearch = false;

					futures.push_back(threadPool.enqueue([boardCopy, move, depth, childSettings, start = searchStart, this]() mutable {
						Search child(boardCopy, TT_SizeMB, childSettings);
						child.searchStart = start;
						const int score = -child.NegaMax(N_INFINITY, P_INFINITY, depth - 1, 1);
						return std::make_pair(score, move);
					}));
				}

				// Search any remaining root moves sequentially (after limited worker fan-out)
				for (std::size_t i = 1 + static_cast<std::size_t>(maxWorkers); i < orderedMoves.size(); ++i) {
					if (shouldStop()) break;
					const Move move = orderedMoves[i];
					board.makeMove(move);
					const int score = -NegaMax(-beta, -alpha, depth - 1, 1);
					board.unmakeMove();

					int candidateScore = score;
					if (isReverseOfLastMove(move) && !MoveOrderUtil::isCaptureMove(board, move)) {
						candidateScore -= REVERSE_MOVE_PENALTY;
					}

					if (candidateScore > alpha) {
						alpha = candidateScore;
						localBestMove = move;
					} else if (candidateScore == alpha && localBestMove.isValid()) {
						if (isReverseOfLastMove(localBestMove) && !isReverseOfLastMove(move)) {
							localBestMove = move;
						}
					}
				}

				for (auto& f : futures) {
					const auto [score, move] = f.get();
					int candidateScore = score;
					if (isReverseOfLastMove(move) && !MoveOrderUtil::isCaptureMove(board, move)) {
						candidateScore -= REVERSE_MOVE_PENALTY;
					}

					if (candidateScore > alpha) {
						alpha = candidateScore;
						localBestMove = move;
					} else if (candidateScore == alpha && localBestMove.isValid()) {
						if (isReverseOfLastMove(localBestMove) && !isReverseOfLastMove(move)) {
							localBestMove = move;
						}
					}
				}
			} else {
				for (const Move& move : orderedMoves) {
					if (shouldStop()) break;
					board.makeMove(move);
					const int score = -NegaMax(-beta, -alpha, depth - 1, 1);
					board.unmakeMove();

					int candidateScore = score;
					if (isReverseOfLastMove(move) && !MoveOrderUtil::isCaptureMove(board, move)) {
						candidateScore -= REVERSE_MOVE_PENALTY;
					}

					if (candidateScore > alpha) {
						alpha = candidateScore;
						localBestMove = move;
					} else if (candidateScore == alpha && localBestMove.isValid()) {
						if (isReverseOfLastMove(localBestMove) && !isReverseOfLastMove(move)) {
							localBestMove = move;
						}
					}
				}
			}

			if (localBestMove.isValid()) {
				bestMove = localBestMove;
				LOG_DEBUG_F("[SearchRoot] depth=%d selected=%s score=%d", depth, localBestMove.toString().c_str(), alpha);
			}

			return alpha;
		}

		bool shouldStop() const {
			if (settings.abortSearch) return true;
			if (settings.endlessSearch) return false;
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - searchStart).count();
			return elapsed >= settings.searchTime;
		}

		ThreadPool threadPool{};
		Evaluation evaluation;
		MoveGenerator gen;
		BoardState& board;
		TranspositionTable TT_Table;
		Move bestMove = Move::invalid();
		std::chrono::steady_clock::time_point searchStart{};

		SearchSettings settings;

		int TT_SizeMB = 100;

		static constexpr int P_INFINITY = 1000000;
		static constexpr int N_INFINITY = -P_INFINITY;

		static constexpr int CHECKMATE_VALUE = 100000;
		static constexpr int DRAW_VALUE = 0;
	};
}

#endif // SEARCH_H
