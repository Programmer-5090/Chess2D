import argparse
import shutil
import subprocess
import time
from pathlib import Path

from engine_probe import engine_perft_split, engine_perft


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


class StockfishSession:
    def __init__(self, stockfish_bin: str):
        self.proc = subprocess.Popen(
            [stockfish_bin],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        self._send("uci")
        self._read_until(lambda line: line.strip() == "uciok")
        self._isready()

    def _send(self, cmd: str) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def _read_until(self, predicate, timeout_sec: float = 60.0) -> list[str]:
        assert self.proc.stdout is not None
        lines = []
        start = time.monotonic()

        while True:
            if time.monotonic() - start > timeout_sec:
                raise TimeoutError(f"Timed out waiting for Stockfish output. Partial output:\n{''.join(lines)}")

            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError(f"Stockfish terminated unexpectedly. Output so far:\n{''.join(lines)}")

            lines.append(line)
            if predicate(line):
                return lines

    def _isready(self) -> None:
        self._send("isready")
        self._read_until(lambda line: line.strip() == "readyok")

    def perft_split(self, fen: str, depth: int) -> dict[str, int]:
        self._send(f"position fen {fen}")
        self._send(f"go perft {depth}")
        lines = self._read_until(lambda line: line.strip().lower().startswith("nodes searched"), timeout_sec=180.0)

        out: dict[str, int] = {}
        for raw in lines:
            line = raw.strip()
            if ":" not in line:
                continue
            left, right = line.split(":", 1)
            move = left.strip().replace(" ", "")
            value = right.strip().split()[0]
            if len(move) in (4, 5) and value.isdigit():
                out[move] = int(value)

        self._isready()
        return out

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self._send("quit")
            except Exception:
                pass
            self.proc.wait(timeout=2)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare engine split perft against Stockfish split perft.")
    parser.add_argument("depth", type=int, nargs="?", default=6, help="Perft depth (default: 6)")
    parser.add_argument(
        "--fen",
        type=str,
        default="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        help="FEN to test",
    )
    parser.add_argument("--show-all", action="store_true", help="Print all moves even if they match")
    parser.add_argument("--timeout", type=float, default=180.0, help="Engine timeout in seconds (default: 180)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    sf_bin = detect_stockfish()
    if not sf_bin:
        print("Stockfish not found. Put it in PATH or tests/stockfish.exe")
        return 1

    print("=== Split PERFT Compare ===")
    print(f"Stockfish: {sf_bin}")
    print(f"Depth: {args.depth}")
    print(f"FEN: {args.fen}")

    sf = StockfishSession(sf_bin)
    try:
        sf_split = sf.perft_split(args.fen, args.depth)
    finally:
        sf.close()

    engine_split = engine_perft_split(args.fen, args.depth, timeout_sec=args.timeout)
    engine_split.pop("total", None)

    all_moves = sorted(set(sf_split.keys()) | set(engine_split.keys()))

    mismatches = []
    for mv in all_moves:
        s = sf_split.get(mv)
        e = engine_split.get(mv)
        if s != e:
            mismatches.append((mv, e, s))

    if args.show_all:
        for mv in all_moves:
            print(f"{mv}: engine={engine_split.get(mv)} stockfish={sf_split.get(mv)}")
    else:
        for mv, e, s in mismatches:
            print(f"MISMATCH {mv}: engine={e} stockfish={s}")

    engine_total = sum(engine_split.values())
    sf_total = sum(sf_split.values())
    print(f"Engine total:    {engine_total}")
    print(f"Stockfish total: {sf_total}")

    if engine_total != sf_total:
        print(f"Total diff: {engine_total - sf_total}")

    if not mismatches and engine_total == sf_total:
        print("Result: MATCH")
        return 0

    missing_in_engine = sorted(set(sf_split.keys()) - set(engine_split.keys()))
    extra_in_engine = sorted(set(engine_split.keys()) - set(sf_split.keys()))

    if missing_in_engine:
        print("Missing moves in engine split:")
        for mv in missing_in_engine:
            print(f"  {mv}")

    if extra_in_engine:
        print("Extra moves in engine split:")
        for mv in extra_in_engine:
            print(f"  {mv}")

    print(f"Result: FAIL ({len(mismatches)} mismatched root moves)")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
