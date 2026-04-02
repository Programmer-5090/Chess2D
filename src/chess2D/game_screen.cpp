#include "game_screen.h"
#include <iostream>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL.h>

namespace Chess {

    // Convert a grid (row, col) produced by Board::screenToBoardCoords back to a
    // 0-63 square index, honouring the board-flip state.
    static int gridToSquare64(int row, int col, bool flipped) {
        const int rank = flipped ? row : (7 - row);
        const int file = flipped ? (7 - col) : col;
        return rank * 8 + file;
    }


    GameWindow::GameWindow(int width, int height)
        : gameBoard(std::min(width, height)), deltaTime(0.0)   // Board is always square
    {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "[GameWindow] SDL_Init failed: " << SDL_GetError() << '\n';
            running = false;
            return;
        }

        window = SDL_CreateWindow("Chess", width, height, SDL_WINDOW_RESIZABLE);
        if (!window) {
            std::cerr << "[GameWindow] SDL_CreateWindow failed: " << SDL_GetError() << '\n';
            running = false;
            return;
        }

        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::cerr << "[GameWindow] SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
            running = false;
            return;
        }

        // Alpha blending is used by the renderer for square highlights.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        icon = IMG_Load("resources/chess_icon.png");
        if (!icon) {
            std::cerr << "Failed to load icon: " << SDL_GetError() << std::endl;
        }
        else {
            SDL_SetWindowIcon(window, icon);
        }
    }


    void GameWindow::show() {
        if (window) {
            SDL_ShowWindow(window);
        }
    }

    void GameWindow::update() {
        input.update();

        if (input.shouldQuit()) {
            running = false;
        }

        // Handle Mouse Input
        if (input.isMouseButtonReleased(SDL_BUTTON_LEFT)) {
            handleMouseClick(input.getMouseX(), input.getMouseY(), gameBoard, true);
        }
        else if (input.isMouseButtonReleased(SDL_BUTTON_RIGHT)) {
            handleMouseClick(input.getMouseX(), input.getMouseY(), gameBoard, false);
        }

        // Clear screen with a dark gray background
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        // Draw the chess board and pass the currently selected square for highlighting
        gameBoard.draw(selectedSquare);

        // Present the rendered frame
        SDL_RenderPresent(renderer);
    }

    void GameWindow::run() {
        show();

        uint64_t lastTime = SDL_GetTicks();

        while (running) {
            uint64_t currentTime = SDL_GetTicks();
            deltaTime = (currentTime - lastTime) / 1000.0;
            lastTime = currentTime;

            if (input.keyDown("R")) resetGame();
            if (input.keyDown("U")) {
                gameBoard.unmakeMove();
                clearSelection();
            }
            if (input.keyDown("F")) {
                gameBoard.setFlipped(!gameBoard.getIsFlipped());
                clearSelection();
            }
            if (input.keyDown("1")) gameBoard.setThemeColor(1);
            if (input.keyDown("2")) gameBoard.setThemeColor(2);
            if (input.keyDown("3")) gameBoard.setThemeColor(3);
            if (input.keyDown("4")) gameBoard.setPieceTheme(1);
            if (input.keyDown("5")) gameBoard.setPieceTheme(2);
            if (input.keyDown("6")) gameBoard.setPieceTheme(3);

            update();
        }

        destroy();
    }

    void GameWindow::destroy() {
        if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
        if (window) { SDL_DestroyWindow(window);     window = nullptr; }
        if (icon) { SDL_DestroySurface(icon);       icon = nullptr; }
        // Quit SDL_image if initialized
        SDL_Quit();
    }

    void GameWindow::initializeGame(const std::string& fen) {
        gameBoard.initializeBoard(renderer);
        if (!fen.empty()) {
            gameBoard.loadFEN(fen);
        }
    }

    void GameWindow::resetGame() {
        gameBoard.resetBoard();
        clearSelection();
    }

    void GameWindow::handleMouseClick(int mouseX, int mouseY,
        Board& board, bool leftMouseClicked) {
        // Right-click always clears the selection.
        if (!leftMouseClicked) {
            clearSelection();
            return;
        }

        int row = 0, col = 0;
        if (!board.screenToBoardCoords(mouseX, mouseY, row, col)) {
            // Click landed outside the board.
            clearSelection();
            return;
        }

        const int clickedSquare = gridToSquare64(row, col, board.getIsFlipped());

        if (selectedSquare < 0) {
            selectedSquare = clickedSquare;
        }
        else {
            if (clickedSquare == selectedSquare) {
                clearSelection();
                return;
            }
            const auto moves = board.getLegalMovesForSquare(selectedSquare);
            Move chosen = pickMove(moves, clickedSquare);

            if (chosen.isValid()) {
                makeMove(chosen, board);
            } else {
                selectedSquare = clickedSquare;
            }
        }
    }

    void GameWindow::makeMove(const Move& move, Board& board) {
        board.makeMove(move);
        clearSelection();

        // Evaluate the position for the side that must now move.
        const int active = board.getCurrentPlayer();

        if (board.isCheckMate(active)) {
            const char* winner = (active == COLOR_WHITE) ? "Black" : "White";
            std::cout << "[Game] Checkmate! " << winner << " wins!\n";
        }
        else if (board.isStaleMate(active)) {
            std::cout << "[Game] Stalemate – the game is a draw.\n";
        }
    }

    const Move GameWindow::pickMove(const std::vector<Move>& moves, int targetSquare) {
        const Move* fallback = nullptr;
        std::vector<Move> promotionMoves;

        for (const auto& m : moves) {
            if (!m.isValid() || m.targetSquare() != targetSquare) continue;

            if (!m.isPromotion()) return m;
            promotionMoves.push_back(m);

            if (!fallback) fallback = &m;
        }
        if (!promotionMoves.empty()) {
            Move chosen = gameBoard.handlePawnPromotion(targetSquare);
            if (chosen.isValid()) return chosen;

            for (const auto& m : promotionMoves) {
                if (m.flag() == Move::Flag::PromoteToQueen) return m;
            }
        }

        if (fallback) return *fallback;
        return Move::invalid();
    }

    const int GameWindow::getSelectedPieceSquare() const {
        return selectedSquare;
    }

    void GameWindow::clearSelection() {
        selectedSquare = -1;
    }

} // namespace Chess