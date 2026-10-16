#include <unity.h>

#include <string.h>

#include "core/AnimationManager.h"
#include "network/HttpPortOwnership.h"
#include "network/NetworkCredentialStore.h"
#include "network/NetworkDefaults.h"
#include "network/NetworkManager.h"
#include "scheduling/ManualOverride.h"
#include "scheduling/ManualTimeProvider.h"
#include "scheduling/PlaybackPolicy.h"
#include "scheduling/Scheduler.h"

namespace {

// Models the ESP32 WebServer lifecycle closely enough to catch the class of
// bug this test exists for: a component that forgets to release the port-80
// listener, or releases it after another component has already claimed it.
class FakePortal final : public ProvisioningPortal {
public:
    FakePortal() {
        status_[0] = '\0';
        pendingSsid_[0] = '\0';
    }

    void start(SubmitHandler handler, void* context) override {
        // Mirrors the real portal: refuse to start if port 80 is already taken.
        if (!HttpPortOwnership::acquire(HttpPortOwnership::Owner::PROVISIONING)) {
            ++refusedStarts;
            return;
        }
        handler_ = handler;
        context_ = context;
        active_ = true;
        ++starts;
    }

    void stop() override {
        if (!active_) {
            return;
        }
        // Mirrors the real portal: stop() AND close(), then release.
        listenerOpen = false;
        active_ = false;
        handler_ = nullptr;
        context_ = nullptr;
        pending_ = false;
        ++stops;
        HttpPortOwnership::release(HttpPortOwnership::Owner::PROVISIONING);
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

    void submit(const char* ssid, const char* password) {
        NetworkCredentials credentials;
        clearNetworkCredentials(credentials);
        if (ssid != nullptr && assignNetworkSsid(credentials, ssid) &&
            (password == nullptr || assignNetworkPassword(credentials, password))) {
            if (handler_ != nullptr) {
                handler_(credentials, context_);
            }
        }
    }

    bool active_ = false;
    bool pending_ = false;
    bool listenerOpen = false;
    uint16_t starts = 0;
    uint16_t stops = 0;
    uint16_t ticks = 0;
    uint16_t refusedStarts = 0;
    char status_[96];
    char pendingSsid_[33];
    SubmitHandler handler_ = nullptr;
    void* context_ = nullptr;
};

class FakeDriver final : public NetworkDriver {
public:
    FakeDriver() {
        lastAttemptedSsid_[0] = '\0';
        apSsid[0] = '\0';
    }

    // Teardown order is recorded here: when the AP is stopped, was the
    // provisioning HTTP server already gone? The whole point of the fix is
    // that the answer is yes.
    bool apStoppedWhilePortalStillOwningPort = false;

    void beginStation(const NetworkCredentials& credentials) override {
        ++beginStationCalls;
        strncpy(lastAttemptedSsid_, credentials.ssid, sizeof(lastAttemptedSsid_) - 1u);
        lastAttemptedSsid_[sizeof(lastAttemptedSsid_) - 1u] = '\0';
    }

    void startAccessPoint(const char* ssid, const char* password) override {
        ++accessPointStarts;
        apActive = true;
        (void)ssid;
        (void)password;
    }

    void stopAccessPoint() override {
        ++accessPointStops;
        apActive = false;
        if (HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING)) {
            apStoppedWhilePortalStillOwningPort = true;
        }
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

    bool connected = false;
    bool apActive = false;
    uint32_t address = 0xC0A80164u;
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
    FakeStore() {
        clearNetworkCredentials(value);
    }

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
    NetworkCredentials value;
    uint16_t loadCalls = 0;
    uint16_t saveCalls = 0;
    uint16_t clearCalls = 0;
};

struct Harness {
    FakeDriver driver;
    FakeStore store;
    FakePortal portal;
    NetworkManager manager;

    Harness()
        : manager(driver, store, &portal) {}

    // Ticks forward in bounded steps. Time is monotonic: it never restarts,
    // so a caller resuming a scenario must pass the current millisecond.
    void run(uint32_t fromMs, uint32_t toMs, uint32_t stepMs = 100u) {
        uint32_t t = fromMs;
        while (t <= toMs) {
            manager.tick(t);
            t += stepMs;
        }
    }
};

NetworkCredentials makeCredentials(const char* ssid, const char* password) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    assignNetworkSsid(credentials, ssid);
    assignNetworkPassword(credentials, password);
    return credentials;
}

}  // namespace

void test_port_ownership_is_exclusive(void) {
    HttpPortOwnership::releaseAll();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));

    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::PROVISIONING));
    TEST_ASSERT_TRUE(HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING));

    // A second, different owner must be refused while the port is taken.
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));

    // Re-acquiring the same owner is idempotent.
    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::PROVISIONING));

    HttpPortOwnership::release(HttpPortOwnership::Owner::PROVISIONING);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));
    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();
}

void test_releasing_by_non_owner_does_not_steal_the_port(void) {
    HttpPortOwnership::releaseAll();
    HttpPortOwnership::acquire(HttpPortOwnership::Owner::PROVISIONING);

    // A stray release from the wrong component must not free the port.
    HttpPortOwnership::release(HttpPortOwnership::Owner::LAN);
    TEST_ASSERT_TRUE(HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING));
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));

    HttpPortOwnership::releaseAll();
}

void test_lan_cannot_start_while_provisioning_owns_port(void) {
    HttpPortOwnership::releaseAll();
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);

    // Saved credentials exist, so no portal opens yet and the port is free.
    TEST_ASSERT_FALSE(h.manager.provisioningActive());
    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();

    // Router absent: the timeout opens the portal, which then owns port 80.
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
    TEST_ASSERT_TRUE(HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING));

    // A LAN server must not be able to claim the port while it is in use.
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();
}

void test_provisioning_releases_port_during_grace_period(void) {
    HttpPortOwnership::releaseAll();
    Harness h;
    h.manager.begin(0u);

    // User submits and the credentials are accepted.
    h.portal.submit("HomeNet", "hunter2");
    h.driver.connected = true;
    h.run(0u, 500u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    TEST_ASSERT_EQUAL_UINT8(1u, h.store.saveCalls);

    // The success page stays up for the grace period, and the portal still
    // owns port 80 throughout it. A LAN server starting now would conflict.
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
    TEST_ASSERT_TRUE(h.driver.apActive);
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));

    // Part-way through the grace period the portal is still up.
    h.run(500u, 2000u);
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));

    // After the grace period the portal stops, the AP goes down, and the port
    // is released.
    h.run(2100u, NetworkDefaults::kApTeardownGraceMs + 2000u);

    TEST_ASSERT_FALSE(h.portal.active_);
    TEST_ASSERT_FALSE(h.manager.provisioningActive());
    TEST_ASSERT_FALSE(h.driver.apActive);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));

    // Now, and only now, the LAN service may claim port 80.
    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();
}

void test_port_is_released_before_ap_teardown(void) {
    HttpPortOwnership::releaseAll();
    Harness h;
    h.manager.begin(0u);
    h.portal.submit("HomeNet", "hunter2");
    h.driver.connected = true;
    h.run(0u, 500u);

    // Step across the teardown boundary. The AP must never be stopped while
    // the provisioning server still owns port 80.
    h.run(500u, NetworkDefaults::kApTeardownGraceMs + 3000u);

    TEST_ASSERT_FALSE(h.driver.apStoppedWhilePortalStillOwningPort);
    TEST_ASSERT_EQUAL_UINT8(1u, h.portal.stops);
    TEST_ASSERT_EQUAL_UINT8(1u, h.driver.accessPointStops);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));
    HttpPortOwnership::releaseAll();
}

void test_port_is_free_after_failed_provisioning(void) {
    HttpPortOwnership::releaseAll();
    Harness h;
    h.manager.begin(0u);

    h.portal.submit("BadNet", "wrong");
    h.driver.connected = false;
    h.run(0u, NetworkDefaults::kPendingCredentialTimeoutMs + 500u);

    // A failed attempt keeps the portal running (that is the point), so the
    // port must still be owned by PROVISIONING and not by anything else.
    TEST_ASSERT_TRUE(h.portal.active_);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
    TEST_ASSERT_TRUE(HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING));
    TEST_ASSERT_FALSE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();
}

void test_boot_with_saved_credentials_releases_port_after_timeout(void) {
    HttpPortOwnership::releaseAll();
    Harness h;
    h.store.hasValue = true;
    h.store.value = makeCredentials("HomeNet", "hunter2");
    h.driver.connected = false;

    h.manager.begin(0u);
    // Starting with saved credentials does not open a portal.
    TEST_ASSERT_FALSE(h.manager.provisioningActive());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));

    // Router absent: timeout opens the portal, which then owns the port.
    h.run(0u, NetworkDefaults::kStaConnectTimeoutMs + 500u);
    TEST_ASSERT_TRUE(h.manager.provisioningActive());
    TEST_ASSERT_TRUE(HttpPortOwnership::isOwnedBy(HttpPortOwnership::Owner::PROVISIONING));

    // Router appears. This path has no pending credentials, so the portal is
    // stopped as soon as the association succeeds and the port is released
    // straight away.
    h.driver.connected = true;
    h.run(NetworkDefaults::kStaConnectTimeoutMs + 600u, NetworkDefaults::kStaConnectTimeoutMs + 1200u);

    TEST_ASSERT_TRUE(h.manager.isConnected());
    TEST_ASSERT_FALSE(h.portal.active_);
    TEST_ASSERT_FALSE(h.manager.provisioningActive());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(HttpPortOwnership::Owner::NONE),
        static_cast<uint8_t>(HttpPortOwnership::current()));

    // The LAN service can now claim port 80.
    TEST_ASSERT_TRUE(HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN));
    HttpPortOwnership::releaseAll();
}

void test_scheduler_unaffected_by_http_lifecycle(void) {
    // The HTTP fix must not perturb scheduling: an invalid clock still yields
    // the V1 fallback, with exactly one start, regardless of server churn.
    HttpPortOwnership::releaseAll();

    ManualTimeProvider clock;
    ManualOverride manual;
    Scheduler scheduler(clock, manual, SchedulerConfig());
    AnimationManager manager;
    PixelMatrix matrix;
    PlaybackPolicy policy(scheduler, manager, matrix);

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

    for (uint16_t i = 0u; i < 150u; ++i) {
        const uint32_t t = static_cast<uint32_t>(i) * 50u;
        h.manager.tick(t);
        policy.tick(t);
    }

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(AnimationId::AURORA),
        static_cast<uint8_t>(policy.appliedAnimation()));
    TEST_ASSERT_EQUAL_UINT16(1u, policy.startCount());
    HttpPortOwnership::releaseAll();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_port_ownership_is_exclusive);
    RUN_TEST(test_releasing_by_non_owner_does_not_steal_the_port);
    RUN_TEST(test_lan_cannot_start_while_provisioning_owns_port);
    RUN_TEST(test_provisioning_releases_port_during_grace_period);
    RUN_TEST(test_port_is_released_before_ap_teardown);
    RUN_TEST(test_port_is_free_after_failed_provisioning);
    RUN_TEST(test_boot_with_saved_credentials_releases_port_after_timeout);
    RUN_TEST(test_scheduler_unaffected_by_http_lifecycle);
    return UNITY_END();
}
