#!/usr/bin/env python3
"""
Compare per-frame CPU profile captures from the engine and produce a
matplotlib chart that highlights how Threading (AI), Physics, and Animation
costs scale across waves.

Each CSV is produced by the in-engine profiler (press F2 to start a 600-frame
capture; the file is written to profile/wave<N>.csv when the capture window
closes or the game exits).

CSV schema (header row):
    frame,wave,total_ms,<section_1>,<section_2>,...

Typical sections emitted by the engine today:
    Physics, Animation_System, Animation, AI_Threading, Movement, PlayerInput,
    Combat, Health, Despawn, Physics_Raycast, Render_Submit, Renderer_DrawFrame,
    Scene::Update

Usage:
    python tools/plot_profile.py \
        --input profile/wave1.csv profile/wave5.csv profile/wave10.csv \
        --out   profile \
        --baseline-wave 1

If --input is omitted, the script auto-discovers profile/wave*.csv.
"""
from __future__ import annotations

import argparse
import csv
import glob
import os
import re
import sys
from dataclasses import dataclass
from typing import Dict, List

import matplotlib
matplotlib.use("Agg")  # headless safe
import matplotlib.pyplot as plt
import numpy as np


# Sections we report on (grouped) and the columns from the CSV that feed them.
# A section is the sum (per frame) of all listed columns that exist in the CSV.

# 'Renderer (parallelizable)' is the CPU work in Renderer_DrawFrame that can
# realistically be moved off the main thread (drawcall sorting + command
# buffer recording). Vulkan is the obvious target here because its API was
# explicitly designed for multi-threaded command recording (one command pool
# + secondary command buffers per worker).

# 'Renderer (serial)' is the work that is NOT trivially parallelizable
# (queue submit, present, GPU fence wait). Useful as a reference baseline.
SECTION_GROUPS: Dict[str, List[str]] = {
    "Renderer (parallelizable)": ["Renderer_DrawCallSort", "Renderer_RecordCmds"],
    "Renderer (serial)":         ["Renderer_QueueSubmit", "Renderer_Present", "Renderer_FenceWait"],
    "Physics":                   ["Physics", "Physics_Raycast"],
    "Animation":                 ["Animation", "Animation_System"],
    "EnemyAI":                   ["AI_Threading"],
    "Render submit (game)":      ["Render_Submit"],
    "Other systems":             ["Movement", "PlayerInput", "Combat", "Health", "Despawn"],
}


@dataclass
class WaveSample:
    wave: int
    path: str
    frames: int
    total_ms: np.ndarray              # per-frame total frame time
    section_ms: Dict[str, np.ndarray] # per-frame ms per SECTION_GROUPS key


def _read_csv(path: str) -> WaveSample:
    with open(path, "r", newline="") as fh:
        reader = csv.reader(fh)
        header = next(reader)
        rows = [r for r in reader if r]
    if not rows:
        raise RuntimeError(f"{path}: no data rows")

    col_index = {name: i for i, name in enumerate(header)}
    if "wave" not in col_index or "total_ms" not in col_index:
        raise RuntimeError(f"{path}: missing required columns")

    wave_vals  = [int(r[col_index["wave"]]) for r in rows]
    total_vals = np.asarray([float(r[col_index["total_ms"]]) for r in rows])
    wave = int(round(np.median(wave_vals)))

    section_ms: Dict[str, np.ndarray] = {}
    for group, cols in SECTION_GROUPS.items():
        acc = np.zeros(len(rows), dtype=np.float64)
        for col in cols:
            if col in col_index:
                ci = col_index[col]
                acc += np.asarray([float(r[ci]) if r[ci] else 0.0 for r in rows])
        section_ms[group] = acc

    return WaveSample(wave=wave, path=path, frames=len(rows),
                      total_ms=total_vals, section_ms=section_ms)


def _wave_from_filename(path: str) -> int:
    m = re.search(r"wave(\d+)", os.path.basename(path), re.IGNORECASE)
    return int(m.group(1)) if m else 0


def _write_demo_csvs(out_dir: str) -> None:
    """Generate plausible synthetic CSVs to validate the plotting pipeline.

    These numbers are illustrative ONLY — they encode the hypothesised
    scaling story (AI threading and physics broadphase blow up with zombie
    count, animation grows linearly with skinned characters).
    """
    rng = np.random.default_rng(42)
    waves = [1, 5, 10]
    # zombie count per wave (rough proxy)
    zombies = {1: 8, 5: 60, 10: 180}

    for w in waves:
        n = zombies[w]
        frames = 600
        ai      = (0.02 * (n ** 1.6)) + rng.normal(0.0, 0.4, frames)   # ~O(n^1.6) no spatial accel
        physics = (0.05 * (n ** 1.4)) + rng.normal(0.0, 0.6, frames)   # broad-phase pain
        anim_s  = (0.12 * n)          + rng.normal(0.0, 0.3, frames)   # linear joint update
        anim    = 0.05 * n            + rng.normal(0.0, 0.1, frames)
        mvmt    = 0.02 * n            + rng.normal(0.0, 0.05, frames)
        input_  = rng.normal(0.15, 0.03, frames)
        combat  = rng.normal(0.20, 0.05, frames)
        health  = rng.normal(0.10, 0.02, frames)
        despawn = rng.normal(0.05, 0.01, frames)
        rcast   = rng.normal(0.10, 0.02, frames)
        rsub    = rng.normal(1.20, 0.10, frames) + 0.005 * n
        rdraw   = rng.normal(2.50, 0.20, frames)

        cols = {
            "Physics": np.clip(physics, 0.0, None),
            "Animation_System": np.clip(anim_s, 0.0, None),
            "Animation": np.clip(anim, 0.0, None),
            "AI_Threading": np.clip(ai, 0.0, None),
            "Movement": np.clip(mvmt, 0.0, None),
            "PlayerInput": np.clip(input_, 0.0, None),
            "Combat": np.clip(combat, 0.0, None),
            "Health": np.clip(health, 0.0, None),
            "Despawn": np.clip(despawn, 0.0, None),
            "Physics_Raycast": np.clip(rcast, 0.0, None),
            "Render_Submit": np.clip(rsub, 0.0, None),
            "Renderer_DrawFrame": np.clip(rdraw, 0.0, None),
            # Renderer CPU breakdown - the multithreading-relevant rows.
            # Recording cost grows roughly linearly with the number of drawables
            # (each renderable -> one or more vkCmdDraw* calls).
            "Renderer_DrawCallSort": np.clip(0.005 * n + rng.normal(0.05, 0.01, frames), 0.0, None),
            "Renderer_RecordCmds":   np.clip(0.30 + 0.04 * n + rng.normal(0.0, 0.10, frames), 0.0, None),
            "Renderer_QueueSubmit":  np.clip(rng.normal(0.20, 0.02, frames), 0.0, None),
            "Renderer_Present":      np.clip(rng.normal(0.15, 0.02, frames), 0.0, None),
            "Renderer_FenceWait":    np.clip(rng.normal(1.50, 0.30, frames), 0.0, None),
        }
        total = sum(v for v in cols.values()) + rng.normal(0.5, 0.05, frames)

        path = os.path.join(out_dir, f"wave{w}.csv")
        with open(path, "w", newline="") as fh:
            wcsv = csv.writer(fh)
            wcsv.writerow(["frame", "wave", "total_ms", *cols.keys()])
            for i in range(frames):
                wcsv.writerow([i, w, f"{total[i]:.6f}",
                               *[f"{cols[k][i]:.6f}" for k in cols]])
        print(f"[plot_profile] wrote demo {path}")


def _discover(profile_dir: str) -> List[str]:
    paths = sorted(glob.glob(os.path.join(profile_dir, "wave*.csv")),
                   key=_wave_from_filename)
    return paths


def _plot_bar(samples: List[WaveSample], baseline_wave: int, out_path: str) -> None:
    """Mean per-frame CPU time per section, grouped bars per wave."""
    samples = sorted(samples, key=lambda s: s.wave)
    groups  = list(SECTION_GROUPS.keys())
    waves   = [s.wave for s in samples]
    means   = np.zeros((len(groups), len(samples)))
    for j, s in enumerate(samples):
        for i, g in enumerate(groups):
            means[i, j] = float(np.mean(s.section_ms[g]))

    x = np.arange(len(groups))
    width = 0.8 / max(len(samples), 1)
    fig, ax = plt.subplots(figsize=(11, 6))
    colors = plt.cm.viridis(np.linspace(0.15, 0.85, len(samples)))
    for j, s in enumerate(samples):
        offset = (j - (len(samples) - 1) / 2.0) * width
        ax.bar(x + offset, means[:, j], width,
               label=f"wave {s.wave} (n={s.frames})", color=colors[j])

    ax.set_xticks(x)
    ax.set_xticklabels(groups, rotation=15, ha="right")
    ax.set_ylabel("Mean CPU time per frame [ms]")
    ax.set_title("Per-section CPU cost by wave")
    ax.grid(axis="y", linestyle=":", alpha=0.5)
    ax.legend(loc="upper left", frameon=False)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[plot_profile] wrote {out_path}")


def _plot_timeline(samples: List[WaveSample], out_path: str) -> None:
    """Per-frame timeline for the three sections of interest, one panel each."""
    panels = ["Renderer (parallelizable)", "Physics", "Animation"]
    fig, axes = plt.subplots(len(panels), 1, figsize=(11, 8), sharex=False)
    samples = sorted(samples, key=lambda s: s.wave)
    colors = plt.cm.viridis(np.linspace(0.15, 0.85, len(samples)))
    for ax, panel in zip(axes, panels):
        for j, s in enumerate(samples):
            ax.plot(s.section_ms[panel], color=colors[j], linewidth=1.0,
                    label=f"wave {s.wave}")
        ax.set_title(panel)
        ax.set_ylabel("ms / frame")
        ax.grid(linestyle=":", alpha=0.5)
        ax.legend(loc="upper right", frameon=False, ncol=len(samples))
    axes[-1].set_xlabel("frame")
    fig.suptitle("Per-frame CPU cost over capture (3 critical sections)")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[plot_profile] wrote {out_path}")


def _print_summary(samples: List[WaveSample], baseline_wave: int) -> None:
    samples = sorted(samples, key=lambda s: s.wave)
    baseline = next((s for s in samples if s.wave == baseline_wave), samples[0])
    print()
    print("  wave | frames |  total | Render|| Physics | Anim  | EnemyAI | factor vs baseline (total)")
    print("  -----+--------+--------+-------+---------+-------+---------+---------------------------")
    base_total = float(np.mean(baseline.total_ms))
    for s in samples:
        tot = float(np.mean(s.total_ms))
        rp  = float(np.mean(s.section_ms["Renderer (parallelizable)"]))
        ph  = float(np.mean(s.section_ms["Physics"]))
        an  = float(np.mean(s.section_ms["Animation"]))
        ai  = float(np.mean(s.section_ms["EnemyAI"]))
        factor = tot / base_total if base_total > 0 else float("nan")
        print(f"   {s.wave:>3} | {s.frames:>6} | {tot:6.2f} | {rp:5.2f} |  {ph:6.2f} | {an:5.2f} |  {ai:5.2f}  | x{factor:.2f}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", nargs="+", default=None,
                        help="CSV files (e.g. profile/wave1.csv profile/wave5.csv). "
                             "If omitted, auto-discovers profile/wave*.csv under --out.")
    parser.add_argument("--out", default="profile",
                        help="Output directory for generated PNGs (default: profile).")
    parser.add_argument("--baseline-wave", type=int, default=1,
                        help="Wave number used as the baseline (default: 1).")
    parser.add_argument("--demo", action="store_true",
                        help="Write synthetic demo CSVs (profile/wave1.csv, wave5.csv, wave10.csv) "
                             "so the plotting pipeline can be validated without running the game.")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)

    if args.demo:
        _write_demo_csvs(args.out)

    inputs = args.input if args.input else _discover(args.out)
    if not inputs:
        print(f"[plot_profile] no CSVs found. Pass --input or place wave*.csv under {args.out}/",
              file=sys.stderr)
        return 1

    samples: List[WaveSample] = []
    for p in inputs:
        if not os.path.isfile(p):
            print(f"[plot_profile] missing: {p}", file=sys.stderr)
            continue
        try:
            samples.append(_read_csv(p))
        except Exception as exc:  # noqa: BLE001
            print(f"[plot_profile] {p}: {exc}", file=sys.stderr)
    if not samples:
        return 1

    _print_summary(samples, args.baseline_wave)
    _plot_bar(samples, args.baseline_wave, os.path.join(args.out, "cpu_breakdown.png"))
    _plot_timeline(samples,                 os.path.join(args.out, "cpu_timeline.png"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
