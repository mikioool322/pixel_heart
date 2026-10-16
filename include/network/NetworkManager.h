#pragma once

#include <stdint.h>

#include "network/NetworkCredentialStore.h"
#include "network/NetworkCredentials.h"
#include "network/NetworkDriver.h"
#include "network/NetworkDefaults.h"
#include "network/ProvisioningPortal.h"

enum class NetworkState : uint8_t {
    BOOT,
    LOAD_CREDENTIALS,
    START_STA,
    WAIT_STA,
    CONNECTED,
    // A provisioning session has been requested but no radio work has started
    // yet. This deferral is what lets an HTTP handler answer a "Change Wi-Fi"
    // request and return before the LAN server is torn down.
    PROVISIONING_REQUESTED,
    START_PROVISIONING,
    PROVISIONING,
    TEST_PENDING_CREDENTIALS,
    // User-initiated provisioning gave up. The previously committed
    // credentials are still intact and normal operation resumes from them.
    FAILED_KEEP_OLD,
    RECOVERING
};

// Non-blocking Wi-Fi lifecycle owner.
//
// Responsibilities:
//   - load persisted credentials at boot
//   - attempt a bounded, non-blocking station connection
//   - fall back to the captive portal when no usable credentials exist
//   - preserve known-good credentials across failures
//   - persist credentials only after a successful association
//
// It never blocks, never sleeps, and never logs a passphrase.
class NetworkManager {
public:
    NetworkManager(
        NetworkDriver& driver,
        NetworkCredentialStore& store,
        ProvisioningPortal* portal);

    void begin(uint32_t nowMs);
    void tick(uint32_t nowMs);

    bool isConnected() const;
    NetworkState state() const;
    uint32_t localAddress() const;

    // True while the captive portal's HTTP server may still own port 80.
    //
    // This stays true through the post-success grace period, so a caller that
    // wants to start its own HTTP server must wait for this to go false.
    // Waiting on isConnected() alone is NOT sufficient: during the grace
    // period the device is already connected while the provisioning server is
    // still listening, and two WebServer(80) instances would conflict.
    bool provisioningActive() const;

    // Re-opens the setup portal so the user can pick a different network.
    //
    // Deliberately does NO radio work and NO storage work: it only records the
    // intent and moves to PROVISIONING_REQUESTED. The AP and portal are brought
    // up on a later tick, which means a caller inside an HTTP handler can
    // trigger this and still have its response delivered first.
    //
    // Never erases credentials. The previous record stays committed until a
    // new network has been proven to work.
    bool startProvisioning(uint32_t nowMs);

    // True while a provisioning session the user explicitly asked for is open.
    bool userProvisioningActive() const;

    // True when a submission was syntactically rejected by validation.
    uint8_t rejectedSubmissions() const;

    // Portal entry point: called by ProvisioningPortal with a validated
    // submission. Does not save; only schedules a test connection.
    // Portal callback. `context` is the NetworkManager* that the portal was
    // started with; the portal never owns it.
    static void handleSubmit(const NetworkCredentials& credentials, void* context);

private:
    void transition(NetworkState next);
    void enterProvisioning(uint32_t nowMs);
    void beginStationAttempt(const NetworkCredentials& credentials, uint32_t nowMs);
    void onStationConnected(uint32_t nowMs);
    void onStationTimeout();
    void onPendingSucceeded(uint32_t nowMs);
    void onPendingFailed();
    void onUserProvisioningTimedOut();
    void submitCredentials(const NetworkCredentials& credentials, uint32_t nowMs);

    NetworkDriver& driver_;
    NetworkCredentialStore& store_;
    ProvisioningPortal* portal_;

    NetworkState state_;
    NetworkCredentials savedCredentials_;
    NetworkCredentials pendingCredentials_;
    bool hasSavedCredentials_;
    bool hasPendingCredentials_;

    uint32_t attemptStartedAtMs_;
    uint32_t lastRetryAtMs_;
    uint32_t connectedSinceMs_;
    uint32_t connectedAccumulatedMs_;

    bool apTeardownPending_;
    uint32_t apTeardownAtMs_;

    // True from portal start until the portal has fully released port 80.
    // Prevents a second WebServer(80) from starting alongside the portal.
    bool provisioningServerActive_;

    // True for a provisioning session the user requested from the LAN page
    // (as opposed to the automatic first-boot portal). While set, a still-live
    // station link to the OLD network must not be treated as "already done".
    bool userProvisioningActive_;

    // When the current user-initiated provisioning session started, used for
    // the give-up timeout.
    uint32_t provisioningStartedAtMs_;

    uint8_t rejectedSubmissions_;

    uint32_t lastNowMs_;
};
