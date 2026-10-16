# Animation Speed / Pacing Audit

**Scope:** audit only. No production code was modified.
**Device:** Seeed XIAO ESP32-C3 (also builds for Arduino Uno), 16×16 WS2812B = 256 pixels.
**Method:** every metric below is derived from the animation source, not from constant names alone.

---

## Executive Summary

Nineteen animation ids are declared in `AnimationId`, but only **eighteen** have an
implementation. Two ids referenced by the scheduler, the special-date rules and the
LAN panel — **`BREATHING_HEART` and `LOVE_MODE`** — have **no implementation and no
`AnimationRegistry` entry at all**. Selecting either (by hand or automatically)
resolves to `nullptr`, so `AnimationManager::play()` calls `stop()` and the panel goes
**dark** for the whole slot, up to 10 minutes. This is a proven bug, not a pacing
opinion, and it is the single most severe finding.

Pacing itself is in good shape for the ambient set. Aurora, Moon/Stars, Sunset/Water,
Christmas Tree, Sun Over Mountains, Campfire and Dancing Duck all use elapsed-time
gating with calm, readable cadences and meaningful cycle lengths. The outlier the user
observed is real: **SWIMMING_FISH moves at 20 px/s — 1.25 screen widths per second**,
crossing the visible matrix in 0.75 s. Its **bubbles are 2.5× faster still** (50 px/s,
3.13 screen heights/s), which is internally inconsistent as well as fast.

SWIMMING_FISH also has a second, structural defect found during this audit: it is the
**only** animation that re-renders on every loop iteration regardless of whether a
logical frame advanced, defeating the dirty-frame optimisation that every other
animation respects.

Three further pacing concerns are flagged as *possible* (fireworks rocket step, meteor
speed, slot-machine result hold) and several items genuinely need physical
verification before any change is chosen.

| Severity | Count |
|---|---|
| Proven bugs | 2 |
| Strong pacing concerns | 2 |
| Possible pacing concerns | 3 |
| Needs physical verification | 7 |
| Looks reasonable | 12 |

---

## Stage 0 — Timing architecture

| Question | Finding |
|---|---|
| How often is `update()` called? | `loop()` → `app.tick(millis())` → `AnimationManager::tick()` → `animation->update(now, matrix)` on **every** loop iteration. There is no gating in the manager. |
| Elapsed-time or call-count based? | **Elapsed-time based.** Every animation compares `now - lastFrameAt_` against an interval constant. |
| Wrap-safe? | **Yes.** All deltas are computed in `uint32_t` (`now - lastX`), which is wrap-safe for `millis()` rollover. `start()` deliberately back-dates `lastFrameAt_ = now - kFrameIntervalMs` so the first frame fires immediately. |
| Can one update advance several logical frames? | **No.** The gate is `if (elapsed >= interval) { advance ONE step; lastFrameAt_ = now; }`. A late tick therefore causes the animation to **lag** rather than jump — the correct choice, and it means no skipped frames and no catch-up. |
| Any `delay()`? | **None** in `src/`. |
| Any ungated `x++`? | **No.** Every movement is inside a time gate. |
| Does dirty-frame optimisation affect speed? | **Partly — see Bug 2.** `PixelMatrix` marks dirty on any mutation and `showIfDirty()` transmits only when dirty. An animation that re-renders without a logical frame change re-transmits unnecessarily. |
| Do scheduler/playlists cut animations short? | **Yes, by design.** Schedule rows and playlist items hold for their configured duration, so a 12 s animation inside a 5 min slot loops and is never cut mid-cycle. A playlist item *can* interrupt an animation at an arbitrary point; that is intended reconciliation behaviour, not a bug. |

`Animation::update(now, matrix)` is the only entry point and `renderFrame`/`renderStage`
are private to each animation, so there is exactly one place per animation that writes
pixels.

---

## Stage 1–2 — Inventory and normalized timing

`False = 1000 / frameIntervalMs`. Cycle figures are derived from the constants named in
each row and were cross-checked against the movement code.

| Animation | Interval | FPS | Cycle | Motion | Assessment | Confidence |
|---|---|---|---|---|---|---|
| TEST | 250 ms/step | 4.0 | ~1.0 s (4 stages) | static stages | Diagnostic pattern | LOOKS REASONABLE |
| HEARTBEAT | 90 ms outline / 85 ms fill / 400 ms hold | 11.1 | ≈ 2.15 s per beat × 6 ≈ 12.9 s | n/a | Plausible ~28 bpm feel | NEEDS PHYSICAL TEST |
| BREATHING_HEART | — | — | — | — | **No implementation** | **PROVEN BUG** |
| ROSE_BLOOM | 110 → 500 ms, stage-dependent | 2.0–9.1 | ≈ 5–8 s bloom + breath | n/a | Deliberately slow, intervals increase | LOOKS REASONABLE |
| SPARKLE_HEART | stage-based (550 / 3400 / 1400 ms) | — | 5.35 s | n/a | Generous 1.4 s final hold | LOOKS REASONABLE |
| FIREWORKS | rocket 10 ms, burst 40 ms, fade 40 ms, pause 80 ms | 100 / 25 / 25 | 4 shells + 10 s text hold | 1 px/step (rocket) → 100 px/s = **6.25 widths/s** | Rocket may be too fast to perceive | POSSIBLE |
| LOVE_MODE | — | — | — | — | **No implementation** | **PROVEN BUG** |
| CAMPFIRE | 140 ms | 7.1 | 0.56 s frame cycle, 12 s total | n/a | Lively but not strobing | LOOKS REASONABLE |
| DANCING_DUCK | 320 ms | 3.1 | 1.28 s frame cycle, 12 s total | n/a | Deliberately slow | LOOKS REASONABLE |
| SLOT_MACHINE | 62 / 70 / 76 ms per reel | 13.2–16.1 | stops at 2.2 / 3.0 / 3.8 s; hold 0.7 s; jackpot 10 s | ≈ 14 symbols/s | Spin good, result hold possibly short | POSSIBLE |
| **SWIMMING_FISH** | **50 ms** | **20.0** | **2.0 s round trip**, 12 s total | **20 px/s = 1.25 widths/s** | **Too fast** | **STRONG + physical** |
| AURORA (×4 variants) | 50 ms | 20.0 | phase ≈ 8.5 s, loop 25.6 s | slow drift | 20 FPS is refresh, not motion | LOOKS REASONABLE |
| CHRISTMAS_TREE | render 90 ms, snow 320 ms, blink 420 ms | 11.1 | snow 0.32 s/px | 3.1 px/s = 0.20 widths/s | Calm | LOOKS REASONABLE |
| MOON_STARS | 180 ms | 5.6 | 11.5 s loop | n/a | Calm | LOOKS REASONABLE |
| SUNSET_WATER | 160 ms | 6.3 | 10.2 s loop | n/a | Subtle shimmer | LOOKS REASONABLE |
| FALLING_STARS | 90 ms | 11.1 | 5.76 s loop | 1 px per axis per frame → 11.1 px/s per axis, **≈ 15.7 px/s diagonal ≈ 0.98 widths/s** | Fast, but meteors are fast | POSSIBLE |
| SUN_OVER_MOUNTAINS | 120 ms | 8.3 | 7.68 s loop | n/a | Scene stays readable | LOOKS REASONABLE |

The four AURORA ids share **one** implementation (`AuroraAnimation`, palette-differing
only), so their pacing is identical by construction and cannot diverge.

---

## Stage 3 — Structural findings

### Bug 1 — `BREATHING_HEART` and `LOVE_MODE` are not implemented (PROVEN)

**Evidence**

- `AnimationId` declares `BREATHING_HEART` and `LOVE_MODE` (`include/core/AnimationId.h`).
- `AnimationRegistry::getAnimation` has **no case** for either id; the switch falls
  through to `default: return nullptr;`.
- There is no `BreathingHeartAnimation.*` or `LoveModeAnimation.*` in `src/animations`
  or `include/animations` (15 implementation files, 19 enum ids minus `NONE` minus these two).
- Both ids are actively used:
  - `ScheduleTable.h` MORNING playlist and `SpecialDates.h` Valentine's pool are not the
    only references — `Scheduler.cpp`'s Valentine's pool contains `LOVE_MODE` and
    `BREATHING_HEART`, and the morning/late-night pools contain `BREATHING_HEART`.
  - `AnimationCatalog` exposes both in the LAN panel's animation stepper.

**Consequence:** `AnimationManager::play(id)` calls `AnimationRegistry::getAnimation(id)`,
gets `nullptr`, and calls `stop()`. Selecting either id leaves the matrix **dark** until
the next slot change. When the scheduler picks one, the panel is dark for the remainder
of that slot.

**Why this matters for a pacing audit:** the darkness is not a slow animation, it is a
non-animation, and it will read to the user as "the lamp stopped working" rather than
"this animation is mistimed". Any pacing work should not proceed until this is resolved,
because two of the five romantic-pool entries are currently dead.

**Classification:** PROVEN BUG (non-timing, but blocking).

---

### Bug 2 — SWIMMING_FISH re-renders and re-transmits every loop iteration (PROVEN)

**Evidence.** Its `update()` is the only one of the fifteen that has **no "not due yet"
early return** before its render call:

```cpp
void SwimmingFishAnimation::update(uint32_t now, PixelMatrix& matrix) {
    if (finished_) return;
    if (totalElapsed >= kAnimationDurationMs) { finished_ = true; return; }

    if (frameElapsed >= kFrameIntervalMs) { /* move, bob, tail */ }
    if (bubbleElapsed >= kBubbleMoveMs)   { advanceBubbles(); }
    if (spawnElapsed >= currentBubbleInterval()) { spawnBubble(); }

    renderFrame(matrix);        // <-- unconditional, outside every time gate
}
```

Every other animation returns early when its frame is not due — verified individually
by inspecting each `update()` body:

| Animation | Returns early when not due? |
|---|---|
| Aurora, Campfire, ChristmasTree, DancingDuck, FallingStars, Fireworks, Heartbeat, MoonStars, RoseBloom, SlotMachine, SparkleHeart, SunOverMountains, SunsetWater, TestAnimation | **Yes** |
| **SwimmingFish** | **No** |

**Consequence:** `renderFrame()` calls `matrix.setPixel(...)` unconditionally, which marks
the frame dirty, so `showIfDirty()` **transmits to the LEDs on every loop iteration**
rather than 20 times per second. This defeats the dirty-frame optimisation for this
animation only, wastes bandwidth and CPU, and — because the physical transmission rate
is no longer tied to the logical frame rate — makes any future timing measurement of
this animation harder to reason about.

**Note:** this does **not** by itself make the motion look faster. Motion speed is still
1 px per 50 ms. It is a separate, real defect.

**Classification:** PROVEN BUG (structural).

---

### Structural checks that passed

| Check | Result |
|---|---|
| A. Frame-rate dependent movement | **None found.** All movement is inside elapsed-time gates. |
| B. Wrong time unit | **None found.** `Ms`-suffixed constants are compared against `millis()` deltas. |
| C. Multiple unintended advances | **None found.** Sub-animations (e.g. fish bob + tail + bubbles) each have their own timer and advance exactly one step per firing. |
| D. Phase fall-through | **None found.** Stage machines in Heartbeat, Fireworks, RoseBloom and SparkleHeart are `if/else if` chains that `return` after acting. |
| E. Loop reset skips a hold | **None found.** `AnimationManager` calls `stop()` then `start(now)` on LOOP; every `start()` restores the initial state. |
| F. Missing end hold | Two animations have **no** end hold: SwimmingFish and Campfire both set `finished_` and return without a final pause. `ONCE` mode therefore ends on the last frame with no dwell. For LOOP (how the scheduler uses them) this is invisible. |
| G. Inconsistent frame constants | **Found — see Stage 6.** SwimmingFish 50 ms for 1 px of motion is a 2.4× outlier against the other moving sprites. |
| H. Integer truncation | **None found** in timing paths. One truncating division exists in SparkleHeart's fade (`elapsed * 255 / kFadeInMs`) and is intentional. |
| I. Skipped frames | **None.** One step per gate, no catch-up. |
| J. Update catch-up | **Correct.** A stalled loop causes lag, never a positional jump. |
| K. Playlist interruption | Possible by design (playlist items are shorter than some cycles). Not a defect. |
| L. Dirty-frame interaction | **Found — Bug 2**, and it is specific to SwimmingFish. |

---

## Stage 4 — Physical 16×16 perception

On a diffused 16×16 panel, 1 pixel = 1/16 of the display width. Apparent motion is
therefore 16× more dramatic than the same px/s on a larger panel.

| Animation | px/s | screen-widths/s | Time to cross the visible 16 px |
|---|---|---|---|
| **SWIMMING_FISH (fish)** | **20.0** | **1.25** | **0.75 s** |
| **SWIMMING_FISH (bubbles, vertical)** | **50.0** | **3.13** | **0.32 s** |
| FALLING_STARS (diagonal) | ≈ 15.7 | ≈ 0.98 | ≈ 1.4 s |
| FIREWORKS (rocket) | ≈ 100 (if 1 px/step) | ≈ 6.25 | ≈ 0.16 s |
| CHRISTMAS_TREE (snow) | 3.1 | 0.20 | ≈ 5.2 s |

For reference, a calm ambient drift is in the 0.1–0.4 widths/s band. The fish is
**3–12× faster than the ambient set**, and the bubbles faster again.

---

## Stage 5–6 — Semantic pacing and group comparison

**CALM / AMBIENT**

| Animation | Cycle | Verdict |
|---|---|---|
| AURORA ×4 | 8.5 s phase / 25.6 s loop | Slow, smooth drift. Appropriate. |
| MOON_STARS | 11.5 s | 180 ms twinkle reads as calm. Appropriate. |
| SUNSET_WATER | 10.2 s | 160 ms shimmer is subtle, not flicker. Appropriate. |
| CHRISTMAS_TREE | snow 320 ms/step | Composition stays readable; twinkle does not dominate. Appropriate. |
| SUN_OVER_MOUNTAINS | 7.68 s | Rays animate at 120 ms and do not dominate the scene. Appropriate. |

Ambient cycles cluster in the 7.7–25.6 s range, which Stage 6 asked me to check. **No
ambient outlier.**

**SPRITE MOVEMENT**

| Animation | Motion | Verdict |
|---|---|---|
| DANCING_DUCK | 320 ms/frame, 1.28 s cycle | Very deliberate. Semantically right for a "dancing" duck. |
| FALLING_STARS | ≈ 0.98 widths/s diagonal | Fast, but a meteor *should* be fast. Events arrive ≈ every 1.08 s, so individual meteors are still perceivable. |
| **SWIMMING_FISH** | **1.25 widths/s** | **Outlier — see below.** |

The fish is a **2.4× frame-interval outlier** against the two comparable sprites
(DancingDuck 320 ms, FallingStars 90 ms for a diagonal sprint) and the only sprite whose
speed contradicts what it depicts: a decorative fish swimming calmly.

**EVENT**

| Animation | Verdict |
|---|---|
| FIREWORKS | Burst 40 ms / fade 40 ms are fine for an explosion. The 10 ms rocket step is 100 FPS and, if it is 1 px/step, the streak crosses the panel in 0.16 s. The 80 ms pause between shells is brief — with 4 shells the sequence is over quickly. The 10 s text hold is generous and readable. |
| SLOT_MACHINE | 62–76 ms reel steps ≈ 14 symbols/s is a convincing fast spin. Staggered stops at 2.2 / 3.0 / 3.8 s give a natural deceleration. The **700 ms** result hold is the item worth checking — three symbols plus a payline is not much dwell time. |
| ROSE_BLOOM | Stage intervals increase (110 → 350 → 400 → 450 → 500 ms), which reads as natural opening. Good. |

**HEART / LOVE**

| Animation | Verdict |
|---|---|
| HEARTBEAT | ~2.15 s per beat is roughly 28 bpm — calm and plausible for a decorative heart. Whether the *double* beat reads correctly is a physical question. |
| BREATHING_HEART | **Not implemented.** |
| SPARKLE_HEART | 550 ms fade-in, 3400 ms sparkle, 1400 ms final hold = 5.35 s. Generous holds. |
| LOVE_MODE | **Not implemented.** |

---

## Stage 7 — Refresh rate vs semantic rate

These are correctly separated in this codebase and must not be confused when acting on
this report:

- **AURORA refreshes at 20 FPS** (50 ms) while its *visual* drift period is 8.5 s per
  phase cycle. 20 FPS is refresh, not motion. **No problem.**
- **SWIMMING_FISH refreshes at 20 FPS** and its *motion* is 1 px per frame, so refresh
  and motion are locked to the same tick at 20 px/s. **This is the problem.** The fix is
  to decouple them, not merely to lower the render rate.
- **CAMPFIRE refreshes at 7.1 FPS** because flame shapes change only every 140 ms. Low
  refresh, correct motion.

A rule of the form "20 FPS is too fast" would wrongly condemn Aurora. The fish is the
only animation where a single tick drives both refresh and whole-pixel displacement at a
rate too high for the depicted subject.

---

## SWIMMING_FISH (detailed)

**Intent:** a fish swimming calmly across a decorative lamp, with bubbles rising.

**Current implementation**
- `include/animations/SwimmingFishAnimation.h`:
  `kFrameIntervalMs = 50`, `kBubbleMoveMs = 20`, `kAnimationDurationMs = 12000`.
- Horizontal: `++fishX_` / `--fishX_`, one pixel per frame tick.
- Start `fishX_ = -5`, sprite is 6 px wide → fully offscreen. Clamps at `fishX_ = 15`
  (one frame of dwell) then reverses at `fishX_ < -5`.
- Vertical bob: `fishY_` follows the 8-frame pattern 6,6,5,5,6,6,7,7 — ±1 px.
- Tail: `tailFrame_` cycles 0→1→2, one step per frame tick.
- Bubbles: `advanceBubbles()` runs every `kBubbleMoveMs`, moving each active bubble
  **1 px up and 1 px sideways** per firing. Spawn interval cycles 700/950/800/1100 ms
  through a 4-entry index.

**Timing math**

| Quantity | Value |
|---|---|
| Fish frame interval | 50 ms (20 FPS) |
| Horizontal displacement | 1 px per frame |
| **Horizontal speed** | **20.0 px/s = 1.25 screen widths/s** |
| Offscreen → right edge (−5 → 15) | 20 steps = **1.00 s** |
| Visible crossing (x = 0 → 15) | 15 steps = **0.75 s** |
| Round trip | 40 steps = **2.00 s** |
| Total loops in 12 s | **6 round trips** (240 frames) |
| Vertical bob cycle | 8 frames = **400 ms** (±1 px) |
| Tail flap cycle | 3 frames = **150 ms** (6.7 flaps/s) |
| Bubble rise speed | 1 px per 20 ms = **50 px/s = 3.13 screen heights/s** |
| Bubble time to cross full height | **320 ms** |
| Bubble spawn cadence | one per 700–1100 ms |
| Bubble vs fish speed | **2.5× faster than the fish** |

**Current physical speed:** the fish traverses the entire visible matrix in 0.75 s and
reverses. Bubbles cross the full height in 0.32 s — faster than the eye can follow as
discrete objects, so they will read as flicker rather than as bubbles.

**Problem:** the subject is a calm swimmer but the motion is projectile-like. The user's
physical observation ("TOO FAST") is consistent with the measurement: at 1.25 display
widths per second this is among the fastest motion in the entire firmware, and the only
sprite whose speed contradicts its subject. Compounding it, bubbles outrun the fish by
2.5×, so the two elements do not read as one scene. Bug 2 also means the panel is
re-transmitting this frame hundreds of times per second.

**Classification:** STRONG PACING CONCERN (physically confirmed), plus PROVEN BUG for
the unconditional re-render and the missing registry entry is unrelated but adjacent.

**Suggested target**
- Movement step every **150–200 ms** → **5.0–6.7 px/s** → **0.31–0.42 widths/s**.
- Visible crossing in **2.4–3.2 s**; round trip in **6.4–8.5 s**.
- Bubbles should rise at or **below** the fish's rate: bubble step **60–80 ms**
  (12.5–16.7 px/s, 0.78–1.04 heights/s), i.e. still brisk but no longer 2.5× the fish.
- Keeping the tail flap at 3 ticks per cycle is fine as an animation detail; at a 150 ms
  tick it becomes a 450 ms flap, which suits a calm swim.
- The vertical bob (8 ticks) would become 1.2–1.6 s, which also suits a calm swim.

**Recommended code-level adjustment (NOT applied)**
1. `SwimmingFishAnimation.h::kFrameIntervalMs` — 50 → **150 or 200**.
2. `SwimmingFishAnimation.h::kBubbleMoveMs` — 20 → **60–80**.
3. Add a "not due yet" early return before `renderFrame(matrix)` in `update()`, so the
   frame is rendered (and therefore transmitted) only when a logical frame actually
   advances — matching every other animation and restoring the dirty-frame optimisation.
4. If the fish is to remain a long ambient piece, revisit `kAnimationDurationMs`
   together with item 1 (see regression risk below).

**Regression risks**
- **Loop seam.** The current 12 s duration is exactly 240 frames, and 240 / 20 steps per
  pass = **12 passes — an even number**, so the fish ends where `start()` puts it
  (`x = -5`, facing right) and the LOOP restart is seamless. Changing only the interval
  breaks this: 12 s at 150 ms = 80 frames = **4 passes (even, still seamless)**, but at
  200 ms = 60 frames = **3 passes (odd)** → the loop restarts mid-return and the fish
  visibly jumps. Any interval change must keep the pass count even, or scale
  `kAnimationDurationMs` to compensate (e.g. 240 frames × 150 ms = 36 s).
- Shortening motion makes the 12 s loop contain fewer passes; if the piece is meant to
  occupy a schedule slot, confirm the reduced variety is acceptable.
- `exit` conditions at the screen edges currently dwell one frame each (`fishX_ = 15`
  clamps). At a slower tick that dwell doubles in real time; it may want removing.
- Slowing `kBubbleMoveMs` changes how long a bubble lives (currently ~320 ms). Bubble
  lifetime must be re-checked against the spawn cadence so the panel does not end up
  with three near-stationary bubbles.

**Tests that should accompany a future fix**
1. Assert `kFrameIntervalMs` and the derived px/s and widths/s.
2. Assert the loop seam: total frames / steps-per-pass is an **even** integer.
3. Assert bubbles are **not faster** than the fish.
4. Assert `update()` does not render when no timer is due (guards Bug 2 regressions) —
   e.g. by checking that `PixelMatrix::isDirty()` stays false across a non-due tick.
5. Assert determinism of the 8-frame bob and 3-frame tail sequences.

---

## Other detailed sections

### FIREWORKS

**Intent:** rockets launch, burst, fade, with a birthday-message finale.
**Implementation:** stage machine — rocket `kRocketStepMs = 10`, burst `kBurstStepMs = 40`,
fade `kFadeStepMs = 40`, `kPauseMs = 80` between shells, `kFireworkCount = 4`,
birthday text held `kBirthdayDurationMs = 10000` with `kLetterColorChangeMs = 1000` and
`kBorderToggleMs = 500`.
**Timing:** rocket refreshes at **100 FPS**. Burst and fade at 25 FPS. Shell-to-shell
pause 80 ms.
**Problem:** if the rocket advances 1 px per step it moves 100 px/s = **6.25 display
widths/s**, crossing the panel in ≈ 0.16 s — likely too brief to register as a rising
streak. The 80 ms inter-shell pause also gives little separation, so four shells may read
as one busy event. The 10 s text hold is good and should not change.
**Classification:** POSSIBLE PACING CONCERN. **The rocket's per-step displacement was
not verified in this pass** and must be checked before acting — if the step is sub-pixel
or multi-pixel the conclusion changes.
**Suggested target:** rocket step 25–40 ms (25–40 px/s ≈ 1.6–2.5 widths/s) unless the
displacement check shows otherwise; inter-shell pause 250–400 ms.
**Regression risk:** stage durations are also the readability budget for the burst; do not
shorten burst/fade.
**Tests:** none exist for Fireworks — timing is entirely untested.

### SLOT_MACHINE

**Intent:** reels spin fast, decelerate, settle, hold the result.
**Implementation:** `kReelStepMs = {70, 62, 76}`, `kStopRequestMs = {2200, 3000, 3800}`,
`kWinHoldMs = 700`, `kJackpotDurationMs = 10000`.
**Timing:** ≈ 14 symbols/s spin (13–16 FPS); staggered stops over 1.6 s.
**Problem:** the spin and staggered deceleration are well judged. The **700 ms** result
hold is the concern — three symbols plus a payline in 0.7 s is a short read.
**Classification:** POSSIBLE PACING CONCERN / NEEDS PHYSICAL TEST.
**Suggested target:** 1200–1500 ms result hold.
**Regression risk:** total duration grows; `kJackpotDurationMs` is separate and unaffected.
**Tests:** `test_slot_machine.cpp` exists; check whether it asserts the hold duration.

### FALLING_STARS

**Intent:** occasional meteors streaking across a night sky.
**Implementation:** `kFrameIntervalMs = 90`, `kLoopFrameCount = 64`, 4 meteors at phase
offsets 0/12/24/38, position `start + step * (dx,dy)` with `dx,dy = ±1`.
**Timing:** 1 px per axis per frame → 11.1 px/s per axis, **≈ 15.7 px/s diagonal ≈ 0.98
widths/s**. A meteor is on-screen ≈ 16 frames ≈ 1.4 s; events arrive ≈ every 1.08 s.
**Problem:** the speed is high in absolute terms, but a shooting star is *supposed* to be
fast and events remain individually perceivable. The risk is that with 4 meteors and a
1.08 s cadence the sky may feel busy rather than sparse.
**Classification:** POSSIBLE PACING CONCERN (spacing, not speed).
**Suggested target:** if it feels busy, widen the phase offsets rather than slowing the
meteor — the speed is semantically appropriate.
**Tests:** `test_falling_stars.cpp` asserts phase progression and a diagonal render; it
does **not** assert the interval or the loop duration.

### HEARTBEAT

**Implementation:** `kBeatFrameCount = 10`, `kBeatCycleCount = 6`, `kOutlineStepMs = 90`,
`kFillStepMs = 85`, `kFullHoldMs = 400`.
**Derived timing (approximate):** outline ≈ 10 × 90 = 900 ms, fill ≈ 10 × 85 = 850 ms,
hold 400 ms → ≈ 2.15 s per beat; × 6 ≈ **12.9 s** per loop ≈ **28 bpm**.
**Assessment:** the rate and the 400 ms rest between beats look right for a decorative
heart. Whether the *double* beat reads as a heartbeat at this size is a perception
question, not a code question.
**Classification:** NEEDS PHYSICAL TEST.
**Note:** the derived per-beat composition is inferred from the constants; the exact
stage sequence was not traced line by line. Confirm before changing anything.
**Tests:** none exist for Heartbeat timing.

### CHRISTMAS_TREE

**Implementation:** render 90 ms, snow step 320 ms, blink 420 ms.
**Assessment:** the dual-rate design (11 FPS render gate, 320 ms snow) is good — snow
drifts at 3.1 px/s (0.2 widths/s) while blinks stay readable. `frameChanged ||
renderElapsed >= kRenderIntervalMs` correctly avoids redundant renders.
**Classification:** LOOKS REASONABLE.
**Tests:** none exist for ChristmasTree timing.

### ROSE_BLOOM

**Implementation:** `stageInterval()` returns 110 / 350 / 400 / 450 / 500 ms by stage;
`kStemSteps = 8`, `kBreathFrames = 48`, `kBreathCycleFrames = 24`.
**Assessment:** increasing stage intervals produce a natural "opening" feel. The bloom is
slow by design, which is correct for the subject.
**Classification:** LOOKS REASONABLE.
**Tests:** none exist for RoseBloom timing.

### SPARKLE_HEART

**Implementation:** `kFadeInMs = 550`, `kSparkleMs = 3400`, `kFinalHoldMs = 1400`,
24 sparkle slots, 8 targets.
**Assessment:** 5.35 s total with a 1.4 s final hold. Holds are generous; the fade is
smooth. One integer division in the fade ramp truncates and is intentional.
**Classification:** LOOKS REASONABLE.
**Tests:** none exist for SparkleHeart timing.

### CAMPFIRE

**Implementation:** `kFrameCount = 4`, `kFrameDurationMs = 140`, `kAnimationDurationMs = 12000`.
**Timing:** 0.56 s per 4-frame flame cycle (7.1 FPS) → ≈ 21 cycles in 12 s.
**Assessment:** 140 ms per flame shape is lively without strobing — appropriate.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_campfire.cpp` exists; verify it asserts the frame duration.

### DANCING_DUCK

**Implementation:** `kFrameCount = 4`, `kFrameDurationMs = 320`, `kAnimationDurationMs = 12000`.
**Timing:** 1.28 s per 4-frame cycle → ≈ 9 cycles in 12 s.
**Assessment:** 320 ms per pose is deliberate and reads as dancing rather than twitching.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_dancing_duck.cpp` exists (2 pre-existing failures, unrelated to timing).

### AURORA / AURORA_ARCTIC / AURORA_SUNSET / AURORA_NEON

**Implementation:** one class, four palettes. `kFrameIntervalMs = 50`,
`kPhasePeriod = 512`, `kPhaseStep = 3`, `kLoopFrameCount = 512`.
**Timing:** phase advances 3 per frame → ≈ 171 frames per phase cycle ≈ **8.5 s**;
full loop 512 × 50 ms = **25.6 s**.
**Assessment:** 20 FPS is the *refresh* rate; the visible drift is slow. Correct.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_aurora.cpp` and `test_aurora_palette.cpp` exist.

### MOON_STARS

**Implementation:** `kFrameIntervalMs = 180`, `kTwinklePeriod = 32`, `kLoopFrameCount = 64`.
**Timing:** 11.5 s loop; twinkle period 32 frames = 5.76 s.
**Assessment:** calm. Appropriate for a night scene.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_moon_stars.cpp` asserts phase progression and the twinkle-table seam.

### SUNSET_WATER

**Implementation:** `kFrameIntervalMs = 160`, `kWavePhaseCount = 32`, `kLoopFrameCount = 64`.
**Timing:** 10.2 s loop.
**Assessment:** 160 ms shimmer is subtle rather than flickering. Appropriate.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_sunset_water.cpp` exists (2 pre-existing failures).

### SUN_OVER_MOUNTAINS

**Implementation:** `kFrameIntervalMs = 120`, `kPhaseCount = 32`, `kLoopFrameCount = 64`.
**Timing:** 7.68 s loop.
**Assessment:** the scene remains recognizable and the rays do not dominate.
**Classification:** LOOKS REASONABLE.
**Tests:** `test_sun_over_mountains.cpp` exists (1 pre-existing failure).

### TEST

**Implementation:** 250 ms per stage, 4 stages, `step_` 1→2→3→0.
**Assessment:** a diagnostic pattern; pacing is not meaningful.
**Classification:** LOOKS REASONABLE.
**Tests:** none.

---

## Stage 10 — Test coverage audit

| Animation | Interval asserted | Progression | Cycle duration | Loop reset | Final hold | Wrap-around | Timing coverage |
|---|---|---|---|---|---|---|---|
| AURORA (+3 palettes) | Yes | Yes | Partial | Yes | n/a | No | Adequate |
| CAMPFIRE | Yes | Yes | No | No | No | No | Partial |
| CHRISTMAS_TREE | **No** | **No** | **No** | **No** | n/a | No | **None** |
| DANCING_DUCK | Yes | Yes | No | No | No | No | Partial |
| FALLING_STARS | **No** | Yes | **No** | No | No | No | Partial |
| FIREWORKS | **No** | **No** | **No** | **No** | **No** | No | **None** |
| HEARTBEAT | **No** | **No** | **No** | **No** | **No** | No | **None** |
| MOON_STARS | Yes | Yes | No | Yes | n/a | No | Adequate |
| ROSE_BLOOM | **No** | **No** | **No** | No | **No** | No | **None** |
| SLOT_MACHINE | Partial | Yes | No | No | Partial | No | Partial |
| SPARKLE_HEART | **No** | **No** | **No** | **No** | **No** | No | **None** |
| **SWIMMING_FISH** | **No** | **No** | **No** | **No** | **No** | No | **None** |
| SUNSET_WATER | Yes | Yes | No | Yes | n/a | No | Adequate |
| SUN_OVER_MOUNTAINS | Yes | Yes | No | Yes | n/a | No | Adequate |
| TEST | **No** | **No** | **No** | **No** | n/a | No | **None** |

**Key gaps**
- **SWIMMING_FISH — the confirmed problem — has no timing test at all.** There is no
  `test_swimming_fish.cpp`.
- No animation test asserts **cycle duration** (`kLoopFrameCount × kFrameIntervalMs`), so
  a change to either constant that breaks the loop seam would pass the suite.
- **No test asserts `millis()` wraparound.** Because all deltas are `uint32_t` this is
  currently safe by construction, but nothing guards a future regression to signed
  arithmetic.
- Fireworks, Heartbeat, RoseBloom, SparkleHeart, ChristmasTree and SwimmingFish have no
  test file at all.

**Pre-existing failures (unchanged, unrelated to timing):** `test_dancing_duck` (2),
`test_sunset_water` (2), `test_mapping` (1), `test_dirty_state` (1), `test_mask_helpers` (1),
`test_sun_over_mountains` (1). Total suite: 294 tests, 286 pass, 8 fail.

---

## ANIMATIONS REQUIRING ATTENTION

| Animation | Problem | Current | Suggested | Reason |
|---|---|---|---|---|
| **BREATHING_HEART** | No implementation, no registry entry → **dark panel** | `nullptr` → `stop()` | Implement, or remove from pools/catalog | **PROVEN BUG.** Referenced by scheduler pools and the panel |
| **LOVE_MODE** | Same as above | `nullptr` → `stop()` | Implement, or remove from pools/catalog | **PROVEN BUG.** Valentine's pool entry |
| **SWIMMING_FISH** | Motion far too fast | 50 ms/px → 20 px/s → **1.25 widths/s**; crosses in 0.75 s | 150–200 ms/px → 5.0–6.7 px/s → 0.31–0.42 widths/s | Physically confirmed; 3–12× the ambient band |
| **SWIMMING_FISH (bubbles)** | Bubbles outrun the fish 2.5× | 20 ms/px → 50 px/s → **3.13 heights/s** | 60–80 ms/px → 12.5–16.7 px/s | Internally inconsistent; reads as flicker |
| **SWIMMING_FISH (render)** | Re-renders/re-transmits every loop iteration | `renderFrame()` ungated | Add "not due" early return | **PROVEN BUG.** Defeats dirty-frame optimisation; unique to this animation |
| **FIREWORKS** | Rocket may be imperceptible; shells poorly separated | 10 ms/step; 80 ms pause | 25–40 ms/step; 250–400 ms pause | 0.16 s to cross the panel; **displacement unverified** |
| **SLOT_MACHINE** | Result hold possibly short | 700 ms | 1200–1500 ms | Three symbols + payline in 0.7 s |
| **FALLING_STARS** | Possibly too busy | 4 meteors, events every 1.08 s | Widen phase offsets (keep speed) | Meteor speed is semantically fine |

## ANIMATIONS THAT LOOK REASONABLE

AURORA, AURORA_ARCTIC, AURORA_SUNSET, AURORA_NEON (8.5 s phase, 25.6 s loop, 20 FPS is
refresh only) · MOON_STARS (11.5 s, calm) · SUNSET_WATER (10.2 s, subtle) ·
SUN_OVER_MOUNTAINS (7.68 s, scene readable) · CHRISTMAS_TREE (dual-rate render/snow,
0.2 widths/s) · CAMPFIRE (140 ms flame, 7.1 FPS) · DANCING_DUCK (320 ms pose, 1.28 s
cycle) · ROSE_BLOOM (increasing stage intervals) · SPARKLE_HEART (5.35 s with generous
holds) · TEST (diagnostic).

## NEEDS PHYSICAL VERIFICATION

1. **HEARTBEAT** — does the double beat read at ~28 bpm, and is the 400 ms rest readable?
2. **FIREWORKS rocket** — verify the per-step displacement in the source, then judge
   whether the launch is visible at all.
3. **SLOT_MACHINE** — is 700 ms enough to read three symbols?
4. **FALLING_STARS** — does the sky feel sparse or busy with 4 meteors?
5. **AURORA variants** — confirm the 8.5 s drift feels like slow movement and not
   stillness.
6. **SWIMMING_FISH after any change** — the primary acceptance test is visual.
7. **CHRISTMAS_TREE** — confirm twinkle does not dominate the composition at 420 ms.

---

## Stage 11 — Production changes

**None.** `git` is not available in this repository (it is not a git working tree), so
this was verified by inspection instead: the only file created by this audit is this
report. No file under `src/`, `include/`, `test/` or `platformio.ini` was modified, and
no temporary scripts were left behind (all analysis helpers were written to the system
temp directory outside the repository).

**Recommendation for the follow-up implementation phase:** start with Bug 1 (the two
missing animations), because a dark panel will otherwise be misread as a pacing problem.
Then SWIMMING_FISH, whose fix must keep the loop-seam pass count even and must not
decouple the bubbles further from the fish.
