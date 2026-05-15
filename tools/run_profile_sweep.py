"""
Drive the engine in headless profile mode for several wave settings, then plot.

Runs:
    bin/<config>-game.exe --profile-wave N --profile-seconds S --profile-out profile
for each wave in --waves, then invokes tools/plot_profile.py to compare them.

Usage (from repo root):
    python tools/run_profile_sweep.py --waves 1 5 10 --seconds 10
    python tools/run_profile_sweep.py --waves 1 5 10 --seconds 10 --config release
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--waves",   nargs="+", type=int, default=[1, 5, 10])
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--out",     default="profile")
    parser.add_argument("--config",  choices=["debug", "release"], default="debug")
    parser.add_argument("--exe",     default=None,
                        help="Override path to game executable (defaults to bin/<config>-game.exe).")
    args = parser.parse_args()

    out_dir = (repo_root / args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    exe = Path(args.exe) if args.exe else repo_root / "bin" / f"{args.config}-game.exe"
    if not exe.is_file():
        print(f"[run_profile_sweep] executable not found: {exe}", file=sys.stderr)
        print(f"[run_profile_sweep] build first (e.g. MSBuild AdventureEngine.sln /p:Configuration={args.config})",
              file=sys.stderr)
        return 1

    for wave in args.waves:
        cmd = [str(exe),
               "--profile-wave", str(wave),
               "--profile-seconds", str(args.seconds),
               "--profile-out", str(out_dir)]
        print(f"[run_profile_sweep] $ {' '.join(cmd)}")
        proc = subprocess.run(cmd, cwd=str(repo_root))
        if proc.returncode != 0:
            print(f"[run_profile_sweep] wave {wave} exited with {proc.returncode}", file=sys.stderr)
            return proc.returncode

    inputs = [str(out_dir / f"wave{w}.csv") for w in args.waves]
    plot_cmd = [sys.executable, str(repo_root / "tools" / "plot_profile.py"),
                "--input", *inputs,
                "--out", str(out_dir),
                "--baseline-wave", str(min(args.waves))]
    print(f"[run_profile_sweep] $ {' '.join(plot_cmd)}")
    return subprocess.run(plot_cmd, cwd=str(repo_root)).returncode


if __name__ == "__main__":
    raise SystemExit(main())
