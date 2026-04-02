import shutil
import subprocess
from pathlib import Path

from engine_probe import engine_moves


FENS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
]


def detect_stockfish() -> str:
    candidates = [
        "stockfish.exe",
        "stockfish-windows-x86-64-avx2.exe",
        "tests/stockfish.exe",
        "tests/stockfish-windows-x86-64-avx2.exe",
    ]
    for c in candidates:
        if Path(c).exists() or shutil.which(c):
            return c
    return ""


def stockfish_moves(stockfish_bin: str, fen: str) -> list[str]:
    cmd = f'(echo uci& echo isready& echo position fen {fen}& echo go perft 1& echo quit) | "{stockfish_bin}"'
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)

    moves = []
    for line in result.stdout.splitlines():
        if ":" not in line:
            continue
        mv = line.split(":", 1)[0].strip().replace(" ", "")
        if len(mv) in (4, 5):
            moves.append(mv)
    return sorted(set(moves))


def main() -> int:
    sf = detect_stockfish()
    if not sf:
        print("Stockfish not found. Put it in PATH or tests/stockfish.exe")
        return 1

    print(f"Using Stockfish: {sf}")
    failed = 0

    for fen in FENS:
        print("\n========================================")
        print(f"FEN: {fen}")
        sf_set = set(stockfish_moves(sf, fen))
        my_set = set(engine_moves(fen))

        missing = sorted(sf_set - my_set)
        extra = sorted(my_set - sf_set)

        print(f"Stockfish moves: {len(sf_set)}")
        print(f"Engine moves:    {len(my_set)}")

        if not missing and not extra:
            print("MATCH")
            continue

        failed += 1
        print("MISMATCH")
        if missing:
            print("  Missing:")
            for m in missing:
                print(f"    {m}")
        if extra:
            print("  Extra:")
            for m in extra:
                print(f"    {m}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
