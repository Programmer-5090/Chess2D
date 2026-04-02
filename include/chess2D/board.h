#ifndef BOARD_H
#define BOARD_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <memory>

#include "board_renderer.h"     
#include "board_state.h"  
#include "move_generator.h"

#include "input.h"
#include "move.h"
#include "pieces.h"

namespace Chess {

    class Board {
    private:
        float boardLength;
        float squareSize;
        float startXPos;
        float startYPos;
        bool isFlipped = false;

        BoardRenderer boardRenderer;
        BoardState    boardState;
        MoveGenerator gen;

    public:
        Board(int length);
        ~Board();

        void initializeBoard(SDL_Renderer* renderer);
        void loadFEN(const std::string& fen);
        void resetBoard();

        void draw(int selectedSquare);

        void makeMove(Move move);
        void unmakeMove();

        bool      screenToBoardCoords(int screenX, int screenY, int& boardR, int& boardC) const;

        bool isCheckMate(int color);
        bool isStaleMate(int color);

        const Move handlePawnPromotion(int square);
        
        SDL_FRect getSquareRect(int r, int c) const;
        std::vector<Move> getLegalMovesForSquare(int square) { return gen.getPieceMoves(square, &boardState); }

        void setFlipped(bool flipped);
        void setCurrentPlayer(int player) { boardState.setSide(player); }

        // Theme controls
        void setThemeColor(int option);
        void setPieceTheme(int option);

        std::string getFEN()     const { return boardState.getFEN(); }
        bool        getIsFlipped()    const { return isFlipped; }
        int         getCurrentPlayer()  const { return boardState.getSide(); }
        int         getHalfMoveClock()  const { return boardState.getFiftyMove(); }
        int         getFullMoveNumber() const { return boardState.getHisPly(); }
    };

} // namespace Chess

#endif // BOARD_H
