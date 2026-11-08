#!/usr/bin/env python3
import sys
import numpy as np
import scipy.io.wavfile as wav

def analyze_wav(path, start_sec=2.0, end_sec=5.0):
    sr, data = wav.read(path)
    if data.ndim == 2:
        # Check stereo channels
        left = data[:, 0].astype(np.float32)
        right = data[:, 1].astype(np.float32)
        max_val = 32768.0 if data.dtype == np.int16 else 1.0
        left /= max_val
        right /= max_val
        mono = 0.5 * (left + right)
    else:
        max_val = 32768.0 if data.dtype == np.int16 else 1.0
        mono = data.astype(np.float32) / max_val
        left = mono
        right = mono

    total_len = len(mono) / sr
    s_idx = int(min(start_sec, total_len * 0.5) * sr)
    e_idx = int(min(end_sec, total_len) * sr)
    if e_idx <= s_idx:
        s_idx = 0
        e_idx = len(mono)

    w = mono[s_idx:e_idx]
    peak = max(np.max(np.abs(left[s_idx:e_idx])), np.max(np.abs(right[s_idx:e_idx])))
    rms = np.sqrt(np.mean(w**2))

    # Clipping detection: count samples within 0.005 dB of 1.0
    clip_count = np.sum(np.abs(w) >= 0.995)

    fft = np.abs(np.fft.rfft(w * np.hanning(len(w))))
    freqs = np.fft.rfftfreq(len(w), 1.0 / sr)

    def band_energy(f_low, f_high):
        idx = (freqs >= f_low) & (freqs < f_high)
        return float(np.sum(fft[idx]**2))

    sub_e = band_energy(20, 100)
    low_mids_e = band_energy(100, 350)
    mids_e = band_energy(350, 1200)
    high_mids_e = band_energy(1200, 4000)
    treble_e = band_energy(4000, 10000)
    air_e = band_energy(10000, 20000)

    tot = sub_e + low_mids_e + mids_e + high_mids_e + treble_e + air_e + 1e-12
    cum = np.cumsum(fft**2)
    rolloff_idx = np.searchsorted(cum, 0.90 * cum[-1])
    rolloff_90 = freqs[rolloff_idx]
    centroid = np.sum(freqs * (fft**2)) / np.sum(fft**2)

    return {
        "path": path,
        "sample_rate": sr,
        "duration": total_len,
        "peak_db": 20.0 * np.log10(max(1e-6, peak)),
        "rms_db": 20.0 * np.log10(max(1e-6, rms)),
        "clipping_detected": bool(clip_count > 5),
        "clip_count": int(clip_count),
        "sub_pct": 100.0 * sub_e / tot,
        "low_mids_pct": 100.0 * low_mids_e / tot,
        "mids_pct": 100.0 * mids_e / tot,
        "high_mids_pct": 100.0 * high_mids_e / tot,
        "treble_pct": 100.0 * treble_e / tot,
        "air_pct": 100.0 * air_e / tot,
        "warmth_pct": 100.0 * (sub_e + low_mids_e + mids_e) / tot, # % energy below 1200 Hz
        "rolloff_90_hz": float(rolloff_90),
        "centroid_hz": float(centroid)
    }

def print_analysis(res, title="Audio Analysis"):
    print(f"=== {title}: {res['path']} ===")
    print(f"  Peak:      {res['peak_db']:.2f} dBFS  (Clipping: {'YES!' if res['clipping_detected'] else 'Clean'})")
    print(f"  RMS:       {res['rms_db']:.2f} dBFS")
    print(f"  Warmth (<1.2k Hz):  {res['warmth_pct']:.1f}%  (Target: >95% for warm worship pad)")
    print(f"    - Sub (20-100Hz):        {res['sub_pct']:.1f}%")
    print(f"    - Low-Mids (100-350Hz):  {res['low_mids_pct']:.1f}%")
    print(f"    - Mids (350-1200Hz):     {res['mids_pct']:.1f}%")
    print(f"    - High-Mids (1.2k-4kHz): {res['high_mids_pct']:.1f}%")
    print(f"    - Treble (4k-10kHz):     {res['treble_pct']:.1f}%")
    print(f"    - Air (10k-20kHz):       {res['air_pct']:.1f}%")
    print(f"  Centroid:  {res['centroid_hz']:.0f} Hz (Target: 300 - 650 Hz)")
    print(f"  Rolloff90: {res['rolloff_90_hz']:.0f} Hz (Target: 400 - 950 Hz)")
    print()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 tools/analyze_pad.py <audio.wav> [reference.wav]")
        sys.exit(1)

    target = analyze_wav(sys.argv[1])
    print_analysis(target, "Synth Render")

    if len(sys.argv) >= 3:
        ref = analyze_wav(sys.argv[2], start_sec=2.0, end_sec=6.0)
        print_analysis(ref, "Reference Target")
        diff_centroid = target['centroid_hz'] - ref['centroid_hz']
        diff_warmth = target['warmth_pct'] - ref['warmth_pct']
        print(f"Comparison:")
        print(f"  Centroid delta: {diff_centroid:+.0f} Hz ({'Too bright/harsh' if diff_centroid > 200 else ('Too dark' if diff_centroid < -200 else 'Spot on!')})")
        print(f"  Warmth delta:   {diff_warmth:+.1f}% ({'Harsh top leakage' if diff_warmth < -5.0 else 'Pristine warm core'})")
