import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

from engine_probe import (
    ProbeTimeoutError,
    build_probe,
    engine_perft,
    engine_perft_mt,
    parse_partial_nodes,
)


BASE_FENS = [
    ("startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"),
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


class StockfishSession:
    def __init__(self, stockfish_bin: str, read_timeout_sec: float = 300.0):
        self.read_timeout_sec = read_timeout_sec
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

    def _read_until(self, predicate, timeout_sec: float | None = None) -> list[str]:
        assert self.proc.stdout is not None
        lines = []
        start = time.monotonic()
        effective_timeout = self.read_timeout_sec if timeout_sec is None else timeout_sec

        while True:
            if time.monotonic() - start > effective_timeout:
                raise TimeoutError(f"Timed out waiting for Stockfish output. Partial output:\n{''.join(lines)}")

            line = self.proc.stdout.readline()
            if not line:
                raise RuntimeError(f"Stockfish terminated unexpectedly. Output so far:\n{''.join(lines)}")

            lines.append(line)
            if predicate(line):
                return lines

    def _isready(self) -> None:
        self._send("isready")
        self._read_until(lambda line: line.strip() == "readyok", timeout_sec=max(30.0, min(self.read_timeout_sec, 120.0)))

    @staticmethod
    def _parse_nodes(lines: list[str]) -> int:
        nodes = None
        for raw in lines:
            line = raw.strip()
            if line.lower().startswith("nodes searched"):
                parts = line.replace(":", " ").split()
                for token in reversed(parts):
                    if token.isdigit():
                        nodes = int(token)
                        break
        if nodes is None:
            raise RuntimeError("Could not parse 'Nodes searched' from Stockfish output.\n" + "".join(lines))
        return nodes

    def perft(self, fen: str, depth: int) -> int:
        self._send(f"position fen {fen}")
        self._send(f"go perft {depth}")
        lines = self._read_until(lambda line: line.strip().lower().startswith("nodes searched"))
        nodes = self._parse_nodes(lines)
        self._isready()
        return nodes

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self._send("quit")
            except Exception:
                pass
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                try:
                    self.proc.wait(timeout=2)
                except Exception:
                    pass


def time_call(fn):
    t0 = time.perf_counter()
    value = fn()
    t1 = time.perf_counter()
    return value, (t1 - t0)


def parse_depth_tokens(tokens: list[str]) -> list[int]:
    if not tokens:
        return [1, 2, 3]

    depths: set[int] = set()

    for token in tokens:
        token = token.strip()
        if not token:
            continue

        # Range: a-b (inclusive)
        if "-" in token:
            parts = token.split("-", 1)
            if len(parts) != 2 or not parts[0].strip().isdigit() or not parts[1].strip().isdigit():
                raise ValueError(f"Invalid depth range: '{token}'. Use format a-b, e.g. 1-5")

            start = int(parts[0].strip())
            end = int(parts[1].strip())
            if start < 1 or end < 1:
                raise ValueError(f"Depths must be >= 1: '{token}'")
            if start > end:
                raise ValueError(f"Range start must be <= end: '{token}'")

            for d in range(start, end + 1):
                depths.add(d)
        else:
            if not token.isdigit():
                raise ValueError(f"Invalid depth: '{token}'")
            d = int(token)
            if d < 1:
                raise ValueError(f"Depth must be >= 1: '{token}'")
            depths.add(d)

    if not depths:
        return [1, 2, 3]

    return sorted(depths)


def _normalize_cli_args(argv: list[str]) -> list[str]:
    normalized: list[str] = []
    for token in argv:
        if token.startswith("--fen") and token != "--fen" and not token.startswith("--fen="):
            fen_value = token[len("--fen"):]
            if fen_value:
                normalized.append("--fen")
                normalized.append(fen_value)
                continue
        normalized.append(token)
    return normalized


def parse_args():
    parser = argparse.ArgumentParser(description="Compare engine perft against Stockfish.")
    parser.add_argument(
        "depths",
        nargs="*",
        type=str,
        help="Depths and/or ranges. Examples: 1 2 3, 1-5, 1-3 5 7",
    )
    parser.add_argument(
        "--fen",
        type=str,
        default="",
        help="Optional custom FEN to test instead of built-in test positions.",
    )
    parser.add_argument(
        "--threads",
        "-t",
        type=int,
        default=1,
        help="Engine thread count for root-split perft (default: 1).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=0.0,
        help="Per-depth timeout in seconds for engine perft (0 = no timeout).",
    )
    parser.add_argument(
        "--sf-timeout",
        type=float,
        default=300.0,
        help="Timeout in seconds for Stockfish per-depth response (default: 300).",
    )
    return parser.parse_args(_normalize_cli_args(sys.argv[1:]))


def main() -> int:
    print("=== Engine PERFT vs Stockfish ===")

    args = parse_args()
    try:
        depths = parse_depth_tokens(args.depths)
    except ValueError as e:
        print(f"Depth parse error: {e}")
        return 1

    if args.threads < 1:
        print("Thread count must be >= 1")
        return 1

    if args.timeout < 0:
        print("Timeout must be >= 0")
        return 1

    if args.sf_timeout <= 0:
        print("Stockfish timeout must be > 0")
        return 1

    timeout_sec = args.timeout if args.timeout > 0 else None

    sf = detect_stockfish()
    if not sf:
        print("Stockfish not found. Put it in PATH or tests/stockfish.exe")
        return 1

    # build once before timing
    build_probe()

    failures = 0
    print(f"Using Stockfish: {sf}")
    print(f"Depths: {', '.join(str(d) for d in depths)}")
    print(f"Engine threads: {args.threads}")
    if timeout_sec is not None:
        print(f"Engine timeout per depth: {timeout_sec:.1f}s")
    print(f"Stockfish timeout per depth: {args.sf_timeout:.1f}s")

    test_fens = BASE_FENS if not args.fen else [("custom", args.fen)]

    sf_session = StockfishSession(sf, read_timeout_sec=args.sf_timeout)
    try:
        for name, fen in test_fens:
            for depth in depths:
                print(f"\n{name} depth {depth}")

                # Always get reference node count from Stockfish.
                try:
                    sf_nodes, sf_sec = time_call(lambda: sf_session.perft(fen, depth))
                except TimeoutError as e:
                    print(f"  stockfish: TIMEOUT after {args.sf_timeout:.1f}s")
                    print("  correctness: FAIL (stockfish timeout)")
                    failures += 1
                    continue

                timed_out = False
                engine_partial = None

                try:
                    if args.threads > 1:
                        engine_nodes, engine_sec = time_call(
                            lambda: engine_perft_mt(fen, depth, args.threads, timeout_sec=timeout_sec)
                        )
                    else:
                        engine_nodes, engine_sec = time_call(
                            lambda: engine_perft(fen, depth, timeout_sec=timeout_sec)
                        )
                except ProbeTimeoutError as e:
                    timed_out = True
                    engine_sec = timeout_sec if timeout_sec is not None else 0.0
                    engine_nodes = None
                    engine_partial = parse_partial_nodes(e.partial_output)

                if timed_out:
                    if engine_partial is not None:
                        print(f"  engine:    TIMEOUT after {engine_sec:.1f}s (partial nodes: {engine_partial})")
                    else:
                        print(f"  engine:    TIMEOUT after {engine_sec:.1f}s (no partial nodes reported)")
                    print(f"  stockfish: {sf_nodes} nodes in {sf_sec:.4f}s")
                    print("  correctness: FAIL (timeout)")
                    failures += 1
                    continue

                ok = engine_nodes == sf_nodes
                print(f"  engine:    {engine_nodes} nodes in {engine_sec:.4f}s")
                print(f"  stockfish: {sf_nodes} nodes in {sf_sec:.4f}s")

                if sf_sec > 0:
                    print(f"  speed ratio (engine/stockfish): {engine_sec / sf_sec:.2f}x")

                print(f"  correctness: {'OK' if ok else 'FAIL'}")
                if not ok:
                    failures += 1
    finally:
        sf_session.close()

    print("=== Done ===")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
