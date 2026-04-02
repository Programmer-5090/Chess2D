#ifndef BOARD_REPRESENTATION
#define BOARD_REPRESENTATION

#include <string>
#include <string_view>

namespace Chess
{
    struct Coord {
        int fileIndex = 0;
        int rankIndex = 0;

        constexpr Coord() = default;
        constexpr Coord(int fileIndex, int rankIndex) : fileIndex(fileIndex), rankIndex(rankIndex) {}

        constexpr bool isLightSq() const {
            return (fileIndex + rankIndex) % 2 != 0;
        }

        constexpr bool isTheSame(const Coord& other) const {
            return (fileIndex == other.fileIndex) && (rankIndex == other.rankIndex);
        }
    };

    class BoardRepresentation
    {
    public:
        static constexpr std::string_view fileNames = "abcdefgh";
        static constexpr std::string_view rankNames = "12345678";

        // Rank 1 (White's back rank)
        static constexpr int a1 = 0;
        static constexpr int b1 = 1;
        static constexpr int c1 = 2;
        static constexpr int d1 = 3;
        static constexpr int e1 = 4;
        static constexpr int f1 = 5;
        static constexpr int g1 = 6;
        static constexpr int h1 = 7;

        // Rank 2
        static constexpr int a2 = 8;
        static constexpr int b2 = 9;
        static constexpr int c2 = 10;
        static constexpr int d2 = 11;
        static constexpr int e2 = 12;
        static constexpr int f2 = 13;
        static constexpr int g2 = 14;
        static constexpr int h2 = 15;

        // Rank 3
        static constexpr int a3 = 16;
        static constexpr int b3 = 17;
        static constexpr int c3 = 18;
        static constexpr int d3 = 19;
        static constexpr int e3 = 20;
        static constexpr int f3 = 21;
        static constexpr int g3 = 22;
        static constexpr int h3 = 23;

        // Rank 4
        static constexpr int a4 = 24;
        static constexpr int b4 = 25;
        static constexpr int c4 = 26;
        static constexpr int d4 = 27;
        static constexpr int e4 = 28;
        static constexpr int f4 = 29;
        static constexpr int g4 = 30;
        static constexpr int h4 = 31;

        // Rank 5
        static constexpr int a5 = 32;
        static constexpr int b5 = 33;
        static constexpr int c5 = 34;
        static constexpr int d5 = 35;
        static constexpr int e5 = 36;
        static constexpr int f5 = 37;
        static constexpr int g5 = 38;
        static constexpr int h5 = 39;

        // Rank 6
        static constexpr int a6 = 40;
        static constexpr int b6 = 41;
        static constexpr int c6 = 42;
        static constexpr int d6 = 43;
        static constexpr int e6 = 44;
        static constexpr int f6 = 45;
        static constexpr int g6 = 46;
        static constexpr int h6 = 47;

        // Rank 7
        static constexpr int a7 = 48;
        static constexpr int b7 = 49;
        static constexpr int c7 = 50;
        static constexpr int d7 = 51;
        static constexpr int e7 = 52;
        static constexpr int f7 = 53;
        static constexpr int g7 = 54;
        static constexpr int h7 = 55;

        // Rank 8 (Black's back rank)
        static constexpr int a8 = 56;
        static constexpr int b8 = 57;
        static constexpr int c8 = 58;
        static constexpr int d8 = 59;
        static constexpr int e8 = 60;
        static constexpr int f8 = 61;
        static constexpr int g8 = 62;
        static constexpr int h8 = 63;

        static constexpr int RankIndex(int squareIndex)
        {
            return squareIndex >> 3;
        }

        static constexpr int FileIndex(int squareIndex)
        {
            return squareIndex & 0b000111;
        }

        static constexpr int IndexFromCoord(int fileIndex, int rankIndex)
        {
            return rankIndex * 8 + fileIndex;
        }

        static constexpr int IndexFromCoord(const Coord& coord)
        {
            return IndexFromCoord(coord.fileIndex, coord.rankIndex);
        }

        static constexpr Coord CoordFromIndex(int squareIndex)
        {
            return Coord(FileIndex(squareIndex), RankIndex(squareIndex));
        }

        static constexpr bool LightSquare(int fileIndex, int rankIndex)
        {
            return ((fileIndex + rankIndex) % 2) != 0;
        }

        static inline std::string SquareNameFromCoordinate(int fileIndex, int rankIndex)
        {
            return std::string(1, fileNames[static_cast<std::size_t>(fileIndex)]) + std::to_string(rankIndex + 1);
        }

        static inline std::string SquareNameFromCoordinate(const Coord& coord)
        {
            return SquareNameFromCoordinate(coord.fileIndex, coord.rankIndex);
        }

        static inline std::string SquareNameFromIndex(int squareIndex)
        {
            return SquareNameFromCoordinate(CoordFromIndex(squareIndex));
        }
    };
}

#endif