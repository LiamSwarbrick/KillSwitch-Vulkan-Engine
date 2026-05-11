#!/usr/bin/env python3
import math
import random
import struct
import wave
from pathlib import Path


SAMPLE_RATE = 44100
OUT_DIR = Path(__file__).resolve().parents[1] / "assets" / "sounds" / "gameplay"


def clamp(value, low=-1.0, high=1.0):
    return max(low, min(high, value))


def env_decay(t, attack, decay):
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def one_pole_lowpass(samples, alpha):
    out = []
    y = 0.0
    for x in samples:
        y += alpha * (x - y)
        out.append(y)
    return out


def normalize(samples, peak=0.92):
    current = max((abs(s) for s in samples), default=0.0)
    if current <= 0.0001:
        return samples
    gain = peak / current
    return [clamp(s * gain) for s in samples]


def write_wav(name, samples):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / name
    samples = normalize(samples)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for s in samples:
            value = int(clamp(s) * 32767)
            frames += struct.pack("<hh", value, value)
        f.writeframes(frames)


def render(duration, fn):
    total = int(duration * SAMPLE_RATE)
    return [fn(i / SAMPLE_RATE, i) for i in range(total)]


def footstep(seed, weight=1.0, scrape=0.35, pitch=82.0, duration=0.24):
    rng = random.Random(seed)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(duration * SAMPLE_RATE))]
    dirt = one_pole_lowpass(noise, 0.12)

    def sample(t, i):
        thump_env = env_decay(t, 0.006, 0.045)
        dirt_env = env_decay(t, 0.002, 0.075)
        low = math.sin(2.0 * math.pi * (pitch - t * 55.0) * t) * thump_env
        grit = dirt[i] * dirt_env
        heel = math.sin(2.0 * math.pi * 190.0 * t) * env_decay(t, 0.001, 0.018)
        return weight * (low * 0.72 + grit * scrape + heel * 0.12)

    return render(duration, sample)


def jump_takeoff():
    rng = random.Random(40)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(0.34 * SAMPLE_RATE))]
    cloth = one_pole_lowpass(noise, 0.18)

    def sample(t, i):
        push = math.sin(2.0 * math.pi * (95.0 + 80.0 * t) * t) * env_decay(t, 0.005, 0.052)
        scrape = cloth[i] * env_decay(t, 0.002, 0.11)
        return push * 0.42 + scrape * 0.44

    return render(0.34, sample)


def landing(seed, hard=False):
    rng = random.Random(seed)
    duration = 0.34 if hard else 0.24
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(duration * SAMPLE_RATE))]
    dirt = one_pole_lowpass(noise, 0.09 if hard else 0.12)

    def sample(t, i):
        thud = math.sin(2.0 * math.pi * (58.0 - 18.0 * t) * t) * env_decay(t, 0.003, 0.075 if hard else 0.052)
        grit = dirt[i] * env_decay(t, 0.001, 0.12 if hard else 0.07)
        return thud * (0.86 if hard else 0.58) + grit * (0.42 if hard else 0.27)

    return render(duration, sample)


def dry_fire():
    rng = random.Random(50)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(0.18 * SAMPLE_RATE))]

    def click(t, center, freq, width):
        return math.sin(2.0 * math.pi * freq * t) * math.exp(-((t - center) ** 2) / width)

    def sample(t, i):
        transient = click(t, 0.012, 3200.0, 0.000018) + click(t, 0.057, 1850.0, 0.00004)
        spring = math.sin(2.0 * math.pi * 620.0 * t) * env_decay(max(t - 0.052, 0.0), 0.001, 0.035)
        grit = noise[i] * env_decay(t, 0.001, 0.026)
        return transient * 0.55 + spring * 0.18 + grit * 0.09

    return render(0.18, sample)


def bullet_impact(seed, flesh=False):
    rng = random.Random(seed)
    duration = 0.34 if flesh else 0.42
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(duration * SAMPLE_RATE))]
    body = one_pole_lowpass(noise, 0.10 if flesh else 0.22)

    def sample(t, i):
        crack = noise[i] * env_decay(t, 0.001, 0.018)
        if flesh:
            thump = math.sin(2.0 * math.pi * 72.0 * t) * env_decay(t, 0.003, 0.08)
            wet = body[i] * env_decay(t, 0.001, 0.11)
            return thump * 0.68 + wet * 0.38 + crack * 0.12

        stone = body[i] * env_decay(t, 0.001, 0.07)
        ring = math.sin(2.0 * math.pi * (1680.0 - 680.0 * t) * t) * env_decay(t, 0.002, 0.13)
        return crack * 0.32 + stone * 0.52 + ring * 0.18

    return render(duration, sample)


def zombie_growl(seed, duration=1.05, attack=False):
    rng = random.Random(seed)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(int(duration * SAMPLE_RATE))]
    breath = one_pole_lowpass(noise, 0.045)

    def sample(t, i):
        swell = min(1.0, t / 0.12) * min(1.0, (duration - t) / 0.18)
        wobble = 1.0 + 0.22 * math.sin(2.0 * math.pi * 5.5 * t) + 0.08 * math.sin(2.0 * math.pi * 11.0 * t)
        base = math.sin(2.0 * math.pi * 76.0 * wobble * t)
        octave = math.sin(2.0 * math.pi * 143.0 * wobble * t)
        rasp = breath[i] * (0.48 if attack else 0.36)
        slash = 0.0
        if attack:
            slash = noise[i] * env_decay(max(t - 0.18, 0.0), 0.002, 0.09) * 0.45
        return (base * 0.42 + octave * 0.16 + rasp + slash) * swell

    return render(duration, sample)


def ui_tone(kind):
    if kind == "select":
        duration, f1, f2 = 0.16, 620.0, 930.0
    elif kind == "back":
        duration, f1, f2 = 0.18, 510.0, 320.0
    else:
        duration, f1, f2 = 0.28, 180.0, 255.0

    def sample(t, _):
        tone_a = math.sin(2.0 * math.pi * f1 * t)
        tone_b = math.sin(2.0 * math.pi * f2 * t)
        click = math.sin(2.0 * math.pi * 1400.0 * t) * env_decay(t, 0.001, 0.018)
        body = (tone_a * 0.44 + tone_b * 0.24) * env_decay(t, 0.004, duration * 0.34)
        return body + click * 0.08

    return render(duration, sample)


def main():
    write_wav("footstep_walk_01.wav", footstep(1, weight=0.50, scrape=0.28, pitch=82.0))
    write_wav("footstep_walk_02.wav", footstep(2, weight=0.46, scrape=0.34, pitch=89.0))
    write_wav("footstep_walk_03.wav", footstep(3, weight=0.52, scrape=0.31, pitch=76.0))
    write_wav("footstep_sprint_01.wav", footstep(11, weight=0.74, scrape=0.42, pitch=96.0, duration=0.21))
    write_wav("footstep_sprint_02.wav", footstep(12, weight=0.70, scrape=0.47, pitch=102.0, duration=0.20))
    write_wav("footstep_sprint_03.wav", footstep(13, weight=0.78, scrape=0.39, pitch=92.0, duration=0.22))
    write_wav("footstep_crouch_01.wav", footstep(21, weight=0.25, scrape=0.22, pitch=70.0, duration=0.19))
    write_wav("footstep_crouch_02.wav", footstep(22, weight=0.22, scrape=0.25, pitch=74.0, duration=0.18))
    write_wav("zombie_step_01.wav", footstep(31, weight=0.70, scrape=0.36, pitch=58.0, duration=0.29))
    write_wav("zombie_step_02.wav", footstep(32, weight=0.76, scrape=0.41, pitch=63.0, duration=0.31))
    write_wav("jump_takeoff.wav", jump_takeoff())
    write_wav("land_soft.wav", landing(60, hard=False))
    write_wav("land_hard.wav", landing(61, hard=True))
    write_wav("weapon_dry_fire.wav", dry_fire())
    write_wav("bullet_impact.wav", bullet_impact(70, flesh=False))
    write_wav("weapon_hit_flesh.wav", bullet_impact(71, flesh=True))
    write_wav("zombie_attack_short.wav", zombie_growl(80, duration=0.72, attack=True))
    write_wav("zombie_alert_short.wav", zombie_growl(81, duration=0.62, attack=True))
    write_wav("zombie_groan_short_01.wav", zombie_growl(82, duration=1.08, attack=False))
    write_wav("zombie_groan_short_02.wav", zombie_growl(83, duration=1.22, attack=False))
    write_wav("ui_select.wav", ui_tone("select"))
    write_wav("ui_back.wav", ui_tone("back"))
    write_wav("ui_pause.wav", ui_tone("pause"))


if __name__ == "__main__":
    main()
