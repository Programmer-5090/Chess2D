#ifndef FEN_UTIL_H
#define FEN_UTIL_H

#include "pieces.h"
#include "bitboard_util.h"
#include "board_rep.h"
#include "board_state.h"

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>

namespace Chess {

inline void loadFENUtil(BoardState& board, const std::string& fen) {
    board.init();

    std::istringstream ss(fen);
    std::string position, turn, castling, enPassant;
    int halfmove, fullmove;

    ss >> position >> turn >> castling >> enPassant >> halfmove >> fullmove;

    auto& pieceBoards = board.getPieceBoards();
    
    // Clear all piece boards
    for (int c = 0; c < 2; ++c) {
        for (int t = 0; t < 6; ++t) {
            pieceBoards[c][t] = 0;
        }
    }

    // FEN format: rank 8 (a8-h8) down to rank 1 (a1-h1)
    int rank = 7, file = 0;
    for (char c : position) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (std::isdigit(c)) {
            file += (c - '0');
        } else {
            int sq = BoardRepresentation::IndexFromCoord(file, rank);
            int pieceType = -1;
            int color = std::isupper(c) ? COLOR_WHITE : COLOR_BLACK;

            char lower = std::tolower(c);
            switch (lower) {
            case 'p': pieceType = PIECE_PAWN; break;
            case 'n': pieceType = PIECE_KNIGHT; break;
            case 'b': pieceType = PIECE_BISHOP; break;
            case 'r': pieceType = PIECE_ROOK; break;
            case 'q': pieceType = PIECE_QUEEN; break;
            case 'k': pieceType = PIECE_KING; break;
            }

            if (pieceType != -1) {
                uint64_t sqMask = (1ULL << sq);
                pieceBoards[color][pieceType] |= sqMask;
            }

            file++;
        }
    }

    board.setSide((turn == "w") ? COLOR_WHITE : COLOR_BLACK);
    board.setEnPas(-1);

    // Parse castling rights: KQkq format
    int cr = 0;
    if (castling != "-") {
        if (castling.find('K') != std::string::npos) cr |= 0x01;  // White kingside
        if (castling.find('Q') != std::string::npos) cr |= 0x02;  // White queenside
        if (castling.find('k') != std::string::npos) cr |= 0x04;  // Black kingside
        if (castling.find('q') != std::string::npos) cr |= 0x08;  // Black queenside
    }
    board.setCastleRights(cr);

    if (enPassant != "-" && enPassant.length() >= 2) {
        int epFile = enPassant[0] - 'a';
        int epRank = (enPassant[1] - '1');
        board.setEnPas(BoardRepresentation::IndexFromCoord(epFile, epRank));
    }

    board.setFiftyMove(halfmove);
    board.setHisPly(fullmove * 2 + (turn == "b" ? 1 : 0));

    board.rebuildOccupancy();
    board.rebuildMailbox();
    board.rebuildPieceLists();
    board.setPosKey(board.generatePosKey());
}

inline std::string toFENUtil(const BoardState& board) {
    std::ostringstream ss;

    const auto& pieceBoards = board.getPieceBoards();

    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            int sq = BoardRepresentation::IndexFromCoord(file, rank);
            uint64_t sqMask = (1ULL << sq);

            int pieceType = -1;
            int color = -1;

            // Find which piece is on this square
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
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    ss << emptyCount;
                    emptyCount = 0;
                }

                char pieceChar = ' ';
                switch (pieceType) {
                case PIECE_PAWN:   pieceChar = 'p'; break;
                case PIECE_KNIGHT: pieceChar = 'n'; break;
                case PIECE_BISHOP: pieceChar = 'b'; break;
                case PIECE_ROOK:   pieceChar = 'r'; break;
                case PIECE_QUEEN:  pieceChar = 'q'; break;
                case PIECE_KING:   pieceChar = 'k'; break;
                }

                if (color == COLOR_WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }

                ss << pieceChar;
            }
        }

        if (emptyCount > 0) {
            ss << emptyCount;
        }

        if (rank > 0) {
            ss << '/';
        }
    }

    ss << ' ' << (board.getSide() == COLOR_WHITE ? 'w' : 'b') << ' ';

    int cr = board.getCastleRights();
    if (cr == 0) {
        ss << "- ";
    } else {
        if (cr & 0x01) ss << 'K';
        if (cr & 0x02) ss << 'Q';
        if (cr & 0x04) ss << 'k';
        if (cr & 0x08) ss << 'q';
        ss << ' ';
    }

    int ep = board.getEnPas();
    if (ep >= 0 && ep < 64) {
        auto coord = BoardRepresentation::CoordFromIndex(ep);
        ss << static_cast<char>('a' + coord.fileIndex) << static_cast<char>('1' + coord.rankIndex) << ' ';
    } else {
        ss << "- ";
    }

    ss << board.getFiftyMove() << ' ' << (board.getHisPly() / 2 + 1);

    return ss.str();
}

}  // namespace Chess

#endif  // FEN_UTIL_H