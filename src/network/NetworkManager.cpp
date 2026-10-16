#include "network/NetworkManager.h"

#include <stdio.h>

// Diagnostics. The lifecycle logs are the primary tool for telling a
// persistence failure (no valid record after a power cycle) apart from a
// station-reconnect failure (a valid record that fails to associate).
//
// A password is NEVER passed to these macros.
#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#define PH_NET_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define PH_NET_LOG(...) do { } while (0)
#endif

// Scans are only ever triggered by the portal's explicit "Scan networks"
// action, so the state machine itself performs no scanning.

// Upper bound on the local setup transitions advanced inside begin(). Every one
// of these is instantaneous; none waits for a network result.
constexpr uint8_t kMaxBeginSteps = 8u;

NetworkManager::NetworkManager(
    NetworkDriver& driver,
    NetworkCredentialStore& store,
    ProvisioningPortal* portal)
    : driver_(driver),
      store_(store),
      portal_(portal),
      state_(NetworkState::BOOT),
      hasSavedCredentials_(false),
      hasPendingCredentials_(false),
      attemptStartedAtMs_(0u),
      lastRetryAtMs_(0u),
      connectedSinceMs_(0u),
      connectedAccumulatedMs_(0u),
      apTeardownPending_(false),
      apTeardownAtMs_(0u),
      provisioningServerActive_(false),
      userProvisioningActive_(false),
      provisioningStartedAtMs_(0u),
      rejectedSubmissions_(0u),
      lastNowMs_(0u) {
    clearNetworkCredentials(savedCredentials_);
    clearNetworkCredentials(pendingCredentials_);
}

void NetworkManager::transition(NetworkState next) {
    state_ = next;
}

bool NetworkManager::isConnected() const {
    return state_ == NetworkState::CONNECTED;
}

NetworkState NetworkManager::state() const {
    return state_;
}

uint32_t NetworkManager::localAddress() const {
    if (!isConnected()) {
        return 0u;
    }
    return driver_.localAddress();
}

uint8_t NetworkManager::rejectedSubmissions() const {
    return rejectedSubmissions_;
}

bool NetworkManager::provisioningActive() const {
    return provisioningServerActive_;
}

void NetworkManager::begin(uint32_t nowMs) {
    lastNowMs_ = nowMs;
    transition(NetworkState::LOAD_CREDENTIALS);

    // Drive the machine past the purely local setup transitions so the caller
    // gets a meaningful state as soon as begin() returns. This is a bounded
    // loop over transitions that perform no waiting: it never touches the
    // radio for a result, never sleeps, and is not the connection wait.
    for (uint8_t step = 0u; step < kMaxBeginSteps; ++step) {
        if (state_ != NetworkState::BOOT &&
            state_ != NetworkState::LOAD_CREDENTIALS &&
            state_ != NetworkState::START_STA &&
            state_ != NetworkState::START_PROVISIONING) {
            break;
        }
        tick(nowMs);
    }
}

bool NetworkManager::userProvisioningActive() const {
    return userProvisioningActive_;
}

bool NetworkManager::startProvisioning(uint32_t nowMs) {
    lastNowMs_ = nowMs;

    // Non-blocking by construction: no radio calls, no storage calls, no
    // waiting. A caller inside an HTTP handler can therefore trigger this and
    // return immediately; the AP and portal come up on a later tick.
    //
    // Nothing is erased. The committed record is left exactly as it is, so
    // normal reset and a power loss during provisioning both preserve it.
    userProvisioningActive_ = true;
    transition(NetworkState::PROVISIONING_REQUESTED);

    PH_NET_LOG("[NET] Wi-Fi setup requested; existing credentials kept\n");
    return true;
}

void NetworkManager::enterProvisioning(uint32_t nowMs) {
    lastNowMs_ = nowMs;
    apTeardownPending_ = false;

    // This method is re-entered every time an in-flight background retry of the
    // saved network times out, so it must be idempotent. Rebuilding the softAP
    // would drop a phone that is in the middle of configuring the device, and
    // re-starting the portal would fight itself for port 80. While the setup
    // network and its portal are already serving, only the state is refreshed.
    const bool alreadyServing =
        driver_.accessPointActive() &&
        (portal_ != nullptr) &&
        portal_->active();

    if (alreadyServing) {
        transition(NetworkState::PROVISIONING);
        return;
    }

    // The setup AP is only ever up while provisioning is actually needed.
    driver_.startAccessPoint(nullptr, nullptr);

    if (portal_ != nullptr) {
        portal_->start(&NetworkManager::handleSubmit, this);
        // start() binds the port-80 HTTP listener, so record that we own it.
        provisioningServerActive_ = true;
        portal_->setPendingConnection(false);
        if (hasPendingCredentials_) {
            portal_->setStatusMessage("Could not connect to this Wi-Fi network.");
            portal_->setPendingSsid(pendingCredentials_.ssid);
        } else {
            portal_->setStatusMessage("Enter your home Wi-Fi details to continue.");
        }
    }

    transition(NetworkState::PROVISIONING);
    lastRetryAtMs_ = nowMs;
    provisioningStartedAtMs_ = nowMs;
}

void NetworkManager::beginStationAttempt(
    const NetworkCredentials& credentials,
    uint32_t nowMs) {
    attemptStartedAtMs_ = nowMs;
    lastRetryAtMs_ = nowMs;
    driver_.beginStation(credentials);
}

void NetworkManager::onStationConnected(uint32_t nowMs) {
    connectedSinceMs_ = nowMs;
    connectedAccumulatedMs_ = 0u;

    PH_NET_LOG("[NET] Connected\n");

    if (hasPendingCredentials_) {
        onPendingSucceeded(nowMs);
        return;
    }

    if (portal_ != nullptr) {
        portal_->stop();
    }
    // The portal HTTP server has released port 80, so the LAN status service
    // may claim it.
    provisioningServerActive_ = false;
    apTeardownPending_ = false;

    transition(NetworkState::CONNECTED);
}

void NetworkManager::onStationTimeout() {
    // Credentials are deliberately NOT erased. A router that is temporarily
    // unavailable must not destroy a working configuration.
    transition(NetworkState::RECOVERING);
    enterProvisioning(lastNowMs_);
}

void NetworkManager::onUserProvisioningTimedOut() {
    // Nothing is erased. The previously committed record is still the one that
    // will be used, so the device returns to normal operation unchanged.
    PH_NET_LOG("[NET] Wi-Fi setup timed out; keeping existing credentials\n");

    if (portal_ != nullptr) {
        portal_->stop();
    }
    provisioningServerActive_ = false;
    userProvisioningActive_ = false;
    apTeardownPending_ = false;

    driver_.stopAccessPoint();
    transition(NetworkState::FAILED_KEEP_OLD);
}

void NetworkManager::onPendingSucceeded(uint32_t nowMs) {
    // Only a proven-good association is ever written to storage.
    if (store_.save(pendingCredentials_)) {
        savedCredentials_ = pendingCredentials_;
        hasSavedCredentials_ = true;
        hasPendingCredentials_ = false;
        PH_NET_LOG("[NET] Credentials persisted\n");
    } else {
        // Reported loudly: silently continuing here is how a device ends up
        // re-provisioning on every boot.
        PH_NET_LOG("[NET] WARNING: credentials were NOT persisted\n");
    }

    if (portal_ != nullptr) {
        portal_->setPendingConnection(false);
        portal_->setStatusMessage("Connected. You can close this page.");
    }

    // Let the phone read the success page, then drop the AP without blocking.
    apTeardownPending_ = true;
    apTeardownAtMs_ = nowMs + NetworkDefaults::kApTeardownGraceMs;

    // The user's request has been fulfilled (or this was a first-boot join).
    userProvisioningActive_ = false;

    transition(NetworkState::CONNECTED);
    connectedSinceMs_ = nowMs;
    connectedAccumulatedMs_ = 0u;
}

void NetworkManager::onPendingFailed() {
    // Discard the attempt. Known-good stored credentials stay untouched.
    clearNetworkCredentials(pendingCredentials_);
    hasPendingCredentials_ = false;

    if (portal_ != nullptr) {
        portal_->setPendingConnection(false);
        portal_->setStatusMessage("Could not connect to this Wi-Fi network.");
    }

    // Grant a fresh window so a typo does not immediately consume the
    // user's remaining time.
    provisioningStartedAtMs_ = lastNowMs_;

    transition(NetworkState::PROVISIONING);
}

void NetworkManager::submitCredentials(
    const NetworkCredentials& credentials,
    uint32_t nowMs) {
    if (!areNetworkCredentialsValid(credentials)) {
        ++rejectedSubmissions_;
        if (portal_ != nullptr) {
            portal_->setStatusMessage("Invalid network details.");
        }
        return;
    }

    // Keep the AP alive so the user can retry without a reboot.
    pendingCredentials_ = credentials;
    hasPendingCredentials_ = true;

    if (portal_ != nullptr) {
        portal_->setPendingConnection(true);
        portal_->setStatusMessage("Trying to connect...");
    }

    beginStationAttempt(pendingCredentials_, nowMs);
    transition(NetworkState::TEST_PENDING_CREDENTIALS);
}

void NetworkManager::handleSubmit(const NetworkCredentials& credentials, void* context) {
    if (context == nullptr) {
        return;
    }

    NetworkManager* self = static_cast<NetworkManager*>(context);
    self->submitCredentials(credentials, self->lastNowMs_);
}

void NetworkManager::tick(uint32_t nowMs) {
    lastNowMs_ = nowMs;

    switch (state_) {
        case NetworkState::BOOT:
            transition(NetworkState::LOAD_CREDENTIALS);
            break;

        case NetworkState::LOAD_CREDENTIALS: {
            NetworkCredentials loaded;
            clearNetworkCredentials(loaded);
            const bool ok = store_.load(loaded);
            if (ok && areNetworkCredentialsValid(loaded)) {
                savedCredentials_ = loaded;
                hasSavedCredentials_ = true;
                PH_NET_LOG("[NET] Saved credentials found\n");
                PH_NET_LOG("[NET] Connecting to saved SSID: %s\n", savedCredentials_.ssid);
                transition(NetworkState::START_STA);
            } else {
                hasSavedCredentials_ = false;
                PH_NET_LOG("[NET] No valid saved credentials\n");
                PH_NET_LOG("[NET] Starting provisioning\n");
                enterProvisioning(nowMs);
            }
            break;
        }

        case NetworkState::START_STA:
            if (hasSavedCredentials_) {
                PH_NET_LOG("[NET] Attempting saved network\n");
                beginStationAttempt(savedCredentials_, nowMs);
                transition(NetworkState::WAIT_STA);
            } else {
                enterProvisioning(nowMs);
            }
            break;

        case NetworkState::WAIT_STA:
            if (driver_.stationConnected()) {
                onStationConnected(nowMs);
            } else if (nowMs - attemptStartedAtMs_ >= NetworkDefaults::kStaConnectTimeoutMs) {
                onStationTimeout();
            } else if (portal_ != nullptr && portal_->active()) {
                // A setup portal can be up while this station attempt is in
                // flight: automatic fallback keeps retrying the saved network
                // in the background. Keep answering the portal, otherwise the
                // setup page goes silent for the whole attempt window (~20s)
                // exactly while the user is configuring the new network.
                // Port 80 is already owned here, so this only pumps the
                // existing server; it never starts a second one.
                portal_->tick();
            }
            break;

        case NetworkState::CONNECTED:
            if (!driver_.stationConnected()) {
                // Link dropped. The clock already acquired is intentionally
                // kept by the time provider; only the radio is retried.
                if (hasSavedCredentials_) {
                    // Known-good credentials exist, so retry silently rather
                    // than reopening a portal the user did not ask for.
                    transition(NetworkState::RECOVERING);
                } else {
                    enterProvisioning(nowMs);
                }
            } else {
                connectedAccumulatedMs_ = nowMs - connectedSinceMs_;

                if (apTeardownPending_ && nowMs >= apTeardownAtMs_) {
                    apTeardownPending_ = false;

                    // Order matters. The provisioning HTTP server must release
                    // port 80 BEFORE the AP goes away, and before any other
                    // component is allowed to start its own port-80 server.
                    // Tearing down only the AP would leave the listener alive
                    // with no owner and requests unanswered.
                    if (portal_ != nullptr) {
                        portal_->stop();
                    }
                    provisioningServerActive_ = false;

                    driver_.stopAccessPoint();
                }
            }
            break;

        case NetworkState::START_PROVISIONING:
            enterProvisioning(nowMs);
            break;

        case NetworkState::PROVISIONING_REQUESTED:
            // Deferred bring-up. By the time control reaches here, any HTTP
            // response from the handler that requested this has been written
            // and its connection closed, so tearing the LAN server down is safe.
            enterProvisioning(nowMs);
            break;

        case NetworkState::PROVISIONING:
            if (userProvisioningActive_) {
                // The station may still be associated with the OLD network, so
                // "connected" must NOT be read as "the user's request is
                // already satisfied". Only a successful new credential test or
                // the give-up timeout ends a user-requested session.
                if (nowMs - provisioningStartedAtMs_ >=
                    NetworkDefaults::kUserProvisioningTimeoutMs) {
                    onUserProvisioningTimedOut();
                    break;
                }

                if (portal_ != nullptr) {
                    portal_->tick();
                }
                // No background retry of the old record here: it would move us
                // to WAIT_STA and pull the portal out from under the user.
                break;
            }

            if (driver_.stationConnected()) {
                onStationConnected(nowMs);
                break;
            }
            if (portal_ != nullptr) {
                portal_->tick();
            }
            // Background retry of known-good credentials while provisioning,
            // so restoring a router does not require touching the phone again.
            if (hasSavedCredentials_ &&
                nowMs - lastRetryAtMs_ >= NetworkDefaults::kStaRetryIntervalMs) {
                beginStationAttempt(savedCredentials_, nowMs);
                transition(NetworkState::WAIT_STA);
            }
            break;

        case NetworkState::FAILED_KEEP_OLD:
            // The previous credentials were deliberately preserved. Resume
            // normal operation from them.
            if (driver_.stationConnected()) {
                connectedSinceMs_ = nowMs;
                connectedAccumulatedMs_ = 0u;
                transition(NetworkState::CONNECTED);
            } else if (hasSavedCredentials_) {
                beginStationAttempt(savedCredentials_, nowMs);
                transition(NetworkState::WAIT_STA);
            } else {
                // Nothing to fall back on, so the automatic portal is the only
                // way forward.
                enterProvisioning(nowMs);
            }
            break;

        case NetworkState::TEST_PENDING_CREDENTIALS:
            if (driver_.stationConnected()) {
                onPendingSucceeded(nowMs);
            } else if (nowMs - attemptStartedAtMs_ >=
                       NetworkDefaults::kPendingCredentialTimeoutMs) {
                onPendingFailed();
            } else if (portal_ != nullptr && portal_->active()) {
                // The portal stays up for the whole test (the connect handler
                // has an explicit 409 guard for this window) and its page
                // auto-reloads while waiting, so it must keep being serviced.
                portal_->tick();
            }
            break;

        case NetworkState::RECOVERING:
            if (hasSavedCredentials_ &&
                nowMs - lastRetryAtMs_ >= NetworkDefaults::kStaRetryIntervalMs) {
                beginStationAttempt(savedCredentials_, nowMs);
                transition(NetworkState::WAIT_STA);
            } else if (portal_ != nullptr) {
                portal_->tick();
            }
            break;
    }
}
