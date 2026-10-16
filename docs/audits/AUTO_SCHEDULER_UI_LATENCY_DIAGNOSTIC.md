# AUTO / SCHEDULER / UI-LATENCY DIAGNOSTIC

Audit date: 2026-10-02. Diagnostic only — **no production code was modified**, no fix was
implemented, nothing was committed.

---

## STAGE 0 — Repository baseline

`git status` / `git log` are **not available**: `E:\pixel_heart` is **not a git repository**
(`fatal: not a git repository (or any of the parent directories): .git`). There is no HEAD to
record and `git diff` cannot be used to prove an empty production diff. Production-code
cleanliness was therefore verified by direct inspection: this audit created files **only** under
`C:\Users\mikio\AppData\Local\Temp\opencode\`. Nothing under `include/`, `src/`, `test/`,
`tests/` or `platformio.ini` was edited.

### Builds

| Target | Result |
|---|---|
| `pio run -e uno` | SUCCESS — RAM 59.1% (1210/2048), Flash 42.5% (13696/32256) |
| `pio run -e xiao_esp32c3` | SUCCESS — RAM 13.1% (42884/327680), Flash 66.4% (870460/1310720) |

Note: both targets **failed mid-audit** (FoxAnimation / OrangeCatAnimation / HalloweenGhostAnimation
did not compile against `include/assets/PixelMask.h`), then began passing again once those files were
finished being edited. The repository is being modified concurrently with this audit. The results
above are the **final** state at the time of writing.

### Tests

`pio test -e native` cannot run on this machine: `platformio.ini` `[env:native]` invokes bare `g++`,
and **no `g++`/`gcc` exists on PATH**. To still obtain a real baseline, every
`test/test_*.cpp` was compiled and run individually with `zig c++` (clang 21) from the repo's
`.venv`, linking the same Unity copy PlatformIO uses. Each test file is built as its own binary
because each one defines its own `main()`.

```
pass=31  fail=6  buildfail=0  total=37
```

Pre-existing failures, **all unrelated to AUTO / scheduler / control / time**:

| Test | Failure | Nature |
|---|---|---|
| `test_dancing_duck` | `Expected 160 Was 320`; `Expected 3 Was 2` | animation pacing constants |
| `test_dirty_state` | `Expected 1 Was 2` (show count) | fake-strip show accounting |
| `test_mapping` | `Expected 255 Was 240` (`xyToIndexStatic(15,15)`) | **stale test**: `PixelMatrix.h:82` asserts `== 240`, the test expects 255 |
| `test_mask_helpers` | `Expected 60 Was 35` | `scaleColor(RosePink,128)` — stale expectation vs current palette |
| `test_sun_over_mountains` | mountain shape/height | rendering content |
| `test_sunset_water` | shimmer seam, sun contrast | rendering content |

**All control / scheduler / panel / time / playback / AUTO tests pass**, including
`test_control_pipeline` (7), `test_playback_policy` (7), `test_scheduler_schedule` (12),
`test_scheduler_playlists` (13), `test_scheduler_special_dates` (16), `test_panel_render` (35),
`test_panel_auth` (17), `test_time_conversion` (7), `test_manual_override` (8),
`test_animation_ordinals` (5).

**Critical gap identified:** every one of those passing tests exercises `SELECT_ANIMATION`
(AUTO → click animation → MANUAL). **No test in the repository exercises
`SET_MODE(AUTO)`.** That is precisely the operation that is failing on the physical device.

---

## Symptom 1 — AUTO freezes

### Exact reproduction

Deterministic, using the production objects composed exactly as `App` composes them
(`ManualTimeProvider` → `ManualOverride` → `Scheduler` → `AnimationManager` → `PlaybackPolicy` →
`ControlPipeline` → `PixelMatrix`), driving them through `ControlPipeline::apply()` exactly as
`App::applyControl()` does.

```
MANUAL, current = SUN_OVER_MOUNTAINS, advanced 20 ticks into mid-animation
clock = 2026-10-02 01:00:00 local, valid = true
exactly ONE ControlRequest{action = SET_MODE, mode = AUTO}
then production-equivalent ticks at +150 ms
```

Result:

```
shows 20 -> 20 -> 20      STOPPED
mode=0(AUTO) manualActive=0 timeValid=1 daypartMin=60
appliedReason=2 (QUIET_HOURS) stopCount=1 curAnim=NONE playing=0
```

The same result is reproduced for `MOON_STARS`, `CAMPFIRE` and `AURORA` as the MANUAL animation,
and for `PlayMode::ONCE`, `PlayMode::LOOP` and `PlayMode::HOLD` alike.

At 09:00, 13:00, 21:00 and 06:02 the identical sequence yields `ADVANCING`. **The failure is
specific to the quiet-hours window 00:00–05:59.**

### Root cause (PROVEN)

`Scheduler::evaluate()` returns `AnimationId::NONE` for quiet hours
(`src/scheduling/Scheduler.cpp:144-149`). `PlaybackPolicy::tick()` handles `NONE` at
`src/scheduling/PlaybackPolicy.cpp:83-92`:

```cpp
if (isDefaultDecision(desired)) {
    if (manager_.isPlaying() || manager_.currentAnimation() != AnimationId::NONE) {
        manager_.stop();          // <-- stops the animation
        ++stopCount_;
    }
    applied_ = desired;
    hasApplied_ = true;
    return;                       // <-- returns; matrix is never touched
}
```

`AnimationManager::stop()` (`src/core/AnimationManager.cpp:35-44`) nulls `currentAnimation_` and sets
`currentAnimationId_ = NONE`. From then on `AnimationManager::tick()` returns immediately
(`AnimationManager.cpp:46-49`, `if (!isPlaying_ || currentAnimation_ == nullptr) return;`), so no
animation ever calls `PixelMatrix::setPixel()` again.

**Nothing in the entire codebase clears or re-transmits the matrix when playback stops.** `matrix_.clear()`
appears exactly twice in production code, both in `App::begin()` (`src/App.cpp:70`, `src/App.cpp:317`).
Grepping `src/` for `matrix.clear()` finds hits only inside animation `renderFrame()` methods.

The NeoPixel strip keeps whatever was last transmitted. Therefore:

> **Clicking "Automatyczny" during 00:00–05:59 stops animation production and leaves the
> last-transmitted MANUAL frame latched on the LEDs. The result is a physically frozen picture.**

### Freeze mechanics — which of A…H is it

| Candidate | Verdict |
|---|---|
| A. `animation.update()` stops being called | **YES — this is the mechanism** |
| B. active Animation pointer becomes null | **YES** (`AnimationManager::stop()`, `currentAnimation_ = nullptr`) |
| C. active stopped, no replacement starts | **YES** (consequence of A+B) |
| D. scheduler returns NONE | **YES — the trigger** |
| E. same animation chosen but `start()` not called | no |
| F. HOLD / completed state | no — every AUTO decision is `PlayMode::LOOP`; the OFF/LOOP/HOLD variants all fail identically |
| G. `dirty_` stays false forever | **YES as a symptom**, not a cause: `showIfDirty()` (`PixelMatrix.cpp:119-126`) correctly declines to re-send an unchanged frame |
| H. `App::tick` stops calling `AnimationManager::update()` | **NO** — `App.cpp:267` calls it unconditionally every tick |
| I. scheduler owns playback but does not update per tick | no — `playbackPolicy_.tick()` runs once per `App::tick` |

So the freeze is **D → C → A → G**, in that causal order.

### Additional freeze class (independent, same family)

`PlaybackPolicy::tick()` lines 60-67: when the scheduler asks for an animation that
`AnimationRegistry` cannot render, the policy **records the decision and returns without touching
`AnimationManager`**. Proven:

```
clock = 2027-02-14 19:00 (Valentine's) -> scheduler wants LOVE_MODE
registry LOVE_MODE = nullptr (no implementation)
MANUAL MOON_STARS playing -> SET_MODE(AUTO)
  sched = Tryb miłosny   applied = Tryb miłosny
  mgr   = Księżyc i gwiazdy   playing = 1   unavailableCount = 1
```

The panel will report `Tryb miłosny` and the LEDs keep animating `Księżyc i gwiazdy` forever.
`unavailableCount` increments but nothing corrects it. `BREATHING_HEART` (id 2) has the same hole.

---

## Symptom 2 — UI requires two clicks

### Classification: **D — AUTO-SPECIFIC STATE BUG** (plus E as a contributing factor)

The previous "stale panel" regression is **NOT** present. The `ControlPipeline::apply()` ordering
introduced by the earlier fix is intact and correct, and I could not reproduce a stale render.

### POST/queue/redirect timeline

`App::tick()` (`src/App.cpp:245-282`), ESP32 build, exact order:

```
1. tickNetwork(now)                                  App.cpp:246
2. applyControl(lanService_.consumeControlRequest())  App.cpp:257-260
     -> ControlPipeline::apply(request, now)          ControlPipeline.h:59
          mutates UserSettings / ManualOverride
          policy_.tick(now)                           ControlPipeline.h:140
     -> matrix_.setBrightness / setRotation / NVS save  App.cpp:152-170
3. publishLanStatus(now)                              App.cpp:263
     -> lanService_.setStatusAnimation(visibleAnimation())
     -> publishPanelState()  (mode / brightness / rotation)
4. lanService_.tick()                                 App.cpp:264
     -> gLanServer.handleClient()  <= serves AT MOST ONE HTTP request
5. animationManager_.tick(now, matrix_)               App.cpp:267
6. matrix_.showIfDirty()                              App.cpp:268
7. rate-limited heartbeat                             App.cpp:272-281
```

Actual timeline for one click:

| Event | Tick | State |
|---|---|---|
| browser POSTs `/mode` | T | arrives inside `handleClient()` at step 4 of tick T |
| Basic Auth accepted | T | `Esp32LanService::authorize()`, `Esp32LanService.cpp:226-252` |
| `queueControl(SET_MODE)` | T | writes `pendingControl_` (`Esp32LanService.cpp:196-199`) |
| 303 `Location: /` sent | T | `redirectToPanel()`, `Esp32LanService.cpp:260-265` |
| request consumed | **T+1** | `consumeControlRequest()`, `App.cpp:258` |
| settings mutated, override cleared | T+1 | `ControlPipeline.h:67-80` |
| playback settled | T+1 | `policy_.tick(now)`, `ControlPipeline.h:140` |
| panel snapshot published | T+1 | `publishLanStatus`, `App.cpp:263` |
| redirected GET `/` served | T+1 or later | `handleClient()` at step 4 of T+1 |

**The GET always renders post-settle state.** `WebServer::handleClient()` serves exactly one request
per call and then discards the client (`framework-arduinoespressif32/libraries/WebServer/src/WebServer.cpp:275-342`,
specifically `if (!keepCurrentClient) { _currentClient = WiFiClient(); _currentStatus = HC_NONE; }`).
`App::tick()` calls it once. Therefore at most one HTTP request is served per App tick, and the
303's follow-up GET lands on tick T+1 **after** step 2 and step 3 of that same tick.

Queue latency: **exactly 1 tick**. Not a race.

### Answers to the Stage-1 questions

1. **Synchronous or deferred?** Deferred by design — HTTP handlers only record a request; the main
   loop applies it. But the *effect* is fully settled before the panel is published, so the
   deferral is invisible to the user.
2. **Does POST return before consumption?** Yes, by ~1 tick. Harmless (see above).
3. **303 immediately after queueing?** Yes (`Esp32LanService.cpp:276`, `301`, `319`, `331`, `341`).
4. **Can GET / precede consumption?** **No**, proven above.
5. **Can the panel render stale state?** **No** for mode/animation/brightness/rotation.
6. **Can physical LED state and panel state disagree?** **YES — this is proven and is the real bug.**
   `ControlPipeline::visibleAnimation()` returns `manager_.currentAnimation()` (`ControlPipeline.h:149`),
   which becomes `NONE` after `stop()`, while the LEDs still hold the previous frame. Measured:
   `visible(panel) = Wyłączone`, LEDs = last SUN frame.
7. **Is `queueControl()`'s return value checked?** It is `void` — nothing to check. Every handler
   calls `redirectToPanel()` unconditionally, including when the request was rejected as invalid
   (`handleAnimation`, `Esp32LanService.cpp:291-301`).
8. **Queue capacity?** **1.** `Esp32LanService::pendingControl_` is a single `ControlRequest`
   (`Esp32LanService.h:110`).
9. **Overflow behaviour?** `queueControl()` unconditionally overwrites
   (`Esp32LanService.cpp:196-199`) — silent drop, no flag, no error, no log.
10. **Can requests be overwritten/dropped?** Yes in principle. Demonstrated: queueing `CAMPFIRE`
    then `FIREWORKS` in one tick yields `FIREWORKS`; `CAMPFIRE` vanishes.
    **But see below — this cannot happen on the real device.**
11. **Multiple requests per tick?** **No.** One, by the `handleClient()` analysis above.
12. **App loop cadence?** `loop() { app.tick(millis()); }` (`src/main.cpp:9-11`), no delay. `App::tick`
    contains no blocking call. Cadence is therefore CPU-bound and very fast; a tick is on the order of
    microseconds. When no HTTP client is connected, `handleClient()` executes `delay(1)`
    (`WebServer.cpp:279-281`, `_nullDelay` defaults to `true` at `WebServer.cpp:50`), which bounds the
    idle loop at roughly 1 kHz.

### Why the two-click observation is real anyway

For the specific pair of clicks the user describes, both clicks are genuinely identical no-ops:

```
MANUAL SUN mid-animation, clock 01:00
click 1 (Automatyczny): mode=AUTO  mgr=NONE  shows 5  FROZEN
click 2 (Automatyczny): mode=AUTO  mgr=NONE  shows 5  IDENTICAL
```

`ControlPipeline::apply()` for `SET_MODE`:
- `outcome.persist = (settings_.mode != request.mode)` → **false on the second click**, so NVS is
  not even written (`ControlPipeline.h:68`);
- `manual_.clear()` again (`ControlPipeline.h:78`);
- `policy_.tick(now)` again → `hasApplied_ && sameDecision(desired, applied_)` is true, so
  `PlaybackPolicy.cpp:69-81` returns without doing anything.

So the second click can never help. What the user perceives as "needs two clicks" is:

1. **D-class:** the request *is* consumed and playback *does* settle — to `NONE`. The panel then
   truthfully reports `Wyłączone`, which reads as "nothing happened", so the user clicks again.
   Both clicks end in the same frozen state.
2. **E-class, proven by control flow:** the very first POST after a browser has no cached Basic
   credentials is **discarded**. `authorize()` calls `requestAuthentication()` and `return`s
   *before* `queueControl()` (`Esp32LanService.cpp:246-251` runs before lines 272-276). The browser
   then shows the dialog; if the user dismisses it, `PanelAuth::kAuthFailureBody` renders a page with
   `<meta http-equiv="refresh" content="1;url=/">` (`PanelAuth.h:37-42`) which navigates back to the
   panel, so the click genuinely vanishes. This is a real "first click does nothing" path, but only
   for the first authenticated request of a browser session.

**Not proven / explicitly ruled out:** queue overwrite as the cause of the two-click symptom.
Because `handleClient()` serves one request per tick and `App::tick()` calls it once, two rapid
clicks always land on two different ticks and both survive. This was verified against the real
library source, not assumed.

---

## Symptom 3 — Sun at 01:00

### Scheduler input

`NtpTimeProvider` (`src/time/NtpTimeProvider.cpp`), `begin()` called from `App::tickNetwork()`
once `networkManager_.isConnected()` (`App.cpp:214-216`):

```cpp
configTzTime("CET-1CEST,M3.5.0,M10.5.0/3",
             "pool.ntp.org", "time.google.com", "time.cloudflare.com");
```

`now()` reads `time(nullptr)` then `localtime_r()`, and marks the snapshot invalid unless
`year >= 2024` (`TimeConversion::kMinCredibleYear`, `TimeConversion.h:16`).

### Local time

The scheduler sees the **same** `ClockSnapshot` the panel renders, because `App::publishLanStatus`
(`App.cpp:121`) and `Scheduler::evaluate` (`Scheduler.cpp:124`) call `now()` on the **same
`NtpTimeProvider` instance** (`App.cpp:28`, `App.cpp:64`). There is only one `TimeProvider` in the
composition root. **A panel/scheduler hour mismatch is architecturally impossible.**

### Daypart

`SchedulerConfig` (`SchedulerConfig.h:15-16`): quiet hours `00:00`–`06:00`.
`isQuietMinute()` (`Scheduler.cpp:93-107`) handles the wrap correctly with `||`
(`minuteOfDay >= start || minuteOfDay < end`). Verified: `00:00=1, 05:59=1, 06:00=0`.

### Playlist

At 01:00 `ScheduleTable::resolveEntry(60)` returns row 4, the overnight `20:00 → 06:00` row with
`PlaylistId::NIGHT`. Verified: `ScheduleTable::contains(kSchedule[4], …)` is
`19:59=0, 20:00=1, 23:00=1, 00:00=1, 05:59=1, 06:00=0` — correct OR-based wraparound, **no
`hour >= 22 && hour < 6` bug anywhere in this repository**. But the row is masked: quiet hours are
checked *before* the schedule table (`Scheduler.cpp:144` precedes `Scheduler.cpp:152`).

### Selected animation

Measured, with the project's real code path:

```
clock 2026-10-02 01:00:00 valid=1 minuteOfDay=60 quiet=1
schedule row = found (overnight NIGHT playlist, masked by quiet)
special date = none
decision: animationId = NONE(0), priority = TIME_OF_DAY, reason = QUIET_HOURS
```

### Why SUN_OVER_MOUNTAINS

**It was not selected. The scheduler cannot select it at 01:00 under any circumstances.**

Complete enumeration of every production path that can select `SUN_OVER_MOUNTAINS` — a repo-wide
grep finds it in exactly 6 places, 3 of them definitions:

| # | Path | Location | Can it fire at 01:00? |
|---|---|---|---|
| 1 | Schedule row 08:00–12:00 `animation` field | `ScheduleTable.h:42` | **NO** — requires hour ∈ [8,12) |
| 2 | MORNING playlist item index 1 (offset 300–599 s) | `Playlists.h:62` | **NO** — row is 06:00–08:00, so only 06:05:00–06:09:59 |
| 3 | Manual selection `a=18` from the panel | `PanelRenderer.h:226-239` | only in MANUAL |
| 4 | `settings_.animation` restored from NVS | `SettingsStore.h:84-85` | **NO** — only consulted when `mode == MANUAL` (`App.cpp:83-85`) |
| 5 | Invalid-time fallback | `Scheduler.cpp:109-116` | **NO** — fallback is `AURORA`, not SUN |
| 6 | Special dates | `SpecialDates.h` | **NO** — no rule references SUN (verified programmatically) |
| 7 | Enum/index accident | — | **NO** — ordinals pinned by `static_assert` (`AnimationId.h:38-59`); see below |

> **At 01:00, SUN_OVER_MOUNTAINS was selected because it was never selected at all.
> The LEDs were showing the last frame that `SunOverMountainsAnimation` had transmitted while it was
> the MANUAL animation, and `AnimationManager::stop()` latched that frame in place. The scheduler's
> actual decision at 01:00 was `NONE` / `QUIET_HOURS`.**

Symptoms 1 and 3 are therefore **one and the same physical event**, observed from two angles: a
frozen sun picture during the quiet-hours window, with the panel (correctly) reporting
`Wyłączone`.

### Exact states required for a genuine scheduler-selected SUN at ~01:00

If the panel genuinely *rendered* `Słońce nad górami` at 01:00, one of these must hold, and none is
produced by the current code:

1. The clock was **not** local — the panel and scheduler share one clock, so this would have to be
   `localtime_r()` returning UTC, which requires `configTzTime()` to have not run. `begin()` runs
   before any `now()`, and `now()` returns invalid when `!started_` (`NtpTimeProvider.cpp:63-65`).
   Ruled out by construction.
2. The persisted NVS `set_mode` byte was `MANUAL` with `set_anim = 18`, so boot re-armed the
   override (`App.cpp:83-85`). Then SUN would be playing in **MANUAL**, not AUTO.
3. The system clock held a plausible-but-wrong civil date/time ≥ 2024. Both panel and scheduler
   would then agree on the wrong time, so the panel would *not* show 01:00.

**Conclusion: not reproducible as a scheduler decision.** If a device log ever shows
`scheduler decision = SUN_OVER_MOUNTAINS` with `minuteOfDay < 360`, the time source is lying, and
`TimeConversion::kMinCredibleYear` is the guard that failed to catch it.

---

## Control pipeline

```
browser <form method=POST>
  -> WebServer route (Esp32LanService.cpp:123-127)
  -> Esp32LanService::authorize()                     .cpp:226-252
       PanelAuth::requiresAuthForMethodToken("POST") == true
       gLanServer.authenticate(kUsername, kPassword)
       on failure: requestAuthentication(401) + RETURN (request LOST - see Symptom 2 / E-class)
  -> handler parses arg, validates                    .cpp:267-342
  -> queueControl(action); pendingControl_.<field> = value   .cpp:196-199   [1 slot, overwrite]
  -> redirectToPanel() -> 303 Location: /             .cpp:260-265
  ...
  === next App::tick ===
  -> App::tickNetwork(now)                            App.cpp:246
  -> App::applyControl(lanService_.consumeControlRequest(), now)   App.cpp:257-260
       -> ControlPipeline::apply(request, now)        ControlPipeline.h:59-143
            mutate settings_ / manual_
            policy_.tick(now)                         ControlPipeline.h:140
            => PlaybackPolicy::tick -> Scheduler::evaluate -> AnimationManager::play/stop
       -> matrix_.setBrightness / setRotation          App.cpp:152-163
       -> settingsStore_.save(settings_)               App.cpp:165-170
  -> App::publishLanStatus(now)                        App.cpp:263
       setStatusAnimation(controlPipeline_.visibleAnimation())  (= manager_.currentAnimation())
       publishPanelState(): mode / brightness / rotation
       [1 Hz gate] ssid / address / time
  -> lanService_.tick() -> gLanServer.handleClient()   App.cpp:264
       serves the POST (this tick) or the redirected GET (next tick)
  -> animationManager_.tick(now, matrix_)              App.cpp:267
  -> matrix_.showIfDirty()                             App.cpp:268
```

Panel actions traced individually:

| Action | Route | Handler | Validation | Mutation | Physical effect |
|---|---|---|---|---|---|
| AUTO/MANUAL | `POST /mode` (`m=auto\|manual`) | `handleMode` `.cpp:267` | none | `settings_.mode`, `manual_.clear()` / `activateUntilCancelled()` | via `policy_.tick()` |
| animation | `POST /animation` (`a=<id>`) | `handleAnimation` `.cpp:279` | `0 < a < ANIMATION_ID_COUNT` **and** `AnimationCatalog::isUserSelectable` | `settings_.animation`, `settings_.mode = MANUAL`, override armed | `manager_.play()` |
| brightness | `POST /brightness` (`level=1..5`) | `handleBrightness` `.cpp:304` | `BrightnessLevels::isValidLevel` | `settings_.brightnessPercent` | `matrix_.setBrightness()` |
| rotation | `POST /rotation` (`rotation=<deg>`) | `handleRotation` `.cpp:322` | `isDisplayRotationValid` | `settings_.rotation` | `matrix_.setRotation()` → `clear()` → dirty |
| Wi-Fi change | `POST /wifi/change` | `handleWifiChange` `.cpp:149` | auth only | `wifiChangeRequested_` flag | none; `App::tickNetwork` `.cpp:225-229` stops LAN and starts the portal on a later tick |

---

## App tick ordering

ESP32 (`src/App.cpp:245-282`), exact:

1. `tickNetwork(now)` — `networkManager_.tick()`, start NTP once connected, consume Wi-Fi-change flag, `(re)begin` LAN server
2. `applyControl(consumeControlRequest(), now)` — consume **one** request, apply, **settle playback**
3. `publishLanStatus(now)` — control state every tick; telemetry 1 Hz
4. `lanService_.tick()` — `handleClient()`, **at most one HTTP request**
5. `animationManager_.tick(now, matrix_)`
6. `matrix_.showIfDirty()`
7. heartbeat every 2 s

AVR (`src/App.cpp:336-342`): `playbackPolicy_.tick()` → `animationManager_.tick()` → `showIfDirty()`.

The previous fix's required invariant holds: **control → settle → publish → serve HTTP.** It was
verified against `ControlPipeline.h:140` and `App.cpp:263-264` and must not be reordered.

---

## Scheduler precedence

Derived from `Scheduler::evaluate()` (`src/scheduling/Scheduler.cpp:118-162`):

| Rank | Condition | Output | `priority` | `reason` | Fallback |
|---|---|---|---|---|---|
| 1 | `manual_.active(nowMs)` | `manual_.animationId` | `MANUAL` (100) | `MANUAL_OVERRIDE` | — |
| 2 | `!snapshot.valid` | `AURORA` | `FALLBACK` (10) | `FALLBACK` | — |
| 3 | `SpecialDates::resolve(month, day, minute, quiet)` | rule's animation | `SPECIAL_EVENT` (70) | `SPECIAL_EVENT` | — |
| 4 | `isQuietMinute(minuteOfDay)` | `NONE` | `TIME_OF_DAY` (30) | `QUIET_HOURS` | — |
| 5 | `ScheduleTable::resolveEntry(minuteOfDay)` | row `animation`, or `Playlists::itemAt()` if `playlist != NONE` | `TIME_OF_DAY` | `SCHEDULE` | row's own `animation` if playlist exhausted |
| 6 | no row matches | `AURORA` | `FALLBACK` | `FALLBACK` | — |

Then `PlaybackPolicy::tick()` (`PlaybackPolicy.cpp:54-101`) applies a second, independent filter:

| Condition | Action |
|---|---|
| desired is not `NONE` **and** `AnimationRegistry::getAnimation(id) == nullptr` | record `applied_`, **do not touch the manager** — panel and LEDs silently diverge |
| `sameDecision(desired, applied_)` | no restart; re-`play()` only if LOOP/HOLD and the manager released it |
| desired is `NONE` | `manager_.stop()` — **frame is not cleared** |
| otherwise | `manager_.play(desired, LOOP, now)` |

`evaluate()` is a pure function of the clock and the override. It has no cache and no
"last decision" state, so there is no stale-scheduler-decision mechanism anywhere in this design.

---

## Time architecture

Single source. `App::clock_` (`NtpTimeProvider` on ESP32, `ManualTimeProvider` on AVR) is the only
`TimeProvider`, injected into `Scheduler` by reference at construction (`App.cpp:28`, `App.cpp:64`).
The scheduler has no access to any other clock and never calls `millis()`.

`ClockSnapshot` is a POD: `year, month, day, hour, minute, second, weekday, valid`. `weekday` is
derived from `tm_wday` when trusted, or from `civilToDays()`/`weekdayFromDays()` (Howard Hinnant's
algorithm, `ClockSnapshot.h:29-48`) when not.

---

## Timezone / DST

| Item | Value |
|---|---|
| Configured TZ | `"CET-1CEST,M3.5.0,M10.5.0/3"` — `src/time/NtpTimeProvider.cpp:15` |
| Mechanism | `configTzTime()` → POSIX TZ installed into libc; **no season logic anywhere in this project** |
| Winter | CET = UTC+1 (POSIX sign is inverted: `CET-1`) |
| Summer | CEST = UTC+2 |
| DST start | last Sunday of March, 02:00 local |
| DST end | last Sunday of October, 03:00 local |
| Conversion | `time()` → `localtime_r()` → `TimeConversion::fromBrokenDownTime()` |
| Validity guard | `snapshot.valid = plausible && year >= 2024` (`TimeConversion.h:37-45`) |

**October 2, 2026 falls before the last Sunday of October 2026 (2026-10-25), so CEST (UTC+2) is
correct for Poland.** The firmware produces that automatically through libc.

No hardcoded UTC+1, no hardcoded UTC+2, no `gmtime()`, no custom civil-time conversion.

**Panel-vs-scheduler mismatch check (the critical one):** *impossible by construction.* Both call
`now()` on the same object in the same tick. `App.cpp:121` and `Scheduler.cpp:124`. There is no
code path by which the panel could show 01:00 while the scheduler sees 23:00, 00:00 or 02:00.

---

## Daypart ranges

Real values from `include/scheduling/ScheduleTable.h:40-48` and `SchedulerConfig.h:15-16`:

| Row | Range (minute of day) | Clock | Animation | Playlist |
|---|---|---|---|---|
| quiet | 0 – 360 | **00:00 – 06:00** | `NONE` | — |
| 0 | 360 – 480 | 06:00 – 08:00 | `NONE` | `MORNING` |
| 1 | 480 – 720 | 08:00 – 12:00 | `SUN_OVER_MOUNTAINS` | — |
| 2 | 720 – 1020 | 12:00 – 17:00 | `SUNSET_WATER` | — |
| 3 | 1020 – 1200 | 17:00 – 20:00 | `NONE` | `EVENING` |
| 4 | 1200 – 360 | 20:00 – 06:00 (wraps) | `NONE` | `NIGHT` |

Boundary semantics: **inclusive start, exclusive end** (`ScheduleTable.h:57-64`).
Overnight wrap uses `||` (`ScheduleTable.h:58-61`). No reversed comparisons, no midnight-wrap bug,
no unsigned arithmetic, no `&&`/`||` error. Verified numerically.

Measured decision sweep (2026-10-02):

```
00:59 -> NONE (QUIET_HOURS)     12:00 -> SUNSET_WATER
01:00 -> NONE (QUIET_HOURS)     17:59 -> SUNSET_WATER
05:59 -> NONE (QUIET_HOURS)     18:00 -> CAMPFIRE
06:00 -> AURORA                 21:59 -> CAMPFIRE
06:05 -> SUN_OVER_MOUNTAINS     22:00 -> MOON_STARS
08:00 -> SUN_OVER_MOUNTAINS     23:59 -> CAMPFIRE
11:59 -> SUN_OVER_MOUNTAINS
```

---

## Playlist contents

From `include/scheduling/Playlists.h:48-71`:

| Playlist | Items | Total | Loop |
|---|---|---|---|
| `MORNING` | `AURORA` 300 s, **`SUN_OVER_MOUNTAINS` 300 s** | 600 s | yes |
| `EVENING` | `AURORA_SUNSET` 300 s, `SUNSET_WATER` 180 s, `CAMPFIRE` 300 s | 780 s | yes |
| `NIGHT` | `MOON_STARS` 600 s, `FALLING_STARS` 300 s, `CAMPFIRE` 300 s | 1200 s | yes |

Index is derived from civil time only, never from a counter:

```cpp
secondsSinceRowStart()  Scheduler.cpp:30-43   // handles the overnight wrap
Playlists::itemAt()     Playlists.h:98-124   // offset %= total, then linear scan
```

Consequences:
- **No modulo/index bug.** `offset %= total` only applies when `playlist.loop` is true; all three
  playlists loop, so `itemAt()` never returns `nullptr` for them.
- **No position persistence.** Progress is a pure function of the clock, so it survives reboots,
  `millis()` rollover and NTP corrections. `test_overnight_row_is_continuous_across_midnight`
  covers this and passes.
- **`SUN_OVER_MOUNTAINS` is NOT index 0** of any playlist that AUTO uses at night. It is index 1 of
  `MORNING` only. It is **not** a fallback and **not** a registry default
  (`Scheduler::fallbackDecision()` returns `AURORA`, `Scheduler.cpp:109-116`).
- Playlist `count`/`loop`/item arrays are `constexpr` and indexed by `PlaylistId` ordinal; the
  array is built with `sizeof(...)/sizeof(...)` so it cannot desynchronise from its enum.

---

## AnimationId stability

`include/core/AnimationId.h` pins **every shipped ordinal with a `static_assert`**
(`AnimationId.h:38-59`), and the three new ids were **appended**:

```
NONE=0 HEARTBEAT=1 BREATHING_HEART=2 ROSE_BLOOM=3 FIREWORKS=4 LOVE_MODE=5 CAMPFIRE=6
DANCING_DUCK=7 SLOT_MACHINE=8 SWIMMING_FISH=9 AURORA=10 AURORA_ARCTIC=11 AURORA_SUNSET=12
AURORA_NEON=13 CHRISTMAS_TREE=14 MOON_STARS=15 SUNSET_WATER=16 FALLING_STARS=17
SUN_OVER_MOUNTAINS=18  <-- unchanged
HALLOWEEN_GHOST=19 FOX=20 ORANGE_CAT=21  <-- appended
ANIMATION_ID_COUNT=22
```

Explicitly ruled out:

- **Enum shift:** impossible. New ids are appended after `SUN_OVER_MOUNTAINS`; no existing value moved.
- **Persisted-ordinal drift:** impossible. NVS stores the raw byte (`SettingsStore.h:85`) and the
  ordinals are compile-time asserted. `test_animation_ordinals` (5 tests) passes.
- **Playlist drift:** impossible. `Playlists.h` names animations symbolically, never by ordinal.
- **Registry/table disagreement:** `AnimationRegistry.cpp:99-106` maps `SUN_OVER_MOUNTAINS` to
  `sunOverMountainsAnimation` on ESP32 and `:63-64` on AVR. Both new ids are registered
  (`AnimationRegistry.cpp:101-106`) and both have implementations and Polish names
  (`AnimationNames.cpp:49-54`).
- **Panel id drift:** `PanelRenderer.h:226-239` iterates raw ordinals and posts `a=<raw>`; the
  handler range-checks against `ANIMATION_ID_COUNT` before narrowing (`Esp32LanService.cpp:292`).

**Verdict: not a cause of any of the three symptoms.** `HALLOWEEN_GHOST` was added to
`SpecialDates.h:61` for 31 October with `overridesQuiet = false`, which is correct and consistent
with every other non-New-Year rule.

---

## AUTO transition state machine

State involved:

| Variable | Owner | Meaning |
|---|---|---|
| `settings_.mode` | `App` / NVS | `AUTO` or `MANUAL` |
| `settings_.animation` | `App` / NVS | last MANUAL selection; **not** consulted in AUTO |
| `manual_.mode_` | `ManualOverride` | `INACTIVE` / `TIMED` / `UNTIL_CANCELLED` — the *real* playback owner |
| `PlaybackPolicy::applied_` | `PlaybackPolicy` | last decision applied; compared for change detection |
| `PlaybackPolicy::hasApplied_` | `PlaybackPolicy` | decision cache flag |
| `manager_.currentAnimationId_` | `AnimationManager` | what the LEDs are actually showing |
| `manager_.isPlaying_` | `AnimationManager` | whether `update()` runs at all |
| `clock_.synchronized_` | `NtpTimeProvider` | latched once a credible clock has been seen |

### `SELECT_ANIMATION` vs `SET_MODE(AUTO)`

| | `SELECT_ANIMATION` | `SET_MODE(AUTO)` |
|---|---|---|
| request mutation | `settings_.animation = id`, `settings_.mode = MANUAL` | `settings_.mode = AUTO` only |
| persist | always `true` | only when the mode actually changed |
| `accepted` | `true` | **`false`** (`ControlPipeline.h:60` leaves it false; nothing ever sets it for `SET_MODE`) |
| manual override | `activateUntilCancelled(id, LOOP)` | **`manual_.clear()`** |
| `policy_.tick()` | yes, same call | yes, same call |
| scheduler consulted | no (override short-circuits `evaluate()` at `Scheduler.cpp:120`) | **yes** |
| `start()` on new animation | yes, guaranteed | **only if the decision is not `NONE` and is renderable** |
| `settings_.animation` after | the clicked id | **left stale** (still the old MANUAL id) |
| playing after | yes | **no, if quiet hours** |
| early return specific to the action | yes: the `restart` branch when `settings_.animation == id && mode == MANUAL` (`ControlPipeline.h:95-100`) | no |
| requires valid time | no | **effectively yes** — invalid time yields the `AURORA` fallback, quiet hours yield `NONE` |

**The asymmetry is proven and it is the bug.** `SELECT_ANIMATION` unconditionally guarantees a
playing animation. `SET_MODE(AUTO)` delegates everything to the scheduler and has **no guarantee at
all** — its Outcome carries no `started` flag, no fallback, and no error, so `App::applyControl()`
has nothing to react to. `App.cpp:173-175` just prints `[CTL] mode=AUTO`.

### Answers to the specific `SET_MODE(AUTO)` questions

1. **Does it call `playbackPolicy_.tick()`?** Yes — `ControlPipeline.h:140`, common to all actions.
2. **With what timestamp?** The `now` passed to `App::tick()`, i.e. `millis()`.
3. **Is scheduler state ready?** Yes. `Scheduler` holds no cached state; `evaluate()` is pure
   (`Scheduler.h:16-26`). It re-reads the clock on every call, so there is nothing to be "not ready".
4. **Does it remove the manual override BEFORE scheduler evaluation?** Yes — `manual_.clear()` at
   `ControlPipeline.h:78` runs before `policy_.tick()` at line 140.
5. **Can `evaluate()` return NONE?** **Yes — every minute from 00:00 to 05:59.**
6. **Can the policy decide "nothing changed"?** Yes, but that is not what happens here. On the
   *second* identical AUTO click, `sameDecision()` returns true and `PlaybackPolicy.cpp:69-81`
   returns without acting (`persist` is also false). On the first click the decision differs
   (MANUAL override → `NONE`), so it takes the stop branch.
7. **Can the manager be stopped before a new AUTO animation is started?** **YES — this is exactly
   the observed failure.**
8. **Is there an early return specific to `SET_MODE`?** No.
9. **Does AUTO require `timeValid == true`?** Not explicitly. But the *decision quality* does:
   invalid time ⇒ `AURORA` fallback; valid time in quiet hours ⇒ `NONE`.
10. **If time is invalid, what playback state is left behind?** `AURORA` LOOP, started normally
    (`Scheduler.cpp:109-116`). Verified: `sched=AURORA applied=AURORA mgr=AURORA playing=1`.
    This path is **safe**.

### Auto transition timing matrix

Same production objects, `MANUAL → exactly ONE SET_MODE(AUTO)`, 10 subsequent ticks:

| Local time | Daypart | Playlist | Selected | Started? | Update continues? | Verdict |
|---|---|---|---|---|---|---|
| 00:59 | quiet | — | `NONE` | no | **no** | **FREEZE** |
| **01:00** | **quiet** | — | **`NONE`** | **no** | **no** | **FREEZE (reproduced)** |
| 05:59 | quiet | — | `NONE` | no | **no** | **FREEZE** |
| 06:00 | morning | `MORNING` #0 | `AURORA` | yes | yes | OK |
| 06:05 | morning | `MORNING` #1 | `SUN_OVER_MOUNTAINS` | yes | yes | OK |
| 08:00 | day | — | `SUN_OVER_MOUNTAINS` | yes | yes | OK |
| 11:59 | day | — | `SUN_OVER_MOUNTAINS` | yes | yes | OK |
| 12:00 | day | — | `SUNSET_WATER` | yes | yes | OK |
| 17:59 | evening | `EVENING` | `SUNSET_WATER` | yes | yes | OK |
| 18:00 | evening | `EVENING` #0 | `AURORA_SUNSET` | yes | yes | OK |
| 21:59 | night | `NIGHT` #1 | `CAMPFIRE` | yes | yes | OK |
| 22:00 | night | `NIGHT` #0 | `MOON_STARS` | yes | yes | OK |
| 23:59 | night | `NIGHT` #1 | `CAMPFIRE` | yes | yes | OK |
| invalid clock | — | — | `AURORA` (fallback) | yes | yes | OK |
| 05:59 → 06:00 in AUTO | quiet→morning | — | `NONE` → `AURORA` | yes, on the boundary tick | yes | OK |

**6 hours per day (00:00–05:59) the AUTO button produces a frozen panel.**

The daypart-change-while-AUTO path is healthy: at 05:59 the manager is stopped, at 06:00
`evaluate()` returns `AURORA`, the decision differs from `applied_`, and `play()` fires on the very
next tick. Measured: `05:59 mgr=NONE playing=0` → `06:00 mgr=AURORA playing=1`.

---

## Time validity

| Question | Answer |
|---|---|
| How is time validity known? | `NtpTimeProvider::now()` returns `valid = false` when `!started_`, when `time(nullptr) <= 0`, when `localtime_r()` fails, or when `year < 2024` / field ranges implausible (`TimeConversion.h:37-45`) |
| Does the scheduler run before valid time exists? | Yes, every tick. Boot is never blocked (`App.cpp:73-76`) |
| What hour/date does it see? | `invalidClockSnapshot()`: `0000-00-00`-shaped, `hour=0, minute=0`, `weekday=4`, `valid=false` (`NtpTimeProvider.cpp:23-34`) |
| Is there a fallback? | Yes — `Scheduler::fallbackDecision()` ⇒ `AURORA`, `FALLBACK`/`FALLBACK` |
| Can invalid time select `SUN_OVER_MOUNTAINS`? | **No.** The fallback is `AURORA`. Verified programmatically. |
| Can AUTO freeze waiting for validity? | **No.** `invalid` short-circuits at `Scheduler.cpp:127` *before* the quiet-hours check, so pre-sync the device plays `AURORA`, not `NONE`. Verified: `sched=AURORA applied=AURORA playing=1` |
| Does it re-evaluate when NTP becomes valid? | Yes, on the very next tick, because `evaluate()` is pure. Verified: `AURORA` → `SUNSET_WATER` on the next tick once the clock read 13:00 |

So the time-validity design is **correct and safe**. It is not a contributor to any symptom.

---

## Panel time vs scheduler time

Both derive from `clock_.now()` on the same object:

- panel: `App::publishLanStatus()`, `src/App.cpp:121`
- scheduler: `Scheduler::evaluate()`, `src/scheduling/Scheduler.cpp:124`

There is exactly one `TimeProvider` member (`App.h:52/71`) injected into the `Scheduler` at
construction (`App.cpp:28`, `App.cpp:64`). For a fixed epoch the two are identical by construction,
so a permanent equality test would be tautological rather than informative. Verified by inspection
of both call sites rather than by assertion.

The panel's time string is additionally gated to 1 Hz (`App.cpp:107-110`) and shows
`"not synchronized"` while `valid == false` (`App.cpp:135-137`) — a deliberate, harmless display
cadence difference, not a value difference.

---

## Queue latency

Measured logically, in App ticks, from the real `handleClient()` semantics:

| Quantity | Value |
|---|---|
| POST queued at | tick T |
| consumed + applied + playback settled | tick **T+1** |
| panel published | tick T+1 (step 3) |
| redirected GET served | tick T+1 (step 4) or T+2 if the browser's connection lands later |
| **normal queue latency** | **1 tick** |
| **worst-case queue latency** | **1 tick** — the slot is consumed at the top of every tick, before `handleClient()`, so a request queued during tick T is always taken in T+1 |
| extra tick from scheduler/control ordering | **none** — `policy_.tick()` is inside `ControlPipeline::apply()`, i.e. step 2, *before* step 3 |
| redirect before either? | The 303 response is written during tick T, but its target GET is served at T+1, after the settle and after the publish |

**Worst case is bounded at exactly one tick and the ordering is correct.** There is no latency bug in
this pipeline. An App tick contains no blocking call and no `delay()` outside `handleClient()`'s
1 ms idle yield, so one tick is sub-millisecond — roughly 1 ms at most in practice, which is not
perceivable and cannot explain a "delayed" UI.

---

## Confirmed root causes

### PROVEN ROOT CAUSE 1 — `SET_MODE(AUTO)` during quiet hours freezes the LEDs
- **Variables:** `PlaybackPolicy::applied_` / `hasApplied_`; `AnimationManager::currentAnimation_`,
  `isPlaying_`; `manager_.currentAnimationId_`; `PixelMatrix` framebuffer + `dirty_`.
- **Functions:** `Scheduler::evaluate()` (`Scheduler.cpp:144-149`) → `PlaybackPolicy::tick()`
  (`PlaybackPolicy.cpp:83-92`) → `AnimationManager::stop()` (`AnimationManager.cpp:35-44`) →
  `AnimationManager::tick()` early return (`AnimationManager.cpp:46-49`).
- **Event order:** POST `/mode m=auto` → queue → next tick consume → `manual_.clear()` →
  `evaluate()` returns `NONE`/`QUIET_HOURS` → `manager_.stop()` → **no `matrix.clear()` anywhere in
  production** → `AnimationManager::tick()` no-ops → `showIfDirty()` correctly declines because
  `dirty_ == false` → the strip holds the last MANUAL frame indefinitely.
- **Not a race.** Deterministic, and reproduced for `ONCE`, `LOOP` and `HOLD` and for four different
  MANUAL animations.
- **Missing test:** no repository test covers `SET_MODE(AUTO)`.

### PROVEN ROOT CAUSE 2 — the physical display and the panel disagree after the freeze
- **Variables:** `AnimationManager::currentAnimationId_` (`NONE`) versus the undismissed strip frame.
- **Functions:** `ControlPipeline::visibleAnimation()` (`ControlPipeline.h:149`) →
  `Esp32LanService::setStatusAnimation()` (`App.cpp:100`) → `sendStatusPage()` (`Esp32LanService.cpp:462`).
- **Measured:** panel `Wyłączone`, LEDs showing the last `SUN_OVER_MOUNTAINS` frame.
- This is what makes the user believe the click did nothing and click again.

### PROVEN ROOT CAUSE 3 — the scheduler cannot select `SUN_OVER_MOUNTAINS` at 01:00
- **Enumerated all 7 selection paths**; every one of them requires hour ∈ [6,12) or MANUAL.
- At 01:00 the decision is `NONE` / `QUIET_HOURS` / `TIME_OF_DAY`.
- The observed sun is Root Cause 1's latched frame. Symptoms 1 and 3 are **one event**.

### PROVEN ROOT CAUSE 4 (separate class) — unrenderable scheduler decisions strand playback
- **Variable:** `PlaybackPolicy::applied_` vs `AnimationManager::currentAnimation_`.
- **Function:** `PlaybackPolicy::tick()`, `src/scheduling/PlaybackPolicy.cpp:60-67`.
- **Event order:** `evaluate()` returns `LOVE_MODE` (14 Feb 19:00, `SpecialDates.h:54`) →
  `isRenderable()` false → `applied_ = desired` and **return** → the manager keeps whatever it had,
  forever, while the panel reports `Tryb miłosny`. `unavailableCount` increments but nothing acts on it.
- Affects `LOVE_MODE` (5) and `BREATHING_HEART` (2), which have no registry entry.
- **Not** the cause of the 01:00 observation (LOVE_MODE cannot match 2 October), but it is a real,
  permanent freeze + panel/physical mismatch of exactly the same shape.

### CONTRIBUTING FACTOR 1 — `SET_MODE(AUTO)` has no outcome contract
`ControlPipeline::Outcome` carries `restarted` for `SELECT_ANIMATION` but nothing analogous for
`SET_MODE`. `accepted` stays `false`, `persist` is `false` on a repeat click, and `App.cpp:173-175`
only prints a log line. `App` cannot detect "AUTO settled to nothing" and has no way to report it.

### CONTRIBUTING FACTOR 2 — single-slot request queue with silent overwrite
`Esp32LanService::pendingControl_` (`Esp32LanService.h:110`) has capacity 1;
`queueControl()` (`Esp32LanService.cpp:196-199`) overwrites without any signal. **Latent, not
currently triggered** — `handleClient()` serves at most one request per `App::tick`, so two
requests can never collide. It becomes a real defect the moment the loop is ever changed to drain
more than one HTTP request per tick.

### CONTRIBUTING FACTOR 3 — the first POST after an auth challenge is discarded
`Esp32LanService::authorize()` returns `false` after `requestAuthentication()` (`Esp32LanService.cpp:246-251`),
and every handler returns before `queueControl()`. Combined with the auto-refreshing 401 body
(`PanelAuth.h:37-42`), a dismissed credential dialog produces a genuinely lost click.
Security-correct, but it is a real "first click does nothing" path and the browser cannot
distinguish it from a lost request.

### SUSPICIOUS BUT NOT PROVEN
- `setStatusTime` / `setStatusSsid` gated to 1 Hz (`App.cpp:107-110`) while the page is otherwise
  live. Verified to affect display only, never control state. Could contribute to a *perceived*
  staleness of the "Czas" row but not to the animation behaviour.
- `_nullDelay = true` (`WebServer.cpp:50`) makes `handleClient()` `delay(1)` whenever no client is
  connected, capping the idle loop near 1 kHz. Harmless at these frame intervals (120–320 ms) but it
  is a 1 ms floor on tick cadence.

### EXPLICITLY NOT A CAUSE
- **Queue overwrite** as the two-click cause — impossible at one HTTP request per tick.
- **Panel staleness** — the previous regression is fixed and the ordering holds.
- **Timezone / DST / UTC-vs-local** — single `TimeProvider`, correct POSIX TZ, correct for Poland,
  no mismatch possible.
- **Daypart boundaries** — verified numerically, OR-based wrap is correct.
- **Playlist modulo / index / ordering** — `SUN_OVER_MOUNTAINS` is not index 0 of any AUTO-at-night
  playlist; `itemAt()` cannot return `nullptr` for a looping playlist.
- **AnimationId regression** — all 22 ordinals pinned by `static_assert`; new ids appended only.
- **Registry default / fallback** — fallback is `AURORA`, never `SUN_OVER_MOUNTAINS`.
- **Invalid-time handling** — short-circuits before quiet hours, so pre-NTP AUTO plays `AURORA`.
- **Scheduler caching / stale decision** — `evaluate()` is pure and holds no state.
- **`ControlPipeline` ordering asymmetry causing a lost start** — `SET_MODE` does clear the override
  before evaluating and does settle playback in the same call; the failure is the *decision value*
  (`NONE`), not the *ordering*.

### Are the three symptoms independent?
**No.** Symptoms 1 and 3 are a single physical event: a sun frame latched at the 00:00 quiet-hours
boundary, observed as a "freeze" and as "sun at 01:00". Symptom 2 is the same event seen through
the UI: the request *is* consumed and playback *does* settle — to `NONE` — so nothing visibly
happens, and Root Cause 2 (panel says `Wyłączone`, LEDs say sun) makes that indistinguishable from
a dropped click. The only genuinely independent finding is Root Cause 4 (unrenderable decisions),
which is a second instance of the same architectural shape: **`PlaybackPolicy` is allowed to reach a
settled state that `AnimationManager` cannot represent, and no layer owns the matrix in that case.**

---

## Recommended fix plan — STAGED, NOT IMPLEMENTED

The shared architectural gap: **nothing owns `PixelMatrix` when playback is stopped or
unrepresentable.** `AnimationManager` owns animation lifecycle; `PlaybackPolicy` owns *what*;
`PixelMatrix` owns pixels; but the "no animation running" state has no owner at all.

### Stage 0 — pin the behaviour with tests (no production change)
1. Add a native `test_auto_mode_transition.cpp` asserting, for the `MANUAL → ONE SET_MODE(AUTO)`
   sequence, that after entry the mode is `AUTO`, `manualOverride_.active()` is `false`, the
   scheduler decision is non-`NONE` **at a time chosen outside quiet hours**, the manager is playing,
   and at least two subsequent `update()` calls advance the frame. This is the missing coverage
   identified in Stage 0.
2. Add an explicit test for the failing window: `SET_MODE(AUTO)` at 01:00 asserting
   `currentAnimation() == NONE`, `isPlaying() == false`, **and** documenting what the panel and the
   LEDs each show. Write it as a characterisation test first, so the fix has a red-to-green target.
3. Add `PanelRenderer hour == Scheduler hour` as a plain equality over a fixed `ClockSnapshot` fed
   through the *same* `TimeProvider` instance — a guard against the two ever being wired apart.

### Stage 1 — give the "nothing is playing" state an owner (fixes symptom 1)
4. Decide the intended quiet-hours behaviour explicitly: *dark* (blank the matrix) or *hold the last
   frame*. Do not let it be implied by the absence of code.
5. If *dark*: clear and transmit the matrix on the `NONE` transition. The natural place is a single
   owner of the stopped state — either `AnimationManager::stop()` grows a matrix argument, or `App`
   clears when it observes the `visible → NONE` edge. Prefer the former: `stop()` already knows the
   transition is happening.
6. If *hold*: make it deliberate and documented, and make the **panel** say so, so the LED state and
   the reported state can never disagree again.
7. Whatever is chosen, add the guard so it cannot regress: a test asserting that after any
   `stop()`, either the matrix is blank or the reported state is documented as "held frame".

### Stage 2 — surface the outcome (`SET_MODE` contract)
8. Add an `Outcome` field, e.g. `bool startedPlaying` (or an explicit
   `settledToNothing`), set by `ControlPipeline::apply()` for **every** action, so `App` can log and
   react. Keep `SELECT_ANIMATION`'s existing guarantees intact — do not change its semantics.
9. Keep the settle ordering exactly as it is: `apply()` → `policy_.tick()` → `publishLanStatus()` →
   `lanService_.tick()`. **Do not reorder `App::tick`.** This is the invariant the previous fix
   established and its regression coverage must continue to pass.

### Stage 3 — unrenderable decisions (Root Cause 4)
10. In `PlaybackPolicy::tick()`, when the desired animation is not renderable, either fall back to
    the global fallback (`AURORA`) and actually start it, or leave `applied_` unchanged so the panel
    keeps reporting what the LEDs are really showing. Either is defensible; silently adopting an
    unrenderable decision while doing nothing physical is not.
11. Consider implementing `LOVE_MODE` / `BREATHING_HEART` or removing them from
    `SpecialDates.h:54` so the scheduler cannot request them.

### Stage 4 — control-path robustness (latent, not urgent)
12. Make `queueControl()` report whether it displaced an unconsumed request, and log it. Capacity 1
    is defensible; silent loss is not. This becomes live only if HTTP handling is ever batched.
13. Leave the 401 path alone — it is security-correct. If anything is desired, it is a log line
    noting that a state-changing request was rejected for credentials, so a serial log can
    distinguish "auth dropped the click" from "device ignored the click".

### Out of scope for this fix plan
- `test_mapping` (`Expected 255 Was 240` vs `PixelMatrix.h:82`'s `static_assert` of 240) — a stale
  test expectation, unrelated.
- `test_mask_helpers`, `test_dancing_duck`, `test_sunset_water`, `test_sun_over_mountains`,
  `test_dirty_state` — pre-existing rendering/pacing test drift, unrelated.
- The lack of a native C++ toolchain (`platformio.ini [env:native]` assumes a bare `g++`). Worth
  fixing separately so `pio test` runs on this machine; it does not affect device behaviour.
