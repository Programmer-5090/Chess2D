#include "board_renderer.h"

#include "board_state.h"
#include "board_rep.h"
#include <iostream>
#include <algorithm>

namespace Chess {

    static bool squareToGrid(int square64, bool flipped, int& outRow, int& outCol) {
        if (square64 < 0 || square64 >= 64) return false;
        const int file = BoardRepresentation::FileIndex(square64);
        const int rank = BoardRepresentation::RankIndex(square64);

        if (!flipped) {
            outRow = 7 - rank;
            outCol = file;
        } else {
            outRow = rank;
            outCol = 7 - file;
        }

        return outRow >= 0 && outRow < 8 && outCol >= 0 && outCol < 8;
    }

    BoardRenderer::BoardRenderer(SDL_Renderer* renderer) : renderer(renderer) {
        boardGrid = { SDL_FRect{0, 0, 0, 0} };
        SDL_SetDefaultTextureScaleMode(renderer, SDL_SCALEMODE_NEAREST);
    }

    BoardRenderer::~BoardRenderer() {
        destroyPieceTextures();
    }

    void BoardRenderer::initialize(float squareSize, bool flipped) {
        this->squareSize = squareSize;
        this->isFlipped = flipped;
    }

    void BoardRenderer::setBlendModeAlpha() {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }
    void BoardRenderer::resetBlendMode() {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    void BoardRenderer::drawChessBoard(int square, const std::vector<Move>& moves, const BoardState& board) {
        this->board = &board;
        drawBackground();
        drawSquareHighlights(square, moves);
        drawPieces();
    }

    void BoardRenderer::drawBackground() {
        BoardColors colors;
        if (colorOption == 1) colors = colorSet1;
        else if (colorOption == 2) colors = colorSet2;
        else colors = colorSet3;

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                const bool light = ((row + col) % 2) == 0;
                const SDL_Color c = light ? colors.lightSquare : colors.darkSquare;
                SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
                SDL_RenderFillRect(renderer, &boardGrid[row][col]);
            }
        }
    }

    void BoardRenderer::drawSquareHighlights(int square, const std::vector<Move>& moves) {
        drawSelectedSquareHighlight(square, moves);
        drawPossibleMoveHighlights(moves);
        if (board) {
            Move lm = board->getLastMove();
            if (lm.isValid()) {
                drawLastMoveHighlight(lm.startSquare());
                drawLastMoveHighlight(lm.targetSquare());
            }
        }
    }

    void BoardRenderer::drawSelectedSquareHighlight(int square, const std::vector<Move>& moves) {
        if (square < 0 || square >= 64) return;
        int row = 0;
        int col = 0;
        if (!squareToGrid(square, isFlipped, row, col)) return;

        bool isLegalMove = false;
        for (const auto& m : moves) {
            if (!m.isValid()) continue;
            if (m.startSquare() == square) { isLegalMove = true; break; }
        }

        setBlendModeAlpha();
        if (isLegalMove) {
            SDL_SetRenderDrawColor(renderer, colors.selectedSquare.r, colors.selectedSquare.g, colors.selectedSquare.b, colors.selectedSquare.a);
        } else {
            if (board->getColorAt(square) == board->getSide()) {
                SDL_SetRenderDrawColor(renderer, colors.selectedSquare.r, colors.selectedSquare.g, colors.selectedSquare.b, colors.selectedSquare.a);
            } else {
                SDL_SetRenderDrawColor(renderer, colors.invalidMove.r, colors.invalidMove.g, colors.invalidMove.b, colors.invalidMove.a);
            }
        }
        SDL_RenderFillRect(renderer, &boardGrid[row][col]);
        resetBlendMode();
    }

    void BoardRenderer::drawPossibleMoveHighlights(const std::vector<Move>& moves) {
        if (moves.empty()) return;

        setBlendModeAlpha();
        SDL_SetRenderDrawColor(renderer, colors.validMove.r, colors.validMove.g, colors.validMove.b, colors.validMove.a);

        for (const auto& m : moves) {
            if (!m.isValid()) continue;
            int row = 0;
            int col = 0;
            if (!squareToGrid(m.targetSquare(), isFlipped, row, col)) continue;
            SDL_RenderFillRect(renderer, &boardGrid[row][col]);
        }

        resetBlendMode();
    }

    void BoardRenderer::drawLastMoveHighlight(int square) {
        if (square < 0 || square >= 64) return;
        int row = 0;
        int col = 0;
        if (!squareToGrid(square, isFlipped, row, col)) return;

        setBlendModeAlpha();
        SDL_SetRenderDrawColor(renderer, colors.lastMove.r, colors.lastMove.g, colors.lastMove.b, colors.lastMove.a);
        SDL_RenderFillRect(renderer, &boardGrid[row][col]);
        resetBlendMode();
    }

    void BoardRenderer::drawPieces() {
        if (!board) return;
        ensurePieceTexturesLoaded();

        for (int sq = 0; sq < 64; ++sq) {
            const int pieceType = board->getPieceTypeAt(sq);
            if (pieceType < 0 || pieceType >= 6) continue;
            const int color = board->getColorAt(sq);
            if (color != COLOR_WHITE && color != COLOR_BLACK) continue;

            int row = 0;
            int col = 0;
            if (!squareToGrid(sq, isFlipped, row, col)) continue;

            const SDL_FRect squareRect = boardGrid[row][col];
            SDL_Texture* tex = pieceTextures[textureIndex(color, pieceType)];
            if (!tex) continue;

            // Query texture size and compute destination rect preserving aspect ratio
            float texW = 0, texH = 0;
            if (SDL_GetTextureSize(tex, &texW, &texH) != 0) {
                // If query fails, fallback to filling the square
                SDL_RenderTexture(renderer, tex, nullptr, &squareRect);
                continue;
            }

            const float maxW = squareRect.w;
            const float maxH = squareRect.h;
            const float scale = std::min(maxW / static_cast<float>(texW), maxH / static_cast<float>(texH));
            const float renderW = texW * scale;
            const float renderH = texH * scale;
            SDL_FRect dst = { squareRect.x + (maxW - renderW) * 0.5f,
                              squareRect.y + (maxH - renderH) * 0.5f,
                              renderW, renderH };

            SDL_RenderTexture(renderer, tex, nullptr, &dst);
        }
    }

    int BoardRenderer::textureIndex(int color, int pieceType) {
        return color * 6 + pieceType;
    }

    std::string BoardRenderer::pieceTexturePath(int color, int pieceType, int pieceTexOption) {
        const char* prefix = (color == COLOR_WHITE) ? "W_" : "B_";

        const char* name = "";
        switch (pieceType) {
            case PIECE_PAWN: name = "Pawn"; break;
            case PIECE_KNIGHT: name = "Knight"; break;
            case PIECE_BISHOP: name = "Bishop"; break;
            case PIECE_ROOK: name = "Rook"; break;
            case PIECE_QUEEN: name = "Queen"; break;
            case PIECE_KING: name = "King"; break;
            default: name = ""; break;
        }

        // pieceTexOption = 1..3 maps to directories "Piece Images 1" .. "Piece Images 3"
        std::string dir = "resources/Piece Images ";
        dir += std::to_string(pieceTexOption);
        return dir + "/" + prefix + name + ".png";
    }

    void BoardRenderer::setFlipped(bool flipped) {
        isFlipped = flipped;
    }

    void BoardRenderer::setColors(int option) {
        colorOption = std::clamp(option, 1, 3);
    }

    void BoardRenderer::setPieceTex(int option) {
        pieceTexOption = std::clamp(option, 1, 3);
        destroyPieceTextures(); // force reload on next draw
    }

    void BoardRenderer::drawBoard() {}
    void BoardRenderer::drawCoordinates() {}

    void BoardRenderer::setGrid(const std::array<std::array<SDL_FRect, 8>, 8>& grid, float squareSize) {
        boardGrid = grid;
        this->squareSize = squareSize;
    }

    void BoardRenderer::ensurePieceTexturesLoaded() {
        for (auto& t : pieceTextures) {
            if (t != nullptr) {
                return;
            }
        }

        pieceTextures.fill(nullptr);
        int loadedCount = 0;
        const int total = 12;
        for (int color = 0; color < 2; ++color) {
            for (int type = 0; type < 6; ++type) {
                const int idx = textureIndex(color, type);
                const std::string path = pieceTexturePath(color, type, pieceTexOption);
                pieceTextures[idx] = IMG_LoadTexture(renderer, path.c_str());
                if (pieceTextures[idx]) {
                    ++loadedCount;
                    std::cout << "Loaded texture: " << path << "\n";
                } else {
                    std::cerr << "Failed to load texture: " << path << " - " << SDL_GetError() << "\n";
                }
                if (pieceTexOption == 1 || pieceTexOption == 2) SDL_SetTextureScaleMode(pieceTextures[idx], SDL_SCALEMODE_NEAREST);
                else SDL_SetTextureScaleMode(pieceTextures[idx], SDL_SCALEMODE_LINEAR);
            }
        }

        std::cout << "Piece textures loaded: " << loadedCount << " / " << total << std::endl;
    }

    void BoardRenderer::destroyPieceTextures() {
        for (auto& tex : pieceTextures) {
            if (tex) {
                SDL_DestroyTexture(tex);
                tex = nullptr;
            }
        }
    }
}