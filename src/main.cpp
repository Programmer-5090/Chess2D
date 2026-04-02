#include "game_screen.h"

#include <iostream>
#include <string>

// Usage:
//   ./chess                          – start from the default position
//   ./chess "<FEN string>"           – start from a custom FEN position
//
// In-game keyboard shortcuts:
//   R  – reset to starting position
//   U  – undo the last move
//   F  – flip the board (swap white / black perspective)
//   ESC – quit
//   1-6 to change thems and textures

int main(int argc, char* argv[]) {
    // Optional FEN supplied on the command line.
    const std::string fen = (argc > 1) ? argv[1] : "";

    if (!fen.empty()) {
        std::cout << "[main] Loading FEN: " << fen << '\n';
    }

    Chess::GameWindow game(600, 600);
    game.initializeGame(fen);
    game.run();

    return 0;
}