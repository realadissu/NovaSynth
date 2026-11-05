# GEMINI.md - Architecture & Engineering Guidelines for Nova Plugins Suite

## Overview

This repository (`Nova Plugins`) is a C++20 / JUCE 8 / CLAP multi-plugin monorepo targeting **AU, VST3, CLAP, and Standalone** on macOS and Windows:
- **Nova Space (`NvSp`)**: Low-CPU Dual-Engine Reverb (Acoustic FDN-16 + Lush/Worship Shimmer Cloud)
- **Nova Synth (`NvSy`)**: JORNA-inspired Multi-Engine Worship Pad & Pluck Workstation
- **Nova Piano (`NvPn`)**: Spectral-Profiled & Physical-Modeling Live Concert Grand

---

## 1. Realtime Safety (RTS) Mandate

All code inside `libs/nova_core_dsp/` and any `processBlock()` path **MUST** be strictly Realtime-Safe:
- **Zero Heap Allocations** on the audio thread (`new`, `malloc`, `std::vector::push_back`, `std::string` allocations, or resizing containers are strictly forbidden in `processBlock`). Pre-allocate all buffers in `prepare()`.
- **Zero Blocking Locks** (`std::mutex`, `juce::CriticalSection`, file I/O, or system calls are forbidden on the audio thread). Use `std::atomic` or lock-free ring buffers (`nova::dsp::LockFreeRingBuffer`).
- **Denormal Protection:** Always include `juce::ScopedNoDenormals` at the entry of `processBlock()` and guard feedback loops against NaN/Inf.

---

## 2. TDD & API Synchronization Mandate

1. Every DSP algorithm in `libs/nova_core_dsp/` and plugin processor state in `libs/nova_core_plugin/` must be verified by unit tests in `tests/`.
2. When modifying public headers in `libs/`, update all dependent plugin targets (`plugins/nova_*`) and unit tests (`tests/*Tests.cpp`) before completing a task.
3. Quality and musicality take priority over shortcuts: all parameters must be smoothed (`OnePoleSmoother`) to prevent zipper noise.

---

## 3. Git Workflow & Regular Commits Mandate

1. **Regular & Incremental Commits:** Commit often at each logical milestone, bug fix, or DSP/feature improvement rather than accumulating large monolithic diffs.
2. **Pre-Commit Verification:** Every commit must compile cleanly with zero errors/warnings and pass all unit tests (`ctest --test-dir build --output-on-failure`).
3. **Meaningful Commit Messages:** Use descriptive Conventional Commit style messages (`feat:`, `fix:`, `refactor:`, `perf:`, `test:`, `docs:`) clearly detailing the changes and musical/DSP rationale.
4. **Synchronized Remote State:** Push verified commits regularly to the remote tracking branch so the working state remains safe, reproducible, and transparent.
