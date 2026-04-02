#ifndef BOARD_RENDERER_H
#define BOARD_RENDERER_H

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <array>
#include <string>
#include <vector>

#include "move.h"
#include "pieces.h"

namespace Chess{
    // Forward declarations
    class Move;
    class BoardState;


    struct RenderColors {
        SDL_Color selectedSquare =  { 0  , 255, 0  , 110 };      // Semi-transparent green
        SDL_Color validMove =       { 0  , 255, 0  , 150 };      // Semi-transparent green  
        SDL_Color invalidMove =     { 255, 0  , 0  , 150 };      // Semi-transparent red
        SDL_Color lastMove =        { 255, 255, 0  , 185 };      // Semi-transparent yellow
        SDL_Color lightSquare =     { 255, 255, 255, 255 };      // Light chess square
        SDL_Color darkSquare =      { 0  , 0  , 0  , 255 };      // Dark chess square
    };
    struct BoardColors {
        SDL_Color lightSquare = { 255, 255, 255, 255 };      // Light chess square
        SDL_Color darkSquare = { 0  , 0  , 0  , 255 };      // Dark chess square
    };
    
    class BoardRenderer {
    private:
        SDL_Renderer* renderer;
        RenderColors colors;

        BoardColors colorSet1;
        BoardColors colorSet2 { { 238, 238, 210, 255 }, { 118, 150, 86 , 255 } };
        BoardColors colorSet3 { { 207, 118, 51 , 255 }, { 90 , 56 , 47 , 255 } };

        const BoardState* board = nullptr;
        std::array<std::array<SDL_FRect, 8>, 8> boardGrid;

        // Single set of piece textures, reloaded when user switches theme
        std::array<SDL_Texture*, 12> pieceTextures{};

        bool isFlipped = false;
        float squareSize = 0;

        int colorOption = 1;      // 1..3 for board color themes
        int pieceTexOption = 1;   // 1..3 for piece texture sets

        static int textureIndex(int color, int pieceType);
        static std::string pieceTexturePath(int color, int pieceType, int pieceTexOption);
        void ensurePieceTexturesLoaded();
        void destroyPieceTextures();

        void setBlendModeAlpha();
        void resetBlendMode();

    public:
        BoardRenderer(SDL_Renderer* renderer);
        ~BoardRenderer();

        void initialize(float squareSize, bool flipped);

        void drawChessBoard(int square, const std::vector<Move>& moves, const BoardState& board);

        void drawBackground();
        void drawSquareHighlights(int square, const std::vector<Move>& moves);
        void drawSelectedSquareHighlight(int square, const std::vector<Move>& moves);
        void drawPossibleMoveHighlights(const std::vector<Move>& moves);
        void drawLastMoveHighlight(int square);
        void drawPieces();
        void drawBoard();
        void drawCoordinates();

        void setFlipped(bool flipped);

        // colorOption: 1..3 select board color theme
        void setColors(int option);
        // pieceOption: piece texture sets 1..3
        void setPieceTex(int option);

        void setGrid(const std::array<std::array<SDL_FRect, 8>, 8>& grid, float squareSize);

        SDL_FRect getSquareRect(int row, int col) const { return boardGrid[row][col]; }
    };
} // namespace Chess
#endif // BOARD_RENDERER_H