#pragma once

#include <stdint.h>

#include "control/AnimationCatalog.h"
#include "control/UserSettings.h"
#include "core/AnimationManager.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/PlaybackPolicy.h"

// The portable core of the panel's control path. ONE click is ONE intent.
//
// This exists as its own unit for two reasons.
//
//  1. ORDERING. apply() applies a queued ControlRequest AND settles playback
//     (PlaybackPolicy) in a single call, so the canonical visible animation is
//     already correct the moment apply() returns. App is required to publish
//     the panel snapshot only AFTER apply(). Reading the visible animation
//     before the settle step is exactly the double-click bug: the 303
//     redirect's GET rendered the animation from before the click, so the first
//     click appeared to do nothing and a second click was needed to refresh it.
//
//  2. TESTABILITY. It has no Arduino, network, storage or matrix dependency, so
//     "exactly one authenticated selection request starts ANY user-selectable
//     animation, from MANUAL and from AUTO" is a deterministic host test.
//
// It performs no persistence and touches no hardware: the caller (App) performs
// the side effects the returned Outcome asks for.
class ControlPipeline {
public:
    struct Outcome {
        // settings_ changed and must be persisted.
        bool persist;
        // The request was structurally valid and was applied.
        bool accepted;
        // A SELECT_ANIMATION re-ran the animation that was already playing.
        bool restarted;
        // Apply brightnessPercent / rotation to the hardware.
        bool applyBrightness;
        bool applyRotation;
        uint8_t brightnessPercent;
        DisplayRotation rotation;
    };

    ControlPipeline(
        UserSettings& settings,
        ManualOverride& manual,
        PlaybackPolicy& policy,
        AnimationManager& manager)
        : settings_(settings),
          manual_(manual),
          policy_(policy),
          manager_(manager),
          accepted_(false) {
    }

    // Applies one deferred request, then settles playback. Never returns with
    // the visible animation still reflecting the pre-request state.
    Outcome apply(const ControlRequest& request, uint32_t now) {
        Outcome outcome{false, false, false, false, false, 0u, DisplayRotation::DEG_0};
        accepted_ = false;

        switch (request.action) {
            case ControlAction::NONE:
                break;

            case ControlAction::SET_MODE:
                outcome.persist = (settings_.mode != request.mode);
                settings_.mode = request.mode;
                if (settings_.mode == ControlMode::MANUAL) {
                    // Re-arm the override so the scheduler can never steal the
                    // panel back, even if it computed an AUTO decision earlier
                    // in this same cycle.
                    manual_.activateUntilCancelled(settings_.animation, PlayMode::LOOP);
                } else {
                    // Clearing the override makes the scheduler take over on
                    // the very next settled tick.
                    manual_.clear();
                }
                break;

            case ControlAction::SELECT_ANIMATION:
                // Validated here as well as in the HTTP handler: this is also
                // the path a hand-crafted request reaches, so the guard must
                // not depend on the UI having been well behaved.
                if (!AnimationCatalog::isUserSelectable(request.animation)) {
                    break;
                }
                accepted_ = true;
                outcome.accepted = true;

                // Clicking the animation already playing in MANUAL means "run
                // it again from the start" - the behaviour the removed Uruchom
                // button used to provide.
                if (settings_.animation == request.animation &&
                    settings_.mode == ControlMode::MANUAL) {
                    policy_.restart(now);
                    outcome.restarted = true;
                    break;
                }

                // AUTO -> MANUAL and start, in ONE step. The manual override is
                // what establishes MANUAL ownership before the scheduler can
                // overwrite the choice.
                outcome.persist = true;
                settings_.animation = request.animation;
                settings_.mode = ControlMode::MANUAL;
                manual_.activateUntilCancelled(settings_.animation, PlayMode::LOOP);
                break;

            case ControlAction::SET_BRIGHTNESS: {
                const uint8_t next = clampBrightnessPercent(
                    static_cast<int>(request.brightnessPercent));
                if (next != settings_.brightnessPercent) {
                    settings_.brightnessPercent = next;
                    outcome.persist = true;
                    outcome.applyBrightness = true;
                    outcome.brightnessPercent = next;
                }
                break;
            }

            case ControlAction::SET_ROTATION:
                if (isDisplayRotationValid(request.rotation) &&
                    request.rotation != settings_.rotation) {
                    settings_.rotation = request.rotation;
                    outcome.persist = true;
                    outcome.applyRotation = true;
                    outcome.rotation = request.rotation;
                }
                break;

            case ControlAction::PLAY:
                policy_.restart(now);
                break;
        }

        // Settle playback NOW. This is the whole point of the class: when
        // apply() returns, visibleAnimation() describes what is on the panel.
        policy_.tick(now);

        return outcome;
    }

    // The animation actually on the panel right now. Canonical: after apply()
    // has settled this is the manual selection in MANUAL mode and the
    // scheduler's choice in AUTO mode, and it agrees with the physical matrix.
    AnimationId visibleAnimation() const {
        return manager_.currentAnimation();
    }

    // True when the last request was a valid SELECT_ANIMATION and was applied.
    bool lastRequestAccepted() const {
        return accepted_;
    }

private:
    UserSettings& settings_;
    ManualOverride& manual_;
    PlaybackPolicy& policy_;
    AnimationManager& manager_;
    bool accepted_;
};
