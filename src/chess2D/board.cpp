#include "board.h"
#include "board_state.h"
#include "board_renderer.h"
#include "move_generator.h"
#include <cmath>
#include <iostream>

namespace Chess {
    Board::Board(int length) : boardRenderer(nullptr) {
        boardLength = static_cast<float>(length);
        float offset = 0.0f;
        startXPos = offset;
        startYPos = offset;
        squareSize = std::round((boardLength / 8.0f) * 100.0f) / 100.0f;
    }

    Board::~Board() {}

    void Board::loadFEN(const std::string& fen) {
        boardState.loadFEN(fen);
    }

    void Board::initializeBoard(SDL_Renderer* renderer) {
        boardState.init();
        gen.init();
        boardRenderer = BoardRenderer(renderer);

        boardRenderer.initialize(squareSize, isFlipped);

        // Build grid of square rects (row 0 = top)
        std::array<std::array<SDL_FRect, 8>, 8> grid{};
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                float x = startXPos + col * squareSize;
                float y = startYPos + row * squareSize;
                grid[row][col] = SDL_FRect{ x, y, squareSize, squareSize };
            }
        }

        boardRenderer.setGrid(grid, squareSize);
    }

    void Board::resetBoard() {
        boardState.reset();
    }

    void Board::makeMove(Move move) {
        boardState.makeMove(move);
    }

    void Board::unmakeMove() {
        if (boardState.unmakeMove()) {
            return;
        }
        std::cout << "Board::unmakeMove: Move was not unmade" << std::endl;
    }

    bool Board::screenToBoardCoords(int screenX, int screenY,
        int& boardR, int& boardC) const {
        const float relX = static_cast<float>(screenX) - startXPos;
        const float relY = static_cast<float>(screenY) - startYPos;

        if (relX < 0.0f || relY < 0.0f) return false;

        const int col = static_cast<int>(relX / squareSize);
        const int row = static_cast<int>(relY / squareSize);

        if (row < 0 || row >= 8 || col < 0 || col >= 8) return false;

        boardR = row;
        boardC = col;
        return true;
    }
    
    void Board::draw(int selectedSquare) {
        std::vector<Move> moves;
        if (selectedSquare >= 0 && selectedSquare < 64) {
            moves = gen.getPieceMoves(selectedSquare, &boardState);
        }
        boardRenderer.drawChessBoard(selectedSquare, moves, boardState);
    }

    bool Board::isCheckMate(int color) {
        if (color != boardState.getSide()) return false;
        gen.generateLegalMoves(boardState, true);
        if (gen.getLegalMoveCount() > 0) return false;
        return gen.getInCheck();
    }

    bool Board::isStaleMate(int color) {
        if (color != boardState.getSide()) return false;
        gen.generateLegalMoves(boardState, true);
        if (gen.getLegalMoveCount() > 0) return false;
        return !gen.getInCheck();
    }

	const Move Board::handlePawnPromotion(int square) {
		int fromSq = square - ((boardState.getSide() == COLOR_WHITE) ? 8 : -8);
        if (fromSq < 0 || fromSq >= 64) {
            std::cout << "handlePawnPromotion: invalid from-square computed" << std::endl;
            return Move::invalid();
        }

        auto candidates = gen.getPieceMoves(fromSq, &boardState);
        std::vector<Move> promotionMoves;
        for (const auto& m : candidates) {
            if (!m.isValid()) continue;
			if (m.isPromotion() && m.targetSquare() == square) promotionMoves.push_back(m);
        }

        if (promotionMoves.empty()) {
            std::cout << "No promotion moves available." << std::endl;
            return Move::invalid();
        }

		std::cout << "Pawn promotion at " << square << ". Choose piece: (q)ueen, (r)ook, (b)ishop, k(n)ight: ";
        char choice = 'q';
        std::cin >> choice;
        choice = static_cast<char>(std::tolower(static_cast<unsigned char>(choice)));

        int desiredFlag = Move::Flag::PromoteToQueen;
        switch (choice) {
        case 'r': desiredFlag = Move::Flag::PromoteToRook; break;
        case 'b': desiredFlag = Move::Flag::PromoteToBishop; break;
        case 'n': desiredFlag = Move::Flag::PromoteToKnight; break;
        case 'q':
        default:  desiredFlag = Move::Flag::PromoteToQueen; break;
        }

        // Find matching promotion move and make it. If not found, fallback to first promotion move.
        const Move* chosen = nullptr;
        for (const auto& m : promotionMoves) {
            if (m.flag() == desiredFlag) { chosen = &m; break; }
        }
        if (!chosen) chosen = &promotionMoves[0];

        return *chosen;
    }


    SDL_FRect Board::getSquareRect(int r, int c) const {
        return boardRenderer.getSquareRect(r, c);
    }

    void Board::setFlipped(bool flipped) {
        isFlipped = flipped;
        boardRenderer.setFlipped(flipped);
    }

    void Board::setThemeColor(int option) {
        boardRenderer.setColors(option);
    }
    
    void Board::setPieceTheme(int option) {
        boardRenderer.setPieceTex(option);
    }
}