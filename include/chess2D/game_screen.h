#ifndef SCREEN_H
#define SCREEN_H

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <iostream>

#include "board.h"
#include "search.h"

namespace Chess  {
    class GameWindow {
    public:
        GameWindow(int width = 600, int height = 600);
    
        virtual void show();
        virtual void update();
        void run();
        void destroy();

        void initializeGame(const std::string& fen = "");
        void resetGame();

        // Return a selected move by value. Accept Board& so promotions can be handled by Board.
        const Move pickMove(const std::vector<Move>& moves, int targetSquare);

        void handleMouseClick(int mouseX, int mouseY, Board& board, bool leftMouseClicked);
        void makeMove(const Move& move, Board& board);
        void handleComputerMove();

        const int getSelectedPieceSquare() const;
        void clearSelection();

    private:
        SDL_Window* window;
        SDL_Renderer* renderer;
        SDL_Surface* icon = nullptr;
        Input input;
        Board gameBoard;
        bool running = true;
        bool computerPlayEnabled = true;
        double deltaTime;
        int playerColor = COLOR_WHITE;
        int selectedSquare = -1;
        SearchSettings aiSettings = Search::DefaultSettings();
    };
}

#endif // SCREEN_H
