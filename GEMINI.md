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

---

## 4. UI Typography & Character Encoding Mandate (No 'â' Glyphs)

- **Zero Raw Multi-Byte Unicode Literals in UI Text:** Never embed raw multi-byte UTF-8 glyphs (such as `▾`, `▼`, `►`, `◄`, `•`, `✓`, etc.) directly into C++ string literals or button labels. In JUCE / C++, raw string literals are prone to code page mismatch (MacRoman / ISO-8859), resulting in corrupted glyph artifacts such as `â`, `â`, or unexpected characters.
- **Enforced Standards:**
  1. **Procedural Vector Drawing (Preferred):** Draw arrows, triangles, chevrons, icons, and status dots using `juce::Graphics::fillPath()`, `juce::Graphics::fillTriangle()`, or custom look-and-feel methods.
  2. **Standard ASCII (Safe Alternative):** Use clean ASCII symbols such as `<`, `>`, `v`, `^`, `*`.
  3. **Explicit UTF-8:** If Unicode symbols are unavoidable, strictly wrap them using `juce::String::fromUTF8(...)` or `juce::CharPointer_UTF8`.
  
  ---

## 5. Modularity & File Size Mandate (Anti-Monolith Rules)

To prevent context degradation and maintain clean separation of concerns:
- **Hard Line Limit:** No new `.cpp` or `.h` file may exceed **400 lines of code**.
- **Legacy Monolith Rule:** If you are editing an existing file that already exceeds 400 lines, you are **FORBIDDEN** from adding new feature logic directly into it. You must first extract a self-contained sub-component or helper class into its own `.h`/`.cpp` file before implementing the change.
- **One Responsibility per File:**
  - **UI (`plugins/nova_*/`):** Never build monolithic `PluginEditor.cpp` files. Every logical UI section (e.g., Oscillator Panel, Filter Section, Modulation Bar, Preset Header, Custom Knob/Visualizer) MUST be an isolated `juce::Component` subclass in its own file.
  - **DSP (`libs/nova_core_dsp/`):** Each DSP building block (Voice, Oscillator, Filter, Envelope, ModMatrix, Effect) MUST reside in its own dedicated header/implementation file.
  - **Strict Header / Implementation Split (.h vs .cpp):** Header files (`.h`) must ONLY contain class/struct declarations, member signatures, and brief inline getters (max 120 lines per `.h` file). All implementation logic (`paint()`, `resized()`, parameter callbacks, DSP math) MUST go into a matching `.cpp` file registered in `CMakeLists.txt`. Never write large header-only UI components.
- **Ignored Binary Data Files:** NEVER read, open, grep, or modify `libs/nova_core_dsp/src/SampleBankData.cpp` (139k lines of raw sample data).

---

## 6. Strict UI / Audio Decoupling Mandate (JUCE Architecture)

- **Zero Direct DSP Access from UI:** `PluginEditor` and child `juce::Component` classes must **NEVER** call methods or mutate state directly on classes in `libs/nova_core_dsp/`.
- **Parameter Changes (UI -> Audio):** All continuous and discrete synth/effect parameters must flow exclusively through `juce::AudioProcessorValueTreeState` (APVTS) attachments (`SliderAttachment`, `ComboBoxAttachment`, `ButtonAttachment`) and be read atomically on the audio thread.
- **Complex State & Metering (Audio <-> UI):** All non-parameter events, preset loads, or visualizer/spectrum data must use lock-free structures (`nova::dsp::LockFreeRingBuffer` or `std::atomic` flags), polled on the UI thread via `juce::Timer`.

---

## 7. Anti-Regression & Negative Testing Mandate (No Happy-Path-Only Tests)

Trivial "happy-path" unit tests (e.g., setting a value and asserting the getter returns it) are insufficient and strictly discouraged. Every test suite in `tests/` must enforce:
1. **Boundary & Hostile Input Testing:** Every DSP block (`libs/nova_core_dsp/`) must be tested against extreme parameter values (`0.0f`, `1.0f`, rapid modulation jumps), extreme sample rates (`22050 Hz`, `44100 Hz`, `96000 Hz`, `192000 Hz`), odd buffer sizes (`1`, `13`, `64`, `2048` samples), and `NaN`/`Inf` input recovery.
2. **Full-Chain Integration Checks:** Parameter changes must be tested end-to-end through `NovaSynthProcessor` (modifying APVTS parameter -> `processBlock()` -> verifying measurable change in audio output or engine state), never just in isolated mock structs.
3. **Acoustic Invariants Assertion:** Every audio-rendering test must assert:
   - Zero `NaN` or `Inf` samples across the entire output buffer.
   - Bounded peak amplitude (`std::abs(sample) <= 4.0f` / no filter blow-ups).
   - Non-silent RMS output when a MIDI note is active, and decay to silence after `triggerPanic()`.
4. **State Serialization Roundtrip:** Any new state or parameter must be covered by a `getStateInformation()` -> `setStateInformation()` roundtrip assertion.

---

## 8. Agent Workflow & Context Hygiene Mandate

To maintain high reasoning quality and prevent context degradation:
1. **Mandatory Autonomous Commit at Task End:** Do NOT finish your turn leaving uncommitted changes if the build and tests succeeded. As the final step of EVERY task, after `ctest --test-dir build --output-on-failure` passes with 100% green tests, you MUST actively execute `git add -A && git commit -m "<type>: <concise message>"` via the terminal tool.
2. **Test-First for Bug Fixes:** When fixing a bug, first write a failing test case in `tests/` that reproduces the issue, then modify the code until the test passes.
3. **Targeted File Pinning:** Always explicitly provide/pin only the 1–3 target files needed for an operation to the agent. Never let the agent blindly search or scan large directories containing binary data or unneeded monoliths.
4. **Multi-File Split for Large Components:** When extracting classes exceeding 400 lines (e.g., complex UI components), split implementations across multiple `.cpp` files (e.g., `Component.cpp`, `ComponentDrawing.cpp`, `ComponentInteraction.cpp`) rather than violating the 450-line file size limit.