#include "piece_list.h"

namespace Chess {

    int PieceList::count() const {
        return static_cast<int>(squares.size());
    }

    void PieceList::add(int square) {
        squares.push_back(square);
    }

    void PieceList::remove(int square) {
        auto it = std::find(squares.begin(), squares.end(), square);
        if (it != squares.end()) {
            squares.erase(it);
        }
    }

    void PieceList::move(int fromSquare, int toSquare) {
        auto it = std::find(squares.begin(), squares.end(), fromSquare);
        if (it != squares.end()) {
            *it = toSquare;
        }
    }

    bool PieceList::contains(int square) const {
        auto it = std::find(squares.begin(), squares.end(), square);
        if (it != squares.end()) {
            return true;
        }
        return false;
    }

    void PieceList::clear() {
        squares.clear();
    }

    const std::vector<int>& PieceList::getSquares() const {
        return squares;
    }

    int PieceList::operator[](int index) const {
        return squares[index];
    }

}  // namespace Chess
