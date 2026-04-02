#include "board_state.h"
#include "board_rep.h"
#include "pieces.h"
#include "move.h"
#include "fen_util.h"
#include <iostream>
#include <iomanip>

using namespace Chess;

/*
 * Helper function to get piece character for display
 * Returns uppercase for white pieces, lowercase for black pieces
 */
char getPieceChar(int pieceType, int color) {
    const char whitePieces[] = { 'K', 'P', 'N', 'B', 'R', 'Q' };
    const char blackPieces[] = { 'k', 'p', 'n', 'b', 'r', 'q' };

    if (pieceType < 0 || pieceType > 5) return '.';
    
    if (color == COLOR_WHITE) {
        return whitePieces[pieceType];
    } else {
        return blackPieces[pieceType];
    }
}

/*
 * Print the chess board in a human-readable format
 * Shows rank numbers on the left and file letters at the top
 */
void printBoard(const BoardState& board) {
    const auto& pieceBoards = board.getPieceBoards();
    
    std::cout << "\n  a b c d e f g h\n";
    std::cout << "  +-+-+-+-+-+-+-+-+\n";
    
    // Print from rank 7 down to rank 0 (top to bottom)
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << (rank + 1) << " |";
        
        for (int file = 0; file < 8; ++file) {
            int sq = BoardRepresentation::IndexFromCoord(file, rank);
            uint64_t sqMask = (1ULL << sq);
            
            int pieceType = -1;
            int color = -1;
            
            // Find which piece is on this square by checking all bitboards
            for (int c = 0; c < 2; ++c) {
                for (int t = 0; t < 6; ++t) {
                    if (pieceBoards[c][t] & sqMask) {
                        color = c;
                        pieceType = t;
                        break;
                    }
                }
                if (pieceType != -1) break;
            }
            
            if (pieceType == -1) {
                std::cout << ".";
            } else {
                std::cout << getPieceChar(pieceType, color);
            }
            std::cout << "|";
        }
        
        std::cout << " " << (rank + 1) << "\n";
        std::cout << "  +-+-+-+-+-+-+-+-+\n";
    }
    
    std::cout << "  a b c d e f g h\n";
}

/*
 * Print board state information
 * Shows side to move, en passant, castling rights, etc.
 */
void printBoardInfo(const BoardState& board) {
    std::cout << "\n=== Board State Info ===\n";
    std::cout << "Side to move: " << (board.getSide() == COLOR_WHITE ? "White" : "Black") << "\n";
    std::cout << "En passant: " << (board.getEnPas() < 0 ? "-" : BoardRepresentation::SquareNameFromIndex(board.getEnPas())) << "\n";
    std::cout << "Fifty move counter: " << board.getFiftyMove() << "\n";
    std::cout << "Move number: " << (board.getHisPly() / 2 + 1) << "\n";
    std::cout << "Position key: 0x" << std::hex << board.getPosKey() << std::dec << "\n";
    std::cout << "Castle rights: ";
    int cr = board.getCastleRights();
    if (cr & 0x01) std::cout << "K";
    if (cr & 0x02) std::cout << "Q";
    if (cr & 0x04) std::cout << "k";
    if (cr & 0x08) std::cout << "q";
    if (cr == 0) std::cout << "-";
    std::cout << "\n";
}


// g++ -std=c++20 -I./include/chess_engine src/chess_engine/*.cpp tests/chess_engine_test.cpp -o tests/chess_board_test
int main() {
    std::cout << "=== Chess Engine Board Test ===\n";
    
    // Create board - constructor initializes Zobrist keys and starting position
    BoardState board;
    
    std::cout << "\nInitial position:\n";
    printBoard(board);
    printBoardInfo(board);
    
    // Test a move: e2 to e4 (white pawn move)
    // e2 = square index 12, e4 = square index 28
    std::cout << "\n\n=== Making move: e2 -> e4 ===\n";
    Move move1(BoardRepresentation::e2, BoardRepresentation::e4);
    board.makeMove(move1);
    
    printBoard(board);
    printBoardInfo(board);
    
    // Test another move: e7 to e5 (black pawn)
    // e7 = square index 52, e5 = square index 36
    std::cout << "\n\n=== Making move: e7 -> e5 ===\n";
    Move move2(BoardRepresentation::e7, BoardRepresentation::e5);
    board.makeMove(move2);
    
    printBoard(board);
    printBoardInfo(board);
    
    // Test kingside castling using FEN
    std::cout << "\n\n=== Castling Test - Kingside ===\n";
    // FEN: Kings and rooks only, white to move, castling available
    board.init("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    
    printBoard(board);
    printBoardInfo(board);
    
    // Kingside castling (O-O): King e1 to g1
    // e1 = square index 4, g1 = square index 6
    std::cout << "\n\n=== Making move: e1 -> g1 (Kingside Castling) ===\n";
    Move castleKingside(BoardRepresentation::e1, BoardRepresentation::g1, Move::Flag::Castling);
    board.makeMove(castleKingside);
    
    printBoard(board);
    printBoardInfo(board);
    
    // Test queenside castling using FEN
    std::cout << "\n\n=== Castling Test - Queenside ===\n";
    board.init("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    
    printBoard(board);
    printBoardInfo(board);
    
    // Queenside castling (O-O-O): King e1 to c1
    // e1 = square index 4, c1 = square index 2
    std::cout << "\n\n=== Making move: e1 -> c1 (Queenside Castling) ===\n";
    Move castleQueenside(BoardRepresentation::e1, BoardRepresentation::c1, Move::Flag::Castling);
    board.makeMove(castleQueenside);
    
    printBoard(board);
    printBoardInfo(board);
    
    // Test pawn two-forward flag
    std::cout << "\n\n=== Pawn Two-Forward Flag Test ===\n";
    board.init();  // Reset to starting position
    
    std::cout << "\nInitial position:\n";
    printBoard(board);
    
    std::cout << "\n\n=== Making move: d2 -> d4 (two squares) ===\n";
    Move pawnDouble(BoardRepresentation::d2, BoardRepresentation::d4, Move::Flag::PawnTwoForward);
    board.makeMove(pawnDouble);
    
    printBoard(board);
    printBoardInfo(board);
    std::cout << "En passant square should be d3 (square index " << BoardRepresentation::d3 << "): " 
              << BoardRepresentation::SquareNameFromIndex(board.getEnPas()) << "\n";
    
    std::cout << "\n=== Test Complete ===\n";
    return 0;
}
