#include <unity.h>

#include <string.h>

#include "core/AnimationManager.h"
#include "network/NetworkCredentialStore.h"
#include "network/NetworkDefaults.h"
#include "network/NetworkManager.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Scheduler.h"

namespace {

// In-memory double. Models the parts of the radio the state machine relies on,
// including "the router is simply not there", which the real driver cannot
// report synchronously.
class FakeDriver final : public NetworkDriver {
public:
    FakeDriver() {
        lastAttemptedSsid_[0] = '\0';
        apSsid[0] = '\0';
    }

    void beginStation(const NetworkCredentials& credentials) override {
        ++beginStationCalls;
        strncpy(lastAttemptedSsid_, credentials.ssid, sizeof(lastAttemptedSsid_) - 1u);
        lastAttemptedSsid_[sizeof(lastAttemptedSsid_) - 1u] = '\0';
    }

    void startAccessPoint(const char* ssid, const char* password) override {
        ++accessPointStarts;
        apActive = true;
        if (ssid == nullptr) {
            apSsid[0] = '\0';
        } else {
            strncpy(apSsid, ssid, sizeof(apSsid) - 1u);
            apSsid[sizeof(apSsid) - 1u] = '\0';
        }
        (void)password;
    }

    void stopAccessPoint() override {
        ++accessPointStops;
        apActive = false;
    }

    bool accessPointActive() const override { return apActive; }

    bool stationConnected() const override { return connected; }

    void disconnectStation() override {
        connected = false;
        ++disconnectCalls;
    }

    uint32_t localAddress() const override { return connected ? address : 0u; }

    void currentSsid(char* out, uint8_t capacity) const override {
        if (out == nullptr || capacity == 0u) {
            return;
        }
        strncpy(out, connected ? lastAttemptedSsid_ : "", static_cast<size_t>(capacity) - 1u);
        out[static_cast<size_t>(capacity) - 1u] = '\0';
    }

    uint8_t scanNetworks(char ssids[][33], uint8_t maxResults) override {
        ++scanCalls;
        for (uint8_t i = 0u; i < maxResults; ++i) {
            ssids[i][0] = '\0';
        }
        return 0u;
    }

    // Test controls.
    bool connected = false;
    bool apActive = false;
    uint32_t address = 0xC0A80164u;  // 192.168.1.100
    char apSsid[40];

    uint16_t beginStationCalls = 0;
    uint16_t accessPointStarts = 0;
    uint16_t accessPointStops = 0;
    uint16_t disconnectCalls = 0;
    uint16_t scanCalls = 0;
    char lastAttemptedSsid_[33];
};

class FakeStore final : public NetworkCredentialStore {
public:
    bool load(NetworkCredentials& out) override {
        ++loadCalls;
        if (!hasValue) {
            clearNetworkCredentials(out);
            return false;
        }
        out = value;
        return true;
    }

    bool save(const NetworkCredentials& credentials) override {
        ++saveCalls;
        if (failSaves) {
            return false;
        }
        value = credentials;
        hasValue = true;
        return true;
    }

    bool clear() override {
        ++clearCalls;
        hasValue = false;
        clearNetworkCredentials(value);
        return true;
    }

    bool hasValue = false;
    bool failSaves = false;
    NetworkCredentials value;
    uint16_t loadCalls = 0;
    uint16_t saveCalls = 0;
    uint16_t clearCalls = 0;
};

class FakePortal final : public ProvisioningPortal {
public:
    void start(SubmitHandler handler, void* context) override {
        handler_ = handler;
        context_ = context;
        active_ = true;
        ++starts;
    }

    void stop() override {
        active_ = false;
        ++stops;
    }

    bool active() const override { return active_; }

    void tick() override { ++ticks; }

    void setPendingConnection(bool pending) override { pending_ = pending; }

    void setStatusMessage(const char* message) override {
        if (message != nullptr) {
            strncpy(status_, message, sizeof(status_) - 1u);
            status_[sizeof(status_) - 1u] = '\0';
        }
    }

    void setPendingSsid(const char* ssid) override {
        if (ssid != nullptr) {
            strncpy(pendingSsid_, ssid, sizeof(pendingSsid_) - 1u);
            pendingSsid_[sizeof(pendingSsid_) - 1u] = '\0';
        }
    }

    // Test-only: emulate the user submitting the form.
    void submit(const char* ssid, const char* password) {
        NetworkCredentials credentials;
        clearNetworkCredentials(credentials);
        if (ssid != nullptr && assignNetworkSsid(credentials, ssid)) {
            if (password == nullptr || assignNetworkPassword(credentials, password)) {
                if (handler_ != nullptr) {
                    handler_(credentials, context_);
                }
            }
        }
    }

    FakePortal() {
        status_[0] = '\0';
        pendingSsid_[0] = '\0';
    }

    bool active_ = false;
    bool pending_ = false;
    uint16_t starts = 0;
    uint16_t stops = 0;
    uint16_t ticks = 0;
    char status_[96];
    char pendingSsid_[33];
    SubmitHandler handler_ = nullptr;
    void* context_ = nullptr;
};

NetworkCredentials makeCredentials(const char* ssid, const char* password) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    assignNetworkSsid(credentials, ssid);
    assignNetworkPassword(credentials, password);
    return credentials;
}

struct Harness {
    FakeDriver driver;
    FakeStore store;
    FakePortal portal;
    NetworkManager manager;

    Harness()
        : manager(driver, store, &portal) {}

    // Advances the manager in bounded steps, never sleeping.
    void run(uint32_t fromMs, uint32_t toMs, uint32_t stepMs = 100u) {
        uint32_t t = fromMs;
        while (t <= toMs) {
            manager.tick(t);
            t += stepMs;
        }
    }
};

}  // namespace

void test_no_credentials_starts_provisioning(void) {
    Harness h;
    h.manager.begin(0u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_FALSE(h.manager.isConnected());

    // The AP must be started once, not re-armed on every tick.
    h.run(100u, 2000u);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
}

void test_saved_credentials_connect_automatically(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");

    h.manager.begin(0u);

    // A station attempt is under way and the portal is NOT open.
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.beginStationCalls);
    TEST_ASSERT_EQUAL_STRING("HomeNet", h.driver.lastAttemptedSsid_);
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.accessPointStarts);

    // Router comes up a moment later.
    h.driver.connected = true;
    h.run(500u, 1500u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    TEST_ASSERT_EQUAL_UINT32(0xC0A80164u, h.manager.localAddress());
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);  // no needless rewrite
}

void test_sta_timeout_preserves_credentials_and_opens_portal(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;  // router unavailable

    h.manager.begin(0u);
    TEST_ASSERT_FALSE(h.manager.isConnected());

    // Cross the 20s bound without the router ever appearing.
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);

    // Credentials are NOT erased, and a portal becomes available.
    TEST_ASSERT_TRUE(h.store.hasValue);
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.clearCalls);
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_FALSE(h.manager.isConnected());
}

void test_background_retry_of_saved_credentials(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);

    const uint16_t attemptsAfterTimeout = h.driver.beginStationCalls;

    // No retry spam: a single 60s window must not trigger many attempts.
    h.run(NetworkDefaults::kStaConnectTimeoutMs, NetworkDefaults::kStaConnectTimeoutMs + 5000u);
    TEST_ASSERT_EQUAL_UINT8(
        attemptsAfterTimeout,
        h.driver.beginStationCalls);

    // After the retry interval one more attempt is made.
    h.run(
        NetworkDefaults::kStaConnectTimeoutMs,
        NetworkDefaults::kStaConnectTimeoutMs + NetworkDefaults::kStaRetryIntervalMs + 1000u);
    TEST_ASSERT_TRUE(h.driver.beginStationCalls > attemptsAfterTimeout);
    TEST_ASSERT_TRUE(h.store.hasValue);
}

void test_pending_success_persists_credentials(void) {
    Harness h;
    h.manager.begin(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(h.manager.state()));

    h.portal.submit("HomeNet", "hunter2");
    h.driver.connected = true;
    h.run(10u, 500u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    // Persisted only after the association actually succeeded.
    TEST_ASSERT_EQUAL_UINT8(1u, h.store.saveCalls);
    TEST_ASSERT_TRUE(h.store.hasValue);
    TEST_ASSERT_EQUAL_STRING("HomeNet", h.store.value.ssid);
    TEST_ASSERT_EQUAL_STRING("hunter2", h.store.value.password);
}

void test_pending_failure_does_not_persist(void) {
    Harness h;
    h.manager.begin(0u);

    h.portal.submit("BadNet", "wrongpass");
    h.driver.connected = false;
    h.run(0u, NetworkDefaults::kPendingCredentialTimeoutMs + 500u);

    // Nothing was written to storage.
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
    TEST_ASSERT_FALSE(h.store.hasValue);

    // The portal remains available for another attempt.
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_FALSE(h.manager.isConnected());
}

void test_failed_new_credentials_preserve_old_ones(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("GoodNet", "goodpass");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);
    TEST_ASSERT_TRUE(h.portal.active_);

    // User opens the portal and types a wrong password for a different network.
    h.portal.submit("NewNet", "totallywrong");
    h.run(
        NetworkDefaults::kStaConnectTimeoutMs,
        NetworkDefaults::kStaConnectTimeoutMs + NetworkDefaults::kPendingCredentialTimeoutMs + 500u);

    // The known-good record must be exactly as it was.
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.clearCalls);
    TEST_ASSERT_TRUE(h.store.hasValue);
    TEST_ASSERT_EQUAL_STRING("GoodNet", h.store.value.ssid);
    TEST_ASSERT_EQUAL_STRING("goodpass", h.store.value.password);
    TEST_ASSERT_TRUE(h.portal.active_);
}

void test_invalid_submission_is_rejected_without_connecting(void) {
    Harness h;
    h.manager.begin(0u);

    // An empty SSID is refused by the shared validator in the portal layer,
    // so it never reaches the driver at all.
    h.portal.submit("", "somepass");
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.beginStationCalls);

    // An over-long SSID is likewise refused before submission.
    char tooLong[64];
    memset(tooLong, 'X', sizeof(tooLong));
    tooLong[sizeof(tooLong) - 1u] = '\0';
    h.portal.submit(tooLong, "pw");
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.beginStationCalls);

    // Nothing invalid was ever written to storage.
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
    TEST_ASSERT_FALSE(h.store.hasValue);

    // A record that is invalid but reaches the manager (e.g. from a future
    // UI) is still refused there, and counted.
    NetworkCredentials bad;
    clearNetworkCredentials(bad);
    bad.valid = false;
    NetworkManager::handleSubmit(bad, &h.manager);
    TEST_ASSERT_EQUAL_UINT8(1u, h.manager.rejectedSubmissions());
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.beginStationCalls);
}

void test_ap_is_torn_down_after_grace_period(void) {
    Harness h;
    h.manager.begin(0u);
    TEST_ASSERT_TRUE(h.driver.apActive);

    h.portal.submit("HomeNet", "hunter2");
    h.driver.connected = true;
    h.run(0u, 500u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    // Still up during the grace window so the phone can read the result.
    TEST_ASSERT_TRUE(h.driver.apActive);

    h.run(500u, NetworkDefaults::kApTeardownGraceMs + 2000u);
    TEST_ASSERT_FALSE(h.driver.apActive);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStops);
}

void test_link_loss_after_success_retries_silently(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");

    h.manager.begin(0u);
    h.driver.connected = true;
    h.run(0u, 500u);
    TEST_ASSERT_TRUE(h.manager.isConnected());
    const uint16_t portalStarts = h.driver.accessPointStarts;

    // Router vanishes.
    h.driver.connected = false;
    h.run(1000u, 2000u);

    // Recovery without reopening a portal the user did not request.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::RECOVERING),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT8(portalStarts, h.driver.accessPointStarts);

    // Credentials still intact for the retry.
    TEST_ASSERT_TRUE(h.store.hasValue);
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.clearCalls);

    // Router returns; the retry interval brings it back.
    h.driver.connected = true;
    h.run(2000u, NetworkDefaults::kStaRetryIntervalMs + 5000u);
    TEST_ASSERT_TRUE(h.manager.isConnected());
}

void test_save_failure_does_not_lose_pending_state(void) {
    Harness h;
    h.store.failSaves = true;
    h.manager.begin(0u);

    h.portal.submit("HomeNet", "hunter2");
    h.driver.connected = true;
    h.run(0u, 500u);

    // The connection succeeded even though the write did not; the device
    // still runs, and no bogus record is written.
    TEST_ASSERT_TRUE(h.manager.isConnected());
    TEST_ASSERT_EQUAL_UINT8(1u, h.store.saveCalls);
    TEST_ASSERT_FALSE(h.store.hasValue);
}

void test_start_provisioning_reopens_portal(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.manager.begin(0u);
    h.driver.connected = true;
    h.run(0u, 500u);
    TEST_ASSERT_TRUE(h.manager.isConnected());

    TEST_ASSERT_TRUE(h.manager.startProvisioning(1000u));

    // startProvisioning() is deliberately deferred: it records the request and
    // does no radio or server work, so an HTTP handler can call it and still
    // have its response delivered before the LAN server is torn down. The AP
    // and portal come up on the next tick.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING_REQUESTED),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_FALSE(h.driver.apActive);

    h.manager.tick(1500u);
    TEST_ASSERT_TRUE(h.driver.apActive);
    TEST_ASSERT_TRUE(h.portal.active_);
}

void test_state_machine_never_blocks_or_uses_delay(void) {
    // A full lifecycle must complete purely through tick() calls, which is
    // the structural guarantee that no blocking wait exists.
    Harness h;
    uint32_t t = 0u;
    const uint32_t before = h.driver.beginStationCalls;

    h.manager.begin(t);
    for (uint16_t i = 0u; i < 4000u; ++i) {
        h.manager.tick(t);
        t += 50u;
    }

    TEST_ASSERT_TRUE(h.driver.beginStationCalls >= before);
    TEST_ASSERT_FALSE(h.manager.isConnected());
    TEST_ASSERT_TRUE(h.driver.apActive);
}

void test_network_activity_does_not_disturb_the_scheduler(void) {
    // The scheduler is fed only by TimeProvider. Driving the network state
    // machine alongside a real scheduler must leave scheduling untouched.
    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy(scheduler, manager, matrix);

    // An invalid clock (pre-NTP) yields the V1 fallback animation.
    ClockSnapshot invalid;
    invalid.year = 0u;
    invalid.month = 1u;
    invalid.day = 1u;
    invalid.hour = 5u;
    invalid.minute = 0u;
    invalid.second = 0u;
    invalid.weekday = 4u;
    invalid.valid = false;
    clock.set(invalid);

    Harness h;
    h.manager.begin(0u);

    for (uint16_t i = 0u; i < 200u; ++i) {
        const uint32_t t = static_cast<uint32_t>(i) * 50u;
        h.manager.tick(t);
        policy.tick(t);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::AURORA),
        static_cast<uint8_t>(policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(SchedulePriority::FALLBACK),
        static_cast<uint8_t>(policy.appliedPriority()));
    TEST_ASSERT_EQUAL_UINT16(1u, policy.startCount());
}

// ---------------------------------------------------------------------------
// New-home fallback edge cases (WIFI-01 .. WIFI-15 matrix).
// ---------------------------------------------------------------------------

// WIFI-14: a stored record that cannot be trusted must not waste a station
// attempt; the device goes straight to the setup AP instead of looping offline.
void test_corrupt_stored_record_falls_back_to_provisioning(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.store.value.valid = false;  // e.g. an interrupted NVS write

    h.manager.begin(0u);

    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.beginStationCalls);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_TRUE(h.portal.active_);
}

// WIFI-12: a connection that arrives inside the bound must win, and fallback
// must never fire afterwards.
void test_connection_just_before_timeout_wins_without_portal(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(100u, NetworkDefaults::kStaConnectTimeoutMs - 100u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::WAIT_STA),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.accessPointStarts);

    // Router answers one step short of the deadline.
    h.driver.connected = true;
    h.manager.tick(NetworkDefaults::kStaConnectTimeoutMs - 50u);
    TEST_ASSERT_TRUE(h.manager.isConnected());

    // The fallback must not fire after the fact.
    h.run(NetworkDefaults::kStaConnectTimeoutMs, NetworkDefaults::kStaConnectTimeoutMs * 3u);
    TEST_ASSERT_EQUAL_UINT8(0u, h.driver.accessPointStarts);
    TEST_ASSERT_FALSE(h.portal.active_);
}

// WIFI-13: failing exactly on the boundary is deterministic and opens exactly
// one setup network / one port-80 owner.
void test_timeout_boundary_transition_is_deterministic(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::WAIT_STA),
        static_cast<uint8_t>(h.manager.state()));

    // Exactly at the bound the portal opens, once.
    h.manager.tick(NetworkDefaults::kStaConnectTimeoutMs);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_EQUAL_UINT8(1u, h.portal.starts);

    // Ticking across the boundary must not open a second server or AP.
    h.manager.tick(NetworkDefaults::kStaConnectTimeoutMs + 1u);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_EQUAL_UINT8(1u, h.portal.starts);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
}

// The automatic fallback repeatedly retries the unreachable saved network. The
// setup network must be created once and then left alone: rebuilding the softAP
// would knock a phone off the very page it is configuring the device with.
void test_automatic_fallback_does_not_recreate_the_setup_ap(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_EQUAL_UINT8(1u, h.portal.starts);

    // Four full background-retry cycles with the router still gone.
    h.run(
        NetworkDefaults::kStaConnectTimeoutMs,
        NetworkDefaults::kStaConnectTimeoutMs +
            4u * (NetworkDefaults::kStaRetryIntervalMs +
                  NetworkDefaults::kStaConnectTimeoutMs));

    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStarts);
    TEST_ASSERT_EQUAL_UINT8(1u, h.portal.starts);
    TEST_ASSERT_TRUE(h.driver.apActive);
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
}

// The setup page must stay serviced even while a background station retry is in
// flight; that window is exactly when the user is entering new credentials.
void test_setup_portal_keeps_answering_during_background_retry(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(100u, NetworkDefaults::kStaConnectTimeoutMs);
    TEST_ASSERT_TRUE(h.portal.active_);

    const uint32_t retryAt =
        NetworkDefaults::kStaConnectTimeoutMs + NetworkDefaults::kStaRetryIntervalMs;

    h.run(NetworkDefaults::kStaConnectTimeoutMs + 100u, retryAt);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::WAIT_STA),
        static_cast<uint8_t>(h.manager.state()));

    const uint16_t ticksBefore = h.portal.ticks;
    h.manager.tick(retryAt + 100u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::WAIT_STA),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT16(ticksBefore + 1u, h.portal.ticks);
}

// The setup page auto-reloads while a candidate is being tested, so the portal
// must keep being serviced for the whole TEST_PENDING_CREDENTIALS window.
void test_portal_is_serviced_while_a_candidate_is_tested(void) {
    Harness h;
    h.manager.begin(0u);  // no saved credentials -> provisioning

    h.portal.submit("NewNet", "pw");
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::TEST_PENDING_CREDENTIALS),
        static_cast<uint8_t>(h.manager.state()));

    const uint16_t ticksBefore = h.portal.ticks;
    h.manager.tick(100u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::TEST_PENDING_CREDENTIALS),
        static_cast<uint8_t>(h.manager.state()));
    TEST_ASSERT_EQUAL_UINT16(ticksBefore + 1u, h.portal.ticks);
}

// WIFI-07 + WIFI-15: a wrong candidate keeps the old record and the portal, and
// a following correct candidate commits the new network.
void test_wrong_candidate_then_correct_candidate_commits(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("OldNet", "oldpass");
    h.driver.connected = false;

    h.manager.begin(0u);
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);
    TEST_ASSERT_TRUE(h.portal.active_);

    // Attempt 1: right SSID, wrong password.
    h.portal.submit("NewNet", "typo");
    h.run(
        NetworkDefaults::kStaConnectTimeoutMs,
        NetworkDefaults::kStaConnectTimeoutMs +
            NetworkDefaults::kPendingCredentialTimeoutMs + 500u);

    TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
    TEST_ASSERT_EQUAL_UINT8(0u, h.store.clearCalls);
    TEST_ASSERT_EQUAL_STRING("OldNet", h.store.value.ssid);
    TEST_ASSERT_TRUE(h.portal.active_);

    // Attempt 2: the same network, correct password.
    h.portal.submit("NewNet", "correct");
    h.driver.connected = true;
    h.run(
        NetworkDefaults::kStaConnectTimeoutMs +
            NetworkDefaults::kPendingCredentialTimeoutMs,
        NetworkDefaults::kStaConnectTimeoutMs +
            NetworkDefaults::kPendingCredentialTimeoutMs + 1000u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    TEST_ASSERT_EQUAL_UINT8(1u, h.store.saveCalls);
    TEST_ASSERT_EQUAL_STRING("NewNet", h.store.value.ssid);
    TEST_ASSERT_EQUAL_STRING("correct", h.store.value.password);
}

// WIFI-15: repeated failures must never destroy the committed record nor close
// the only way back in.
void test_repeated_failed_candidates_never_destroy_credentials(void) {
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("OldNet", "oldpass");
    h.driver.connected = false;

    h.manager.begin(0u);

    uint32_t now = 0u;
    while (now < NetworkDefaults::kStaConnectTimeoutMs + 500u) {
        now += 100u;
        h.manager.tick(now);
    }
    TEST_ASSERT_TRUE(h.portal.active_);

    for (uint8_t attempt = 0u; attempt < 3u; ++attempt) {
        h.portal.submit("NewNet", "stillwrong");

        const uint32_t deadline = now + NetworkDefaults::kPendingCredentialTimeoutMs + 500u;
        while (now < deadline) {
            now += 100u;
            h.manager.tick(now);
        }

        TEST_ASSERT_EQUAL_UINT8(0u, h.store.saveCalls);
        TEST_ASSERT_EQUAL_UINT8(0u, h.store.clearCalls);
        TEST_ASSERT_TRUE(h.store.hasValue);
        TEST_ASSERT_EQUAL_STRING("OldNet", h.store.value.ssid);
        TEST_ASSERT_TRUE(h.portal.active_);
        TEST_ASSERT_FALSE(h.manager.isConnected());
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_no_credentials_starts_provisioning);
    RUN_TEST(test_saved_credentials_connect_automatically);
    RUN_TEST(test_sta_timeout_preserves_credentials_and_opens_portal);
    RUN_TEST(test_background_retry_of_saved_credentials);
    RUN_TEST(test_pending_success_persists_credentials);
    RUN_TEST(test_pending_failure_does_not_persist);
    RUN_TEST(test_failed_new_credentials_preserve_old_ones);
    RUN_TEST(test_invalid_submission_is_rejected_without_connecting);
    RUN_TEST(test_ap_is_torn_down_after_grace_period);
    RUN_TEST(test_link_loss_after_success_retries_silently);
    RUN_TEST(test_save_failure_does_not_lose_pending_state);
    RUN_TEST(test_start_provisioning_reopens_portal);
    RUN_TEST(test_state_machine_never_blocks_or_uses_delay);
    RUN_TEST(test_network_activity_does_not_disturb_the_scheduler);
    RUN_TEST(test_corrupt_stored_record_falls_back_to_provisioning);
    RUN_TEST(test_connection_just_before_timeout_wins_without_portal);
    RUN_TEST(test_timeout_boundary_transition_is_deterministic);
    RUN_TEST(test_automatic_fallback_does_not_recreate_the_setup_ap);
    RUN_TEST(test_setup_portal_keeps_answering_during_background_retry);
    RUN_TEST(test_portal_is_serviced_while_a_candidate_is_tested);
    RUN_TEST(test_wrong_candidate_then_correct_candidate_commits);
    RUN_TEST(test_repeated_failed_candidates_never_destroy_credentials);
    return UNITY_END();
}
