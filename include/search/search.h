#ifndef SEARCH_H
#define SEARCH_H

#include <vector>
#include <chrono>
#include <future>
#include <sstream>
#include <memory>

#include "thread_pool.h"
#include "move_order_util.h"
#include "trans_table.h"
#include "evaluation.h"
#include "move_generator.h"
#include "board_state.h"
#include "logger.h"
#include "profiler.h"

namespace Chess {

	struct SearchSettings {
		int depth;
		bool useIterativeDeepening;
		bool useThreading;
		bool useMoveOrdering;
		bool logMoveOrdering;
		int logMoveOrderingMax;
		bool logRootMoveScores;
		bool logDepthTiming;
		EvaluationOptions evaluation;
		bool detectRepetitionDraw;
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
			s.useMoveOrdering = true;
			s.logMoveOrdering = false;
			s.logMoveOrderingMax = 32;
			s.logRootMoveScores = false;
			s.logDepthTiming = false;
			s.evaluation.useMobilityEvaluation = true;
			s.evaluation.contemptScore = 0;
			s.detectRepetitionDraw = true;
			s.searchTime = 1000;
			s.endlessSearch = false;
			s.abortSearch = false;
			return s;
		}

		Search(BoardState& boardRef, int ttSizeMB = 100, SearchSettings settings = DefaultSettings(),
			std::shared_ptr<TranspositionTable> sharedTT = nullptr)
			: board(boardRef),
			evaluation(boardRef, settings.evaluation),
			TT_Table(sharedTT ? std::move(sharedTT) : std::make_shared<TranspositionTable>(ttSizeMB)),
			settings(settings),
			TT_SizeMB(ttSizeMB) {
			gen.init();
		}

		int runSearch(int depth) {
			evaluation.setOptions(settings.evaluation);
			searchStart = std::chrono::steady_clock::now();
			bestMove = Move::invalid();
			TT_Table->newSearch();
			MoveOrderUtil::clearHeuristics();

			int targetDepth = depth > 0 ? depth : settings.depth;
			if (targetDepth <= 0) targetDepth = 1;

			int bestScore = 0;
			if (settings.useIterativeDeepening || settings.endlessSearch) {
				for (int currentDepth = 1; !shouldStop(); ++currentDepth) {
					if (!settings.endlessSearch && currentDepth > targetDepth) break;
					const auto depthStart = std::chrono::steady_clock::now();
					bestScore = searchRoot(currentDepth);
					if (settings.logDepthTiming) {
						const auto depthMs = std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now() - depthStart).count();
						LOG_DEBUG_F("[DepthTiming] depth=%d elapsed=%lldms", currentDepth, depthMs);
					}
				}
			} else {
				const auto depthStart = std::chrono::steady_clock::now();
				bestScore = searchRoot(targetDepth);
				if (settings.logDepthTiming) {
					const auto depthMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - depthStart).count();
					LOG_DEBUG_F("[DepthTiming] depth=%d elapsed=%lldms", targetDepth, depthMs);
				}
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

			if (board.isDrawByFiftyMove()
				|| board.isInsufficientMaterial()
				|| (settings.detectRepetitionDraw && board.isRepetition())) {
				return settings.evaluation.contemptScore;
			}

			if (depth <= 0) {
				return QuiescenceSearch(alpha, beta, ply);
			}

			const int ttScore = TT_Table->lookupEval(alpha, beta, depth, ply, board);
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

			const Move ttMove = TT_Table->lookupMove(board);
			std::vector<Move> orderedMoves;
			if (settings.useMoveOrdering) {
				MoveOrderUtil::orderMoves(board, moves, ply, ttMove, orderedMoves);
			} else {
				orderedMoves = moves;
			}

			const int originalAlpha = alpha;
			Move localBestMove = Move::invalid();
			bool isFirstMove = true;

			for (const Move& move : orderedMoves) {
				board.makeMove(move);
				int score = 0;
				if (isFirstMove) {
					score = -NegaMax(-beta, -alpha, depth - 1, ply + 1);
				} else {
					// PVS: null-window search first, then re-search if it improves alpha.
					score = -NegaMax(-(alpha + 1), -alpha, depth - 1, ply + 1);
					if (score > alpha && score < beta) {
						score = -NegaMax(-beta, -alpha, depth - 1, ply + 1);
					}
				}
				board.unmakeMove();
				isFirstMove = false;

				if (score >= beta) {
					if (!MoveOrderUtil::isCaptureMove(board, move)) {
						MoveOrderUtil::updateKiller(move, ply);
						MoveOrderUtil::updateHistory(move, board.getSide(), depth);
					}
					TT_Table->storeEval(score, depth, ply, LOWER, board, move); // LOWER bound (fail-soft)
					return score;
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
			TT_Table->storeEval(alpha, depth, ply, boundType, board, localBestMove);
			return alpha;
		}

		int QuiescenceSearch(int alpha, int beta, int ply) {
			if (shouldStop()) {
				return evaluation.Evaluate();
			}

			if (board.isDrawByFiftyMove()
				|| board.isInsufficientMaterial()
				|| (settings.detectRepetitionDraw && board.isRepetition())) {
				return settings.evaluation.contemptScore;
			}

			gen.generateLegalMoves(board, false);
			const bool inCheck = gen.getInCheck();

			if (!inCheck) {
				const int standPat = evaluation.Evaluate();
				if (standPat >= beta) {
					return standPat;
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
			if (settings.useMoveOrdering) {
				MoveOrderUtil::orderMoves(board, moves, orderedMoves);
			} else {
				orderedMoves = moves;
			}

			for (const Move& move : orderedMoves) {
				board.makeMove(move);
				const int score = -QuiescenceSearch(-beta, -alpha, ply + 1);
				board.unmakeMove();

				if (score >= beta) {
					return score;
				}

				if (score > alpha) {
					alpha = score;
				}
			}

			return alpha;
		}

	private:
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

			const Move ttMove = TT_Table->lookupMove(board);
			std::vector<Move> orderedMoves;
			if (settings.useMoveOrdering) {
				MoveOrderUtil::orderMoves(board, moves, 0, ttMove, orderedMoves);
			} else {
				orderedMoves = moves;
			}

			if (settings.logMoveOrdering) {
				logOrderedRootMoves(depth, ttMove, orderedMoves);
			}

			int alpha = N_INFINITY;
			const int beta = P_INFINITY;
			Move localBestMove = Move::invalid();
			auto logRootMoveScore = [this, depth](const Move& move, int score) {
				if (settings.logRootMoveScores) {
					LOG_DEBUG_F("[SearchRoot] depth=%d move=%s score=%d", depth, move.toString().c_str(), score);
				}
			};

			const int poolWorkers = static_cast<int>(sharedThreadPool().getThreadCount() > 0 ? sharedThreadPool().getThreadCount() - 1 : 0);
			int allowedWorkers = poolWorkers;
			if (allowedWorkers < 0) allowedWorkers = 0;
			if (allowedWorkers > MAX_ROOT_THREADS) allowedWorkers = MAX_ROOT_THREADS;

			if (settings.useThreading
				&& depth >= MIN_THREADED_ROOT_DEPTH
				&& static_cast<int>(orderedMoves.size()) >= MIN_THREADED_ROOT_MOVES
				&& allowedWorkers > 0) {
				// Search first move synchronously to get an initial alpha for better move quality.
				const Move firstMove = orderedMoves.front();
				board.makeMove(firstMove);
				alpha = -NegaMax(-beta, -alpha, depth - 1, 1);
				board.unmakeMove();
				localBestMove = firstMove;
				logRootMoveScore(firstMove, alpha);

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

					futures.push_back(sharedThreadPool().enqueue([boardCopy, move, depth, childSettings, start = searchStart, this]() mutable {
						Search child(boardCopy, TT_SizeMB, childSettings, TT_Table);
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
					logRootMoveScore(move, score);

					if (score > alpha || (score == alpha && preferRootMoveOnTie(move, localBestMove))) {
						alpha = score;
						localBestMove = move;
					}
				}

				for (auto& f : futures) {
					const auto [score, move] = f.get();
					logRootMoveScore(move, score);

					if (score > alpha || (score == alpha && preferRootMoveOnTie(move, localBestMove))) {
						alpha = score;
						localBestMove = move;
					}
				}
			} else {
				bool isFirstMove = true;
				for (const Move& move : orderedMoves) {
					if (shouldStop()) break;
					board.makeMove(move);
					int score = 0;
					if (isFirstMove) {
						score = -NegaMax(-beta, -alpha, depth - 1, 1);
					} else {
						score = -NegaMax(-(alpha + 1), -alpha, depth - 1, 1);
						if (score > alpha && score < beta) {
							score = -NegaMax(-beta, -alpha, depth - 1, 1);
						}
					}
					board.unmakeMove();
					isFirstMove = false;
					logRootMoveScore(move, score);

					if (score > alpha || (score == alpha && preferRootMoveOnTie(move, localBestMove))) {
						alpha = score;
						localBestMove = move;
					}
				}
			}

			if (localBestMove.isValid()) {
				bestMove = localBestMove;
				LOG_DEBUG_F("[SearchRoot] depth=%d selected=%s score=%d", depth, localBestMove.toString().c_str(), alpha);
			}

			return alpha;
		}

		bool preferRootMoveOnTie(const Move& candidate, const Move& currentBest) const {
			if (!candidate.isValid()) return false;
			if (!currentBest.isValid()) return true;
			return rootMoveTieBreakScore(candidate) > rootMoveTieBreakScore(currentBest);
		}

		int rootMoveTieBreakScore(const Move& move) const {
			if (!move.isValid()) return -100000;
			const int pieceType = board.getPieceTypeAt(move.startSquare());
			if (pieceType < 0) return -100000;

			int score = 0;
			const bool opening = board.getHisPly() < 20;
			const int side = board.getSide();
			const int rights = board.getCastleRights();

			if (move.flag() == Move::Flag::Castling) {
				score += 50;
			}

			if (opening) {
				switch (pieceType) {
				case PIECE_KNIGHT:
				case PIECE_BISHOP:
					score += 12;
					break;
				case PIECE_ROOK:
					score -= 30;
					break;
				case PIECE_QUEEN:
					score -= 14;
					break;
				case PIECE_KING:
					if (move.flag() != Move::Flag::Castling) score -= 30;
					break;
				default:
					break;
				}
			}

			if (pieceType == PIECE_ROOK) {
				const int from = move.startSquare();
				if (side == COLOR_WHITE) {
					if (from == BoardRepresentation::h1 && (rights & 0x01)) score -= 18;
					if (from == BoardRepresentation::a1 && (rights & 0x02)) score -= 12;
				}
				else {
					if (from == BoardRepresentation::h8 && (rights & 0x04)) score -= 18;
					if (from == BoardRepresentation::a8 && (rights & 0x08)) score -= 12;
				}
			}

			if (pieceType == PIECE_KING) {
				const int from = move.startSquare();
				if (side == COLOR_WHITE && from == BoardRepresentation::e1 && (rights & 0x03)) score -= 20;
				if (side == COLOR_BLACK && from == BoardRepresentation::e8 && (rights & 0x0C)) score -= 20;
			}

			return score;
		}

		bool shouldStop() const {
			if (settings.abortSearch) return true;
			if (settings.endlessSearch) return false;
			if (settings.searchTime <= 0) return false;
			const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - searchStart).count();
			return elapsed >= settings.searchTime;
		}

		void logOrderedRootMoves(int depth, const Move& ttMove, const std::vector<Move>& orderedMoves) const {
			std::ostringstream oss;
			oss << "[MoveOrder] depth=" << depth
				<< " total=" << orderedMoves.size()
				<< " tt=" << (ttMove.isValid() ? ttMove.toString() : "-")
				<< " order=";

			int maxToLog = settings.logMoveOrderingMax;
			if (maxToLog <= 0 || maxToLog > static_cast<int>(orderedMoves.size())) {
				maxToLog = static_cast<int>(orderedMoves.size());
			}

			for (int i = 0; i < maxToLog; ++i) {
				if (i > 0) oss << ", ";
				oss << (i + 1) << ":" << orderedMoves[static_cast<std::size_t>(i)].toString();
				if (ttMove.isValid() && orderedMoves[static_cast<std::size_t>(i)] == ttMove) {
					oss << "(TT)";
				}
			}

			if (maxToLog < static_cast<int>(orderedMoves.size())) {
				oss << ", ...";
			}

			LOG_DEBUG(oss.str());
		}

		Evaluation evaluation;
		MoveGenerator gen;
		BoardState& board;
		std::shared_ptr<TranspositionTable> TT_Table;
		Move bestMove = Move::invalid();
		std::chrono::steady_clock::time_point searchStart{};

		static constexpr int MAX_ROOT_THREADS = 8;
		static constexpr int MIN_THREADED_ROOT_DEPTH = 4;
		static constexpr int MIN_THREADED_ROOT_MOVES = 10;

		static ThreadPool& sharedThreadPool() {
			static ThreadPool pool{};
			return pool;
		}

		SearchSettings settings;

		int TT_SizeMB = 100;

		static constexpr int P_INFINITY = 1000000;
		static constexpr int N_INFINITY = -P_INFINITY;

		static constexpr int CHECKMATE_VALUE = 100000;
		static constexpr int DRAW_VALUE = 0;
	};
}  // namespace Chess
#endif // SEARCH_H
