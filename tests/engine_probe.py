import json
import re
import shutil
import subprocess
import tempfile
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

HARNESS_CPP = r'''

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "chess_engine/board_state.h"
#include "chess_engine/move_generator.h"
#include "chess_engine/fen_util.h"
#include "chess_engine/board_rep.h"
#include "thread_pool.h"
#include "logger.h"

using namespace Chess;

std::string toUci(const Move& m) {
    std::string out = BoardRepresentation::SquareNameFromIndex(m.startSquare()) +
                      BoardRepresentation::SquareNameFromIndex(m.targetSquare());

    if (m.isPromotion()) {
        switch (m.flag()) {
            case Move::Flag::PromoteToQueen: out.push_back('q'); break;
            case Move::Flag::PromoteToRook: out.push_back('r'); break;
            case Move::Flag::PromoteToBishop: out.push_back('b'); break;
            case Move::Flag::PromoteToKnight: out.push_back('n'); break;
            default: break;
        }
    }
    return out;
}

uint64_t perft(BoardState& board, int depth, MoveGenerator& gen) {
    if (depth == 0) return 1;

    gen.generateLegalMoves(board, true);
    const int count = gen.getLegalMoveCount();

    if (depth == 1) return static_cast<uint64_t>(count);

    // Preserve move list for this ply; recursive calls overwrite generator state.
    std::vector<Move> plyMoves;
    plyMoves.reserve(count);
    for (int i = 0; i < count; ++i) {
        plyMoves.push_back(gen.moveList[i]);
    }

    uint64_t nodes = 0;
    for (const Move& m : plyMoves) {
        board.makeMove(m);
        nodes += perft(board, depth - 1, gen);
        board.unmakeMove();
    }
    return nodes;
}

uint64_t perft_mt(BoardState& board, int depth, int threadCount) {
    if (depth <= 1 || threadCount <= 1) {
        MoveGenerator gen;
        gen.init();
        return perft(board, depth, gen);
    }

    MoveGenerator gen;
    gen.init();
    gen.generateLegalMoves(board, true);
    const int rootCount = gen.getLegalMoveCount();

    if (depth == 1) return static_cast<uint64_t>(rootCount);

    std::vector<Move> rootMoves;
    rootMoves.reserve(rootCount);
    for (int i = 0; i < rootCount; ++i) {
        rootMoves.push_back(gen.moveList[i]);
    }

    ThreadPool pool(threadCount);
    std::vector<std::future<uint64_t>> futures;

    // For deeper searches, split by first two plies to reduce stragglers.
    if (depth >= 3) {
        for (const Move& m1 : rootMoves) {
            BoardState afterFirst = board;
            afterFirst.makeMove(m1);

            MoveGenerator gen2;
            gen2.init();
            gen2.generateLegalMoves(afterFirst, true);
            const int childCount = gen2.getLegalMoveCount();

            std::vector<Move> childMoves;
            childMoves.reserve(childCount);
            for (int j = 0; j < childCount; ++j) {
                childMoves.push_back(gen2.moveList[j]);
            }

            for (const Move& m2 : childMoves) {
                futures.emplace_back(pool.enqueue([afterFirst, m2, depth]() mutable -> uint64_t {
                    BoardState local = afterFirst;
                    local.makeMove(m2);
                    MoveGenerator localGen;
                    localGen.init();
                    return perft(local, depth - 2, localGen);
                }));
            }
        }
    } else {
        // depth == 2 fallback: root split is enough
        futures.reserve(rootMoves.size());
        for (const Move& m : rootMoves) {
            futures.emplace_back(pool.enqueue([board, m, depth]() mutable -> uint64_t {
                BoardState local = board;
                local.makeMove(m);
                MoveGenerator localGen;
                localGen.init();
                return perft(local, depth - 1, localGen);
            }));
        }
    }

    uint64_t nodes = 0;
    std::vector<char> done(futures.size(), 0);
    size_t completed = 0;

    while (completed < futures.size()) {
        bool progressed = false;
        for (size_t i = 0; i < futures.size(); ++i) {
            if (done[i]) continue;
            if (futures[i].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                nodes += futures[i].get();
                done[i] = 1;
                ++completed;
                progressed = true;
                std::cout << "progress " << completed << "/" << futures.size()
                          << " nodes " << nodes << "\n" << std::flush;
            }
        }
        if (!progressed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return nodes;
}

void perft_split(BoardState& board, int depth) {
    MoveGenerator gen;
    gen.init();
    gen.generateLegalMoves(board, true);
    const int rootCount = gen.getLegalMoveCount();

    std::vector<Move> rootMoves;
    rootMoves.reserve(rootCount);
    for (int i = 0; i < rootCount; ++i) {
        rootMoves.push_back(gen.moveList[i]);
    }

    uint64_t total = 0;
    for (const Move& m : rootMoves) {
        board.makeMove(m);
        MoveGenerator localGen;
        localGen.init();
        const uint64_t nodes = perft(board, depth - 1, localGen);
        board.unmakeMove();
        total += nodes;
        std::cout << toUci(m) << " " << nodes << "\n";
    }

    std::cout << "total " << total << "\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: engine_probe <moves|perft|perft_mt|perft_split> <fen> [depth] [threads]\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string fen = argv[2];

    BoardState board;
    board.init(fen);

    MoveGenerator gen;
    gen.init();

    // Ensure project-wide logger does not redirect std::cout/cerr for the probe
    Logger::setSilent(true);

    if (mode == "moves") {
        gen.generateLegalMoves(board, true);
        std::vector<std::string> moves;
        moves.reserve(gen.getLegalMoveCount());
        for (int i = 0; i < gen.getLegalMoveCount(); ++i) {
            moves.push_back(toUci(gen.moveList[i]));
        }
        std::sort(moves.begin(), moves.end());

        for (const auto& m : moves) {
            std::cout << m << "\n";
        }
        return 0;
    }

    if (mode == "perft") {
        if (argc < 4) {
            std::cerr << "perft mode requires depth\n";
            return 1;
        }
        int depth = std::stoi(argv[3]);
        std::cout << perft(board, depth, gen) << "\n";
        return 0;
    }

    if (mode == "perft_split") {
        if (argc < 4) {
            std::cerr << "perft_split mode requires depth\n";
            return 1;
        }
        int depth = std::stoi(argv[3]);
        perft_split(board, depth);
        return 0;
    }

    if (mode == "perft_mt") {
        if (argc < 5) {
            std::cerr << "perft_mt mode requires depth and thread count\n";
            return 1;
        }
        int depth = std::stoi(argv[3]);
        int threads = std::stoi(argv[4]);
        std::cout << perft_mt(board, depth, threads) << "\n";
        return 0;
    }

    std::cerr << "unknown mode\n";
    return 1;
}
'''


def _find_compiler() -> str:
    for candidate in ("g++", "clang++"):
        if shutil.which(candidate):
            return candidate
    raise RuntimeError("No C++ compiler found (expected g++ or clang++ in PATH).")


@lru_cache(maxsize=1)
def build_probe() -> Path:
    # Build the probe using CMake so it uses the project's include layout and flags.
    temp_dir = Path(tempfile.gettempdir()) / "chess_cpp_probe"
    build_dir = temp_dir / "build"
    temp_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    cpp_file = temp_dir / "engine_probe_tmp.cpp"
    exe_name = "engine_probe_tmp.exe" if shutil.which("where") else "engine_probe_tmp"
    exe_file = build_dir / exe_name

    cpp_file.write_text(HARNESS_CPP, encoding="utf-8")

    # Write a minimal CMakeLists.txt that builds the probe executable and uses the repo include dirs
    cmakelists = f'''cmake_minimum_required(VERSION 3.8)
project(engine_probe_tmp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "{(ROOT / 'src' / 'chess_engine').as_posix()}/*.cpp"
    "{(ROOT / 'utils/logger.cpp').as_posix()}"
)

# Exclude any internal test/main files to avoid duplicate `main` symbols when linking.
list(REMOVE_ITEM PROJECT_SOURCES "{(ROOT / 'src' / 'chess_engine' / 'main.cpp').as_posix()}")

add_executable(engine_probe_tmp
    engine_probe_tmp.cpp
    ${{PROJECT_SOURCES}}
)

target_include_directories(engine_probe_tmp PRIVATE
    "{(ROOT / 'include').as_posix()}"
    "{(ROOT / 'include/chess_engine').as_posix()}"
    "{(ROOT / 'utils/include').as_posix()}"
)

# Enable link-time optimization (LTO) / IPO when supported for the probe
set_property(TARGET engine_probe_tmp PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
'''
    (temp_dir / 'CMakeLists.txt').write_text(cmakelists, encoding='utf-8')

    # Configure with CMake
    cmake_cmd = [shutil.which('cmake') or 'cmake', '-S', temp_dir.as_posix(), '-B', build_dir.as_posix(), '-DCMAKE_BUILD_TYPE=Release']
    # Prefer Ninja generator if available
    if shutil.which('ninja'):
        cmake_cmd.extend(['-G', 'Ninja'])

    result = subprocess.run(cmake_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"CMake configure failed:\n{result.stderr}\n{result.stdout}")

    # Build with CMake
    build_cmd = [shutil.which('cmake') or 'cmake', '--build', build_dir.as_posix(), '--config', 'Release', '--target', 'engine_probe_tmp']
    result = subprocess.run(build_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"CMake build failed:\n{result.stderr}\n{result.stdout}")

    # Locate the executable (handle common output locations)
    possible_paths = [
        build_dir / exe_name,
        build_dir / 'Release' / exe_name,
        build_dir / 'engine_probe_tmp' if exe_name.endswith('.exe') else build_dir / 'engine_probe_tmp',
    ]
    for p in possible_paths:
        if p.exists():
            return p

    # As a fallback, raise with diagnostic output
    raise RuntimeError(f"Probe build succeeded but executable not found. Looked for: {possible_paths}")


class ProbeTimeoutError(RuntimeError):
    def __init__(self, message: str, partial_output: str = ""):
        super().__init__(message)
        self.partial_output = partial_output or ""


def _parse_nodes_from_output(output: str) -> int:
    # Preferred: final line is a raw integer.
    for line in reversed([ln.strip() for ln in output.splitlines() if ln.strip()]):
        if line.isdigit():
            return int(line)

    # Fallback: parse last progress line: "progress X/Y nodes N"
    matches = re.findall(r"\bnodes\s+(\d+)\b", output)
    if matches:
        return int(matches[-1])

    raise RuntimeError(f"Could not parse nodes from probe output:\n{output}")


def _run_probe(args: list[str], timeout_sec: float | None = None) -> str:
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=timeout_sec)
    except subprocess.TimeoutExpired as e:
        partial = (e.stdout or "") + (e.stderr or "")
        raise ProbeTimeoutError("Probe timed out", partial_output=partial)

    if result.returncode != 0:
        raise RuntimeError(f"Probe run failed:\n{result.stderr}\n{result.stdout}")

    return result.stdout


def engine_moves(fen: str) -> list[str]:
    exe = build_probe()
    stdout = _run_probe([exe.as_posix(), "moves", fen])
    return [line.strip() for line in stdout.splitlines() if line.strip()]


def engine_perft(fen: str, depth: int, timeout_sec: float | None = None) -> int:
    exe = build_probe()
    stdout = _run_probe([exe.as_posix(), "perft", fen, str(depth)], timeout_sec=timeout_sec)
    return _parse_nodes_from_output(stdout)


def engine_perft_mt(fen: str, depth: int, threads: int, timeout_sec: float | None = None) -> int:
    exe = build_probe()
    stdout = _run_probe(
        [exe.as_posix(), "perft_mt", fen, str(depth), str(threads)],
        timeout_sec=timeout_sec,
    )
    return _parse_nodes_from_output(stdout)


def engine_perft_split(fen: str, depth: int, timeout_sec: float | None = None) -> dict[str, int]:
    exe = build_probe()
    stdout = _run_probe([exe.as_posix(), "perft_split", fen, str(depth)], timeout_sec=timeout_sec)
    out: dict[str, int] = {}
    for raw in stdout.splitlines():
        line = raw.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            continue
        move, nodes = parts
        if nodes.isdigit():
            out[move] = int(nodes)
    return out


def parse_partial_nodes(output: str) -> int | None:
    try:
        return _parse_nodes_from_output(output)
    except Exception:
        return None
