#include <unity.h>

#include <string.h>

#include "fakes/FakeNvs.h"
#include "network/CredentialStoreLogic.h"
#include "network/NetworkDefaults.h"
#include "network/NetworkManager.h"

namespace {

// NetworkCredentialStore backed by the REAL production persistence logic over
// the in-memory NVS double. Tests therefore exercise the shipping algorithm,
// not a reimplementation of it.
class NvsBackedStore final : public NetworkCredentialStore {
public:
    explicit NvsBackedStore(FakeNvs& nvs)
        : logic_(nvs, false) {}

    bool load(NetworkCredentials& out) override { return logic_.load(out); }
    bool save(const NetworkCredentials& credentials) override { return logic_.save(credentials); }
    bool clear() override { return logic_.clear(); }

    CredentialStoreLogic& logic() { return logic_; }

private:
    CredentialStoreLogic logic_;
};

class NullDriver final : public NetworkDriver {
public:
    NullDriver() {
        ssid[0] = '\0';
    }

    void beginStation(const NetworkCredentials& credentials) override {
        ++attempts;
        strncpy(ssid, credentials.ssid, sizeof(ssid) - 1u);
        ssid[sizeof(ssid) - 1u] = '\0';
    }
    void startAccessPoint(const char*, const char*) override {
        ++apStarts;
        apActive = true;
    }
    void stopAccessPoint() override {
        ++apStops;
        apActive = false;
    }
    bool accessPointActive() const override { return apActive; }
    bool stationConnected() const override { return connected; }
    void disconnectStation() override { connected = false; }
    uint32_t localAddress() const override { return 0u; }
    void currentSsid(char* out, uint8_t capacity) const override {
        if (out != nullptr && capacity > 0u) {
            strncpy(out, ssid, static_cast<size_t>(capacity) - 1u);
            out[static_cast<size_t>(capacity) - 1u] = '\0';
        }
    }
    uint8_t scanNetworks(char[][33], uint8_t) override { return 0u; }

    bool connected = false;
    bool apActive = false;
    uint16_t attempts = 0;
    uint16_t apStarts = 0;
    uint16_t apStops = 0;
    char ssid[33];
};

class NullPortal final : public ProvisioningPortal {
public:
    void start(SubmitHandler handler, void* context) override {
        handler_ = handler;
        context_ = context;
        active_ = true;
        ++starts;
    }
    void stop() override {
        active_ = false;
    }
    bool active() const override { return active_; }
    void tick() override {}

    void setPendingConnection(bool) override {}
    void setStatusMessage(const char*) override {}
    void setPendingSsid(const char*) override {}

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
    uint16_t starts = 0;
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

}  // namespace

// ---------------------------------------------------------------------------
// Representation / round trip
// ---------------------------------------------------------------------------

void test_save_then_load_roundtrip_is_exact(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);

    const NetworkCredentials saved = makeCredentials("HomeNet", "hunter2pass");
    TEST_ASSERT_TRUE(logic.save(saved));

    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(logic.load(loaded));
    TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("hunter2pass", loaded.password);
    TEST_ASSERT_TRUE(loaded.valid);

    // Byte-for-byte identical record, proven without printing the password.
    TEST_ASSERT_EQUAL_UINT32(
        CredentialStoreLogic::fingerprint(saved),
        CredentialStoreLogic::fingerprint(loaded));
}

void test_stored_representation_is_strlen_plus_one(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials("HomeNet", "pw")));

    // The terminator is part of the stored blob, otherwise an empty password
    // could never be written at all.
    TEST_ASSERT_EQUAL_UINT8(8u, static_cast<uint8_t>(nvs.getBytesLength("wifi_ssid")));
    TEST_ASSERT_EQUAL_UINT8(3u, static_cast<uint8_t>(nvs.getBytesLength("wifi_pass")));
}

void test_max_length_credentials_survive(void) {
    char ssid[33];
    char password[64];
    for (uint8_t i = 0u; i < 32u; ++i) {
        ssid[i] = 'S';
    }
    ssid[32] = '\0';
    for (uint8_t i = 0u; i < 63u; ++i) {
        password[i] = 'P';
    }
    password[63] = '\0';

    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials(ssid, password)));

    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(logic.load(loaded));
    TEST_ASSERT_EQUAL_STRING(ssid, loaded.ssid);
    TEST_ASSERT_EQUAL_STRING(password, loaded.password);
}

void test_open_network_persists_an_empty_password(void) {
    // A zero-length password must be storable. The raw-byte representation
    // could not do this: the backend rejects a zero-length blob.
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials("OpenNet", "")));

    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(logic.load(loaded));
    TEST_ASSERT_EQUAL_STRING("OpenNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("", loaded.password);
}

// ---------------------------------------------------------------------------
// Load rejection rules
// ---------------------------------------------------------------------------

void test_valid_marker_false_is_rejected(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials("HomeNet", "pw")));

    // Simulate a record whose commit marker was never set.
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", false);
    nvs.end();

    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
    TEST_ASSERT_FALSE(loaded.valid);
}

void test_missing_valid_marker_is_rejected(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBytes("wifi_ssid", "HomeNet\0", 8u);
    nvs.putBytes("wifi_pass", "pw\0", 3u);
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_missing_ssid_is_rejected(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", true);
    nvs.putBytes("wifi_pass", "pw\0", 3u);
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_overlong_stored_ssid_is_rejected(void) {
    char tooLong[40];
    memset(tooLong, 'X', sizeof(tooLong));

    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", true);
    nvs.putBytes("wifi_ssid", tooLong, sizeof(tooLong));
    nvs.putBytes("wifi_pass", "pw\0", 3u);
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_overlong_stored_password_is_rejected(void) {
    char tooLong[80];
    memset(tooLong, 'Y', sizeof(tooLong));

    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", true);
    nvs.putBytes("wifi_ssid", "HomeNet\0", 8u);
    nvs.putBytes("wifi_pass", tooLong, sizeof(tooLong));
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_empty_stored_ssid_is_rejected(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", true);
    // A terminator-only SSID is a 1-byte blob: legal to store, not legal to use.
    nvs.putBytes("wifi_ssid", "\0", 1u);
    nvs.putBytes("wifi_pass", "pw\0", 3u);
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_load_does_not_erase_the_record_on_failure(void) {
    FakeNvs nvs;
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", false);  // simulate an incomplete record
    nvs.putBytes("wifi_ssid", "HomeNet\0", 8u);
    nvs.end();

    CredentialStoreLogic logic(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));

    // A failed load must leave the stored bytes alone.
    TEST_ASSERT_TRUE(nvs.isKey("wifi_ssid"));
    TEST_ASSERT_EQUAL_UINT8(8u, static_cast<uint8_t>(nvs.getBytesLength("wifi_ssid")));
}

// ---------------------------------------------------------------------------
// Commit ordering
// ---------------------------------------------------------------------------

void test_failed_ssid_write_never_sets_valid_true(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);

    nvs.failNextPutBytes();  // the SSID write fails
    TEST_ASSERT_FALSE(logic.save(makeCredentials("HomeNet", "pw")));

    // The marker must not be true over missing payload.
    nvs.begin("pixelheart", false);
    const bool marker = nvs.getBool("wifi_valid", true);
    nvs.end();
    TEST_ASSERT_FALSE(marker);

    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_failed_password_write_never_sets_valid_true(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);

    // Invalidate marker, write SSID, then fail on the password.
    nvs.begin("pixelheart", false);
    nvs.putBool("wifi_valid", false);
    nvs.end();
    nvs.failNextPutBytes();
    nvs.failNextPutBytes();

    TEST_ASSERT_FALSE(logic.save(makeCredentials("HomeNet", "pw")));

    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
}

void test_valid_marker_is_written_last(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials("HomeNet", "pw")));

    // marker first (invalidating), then ssid, then password, then marker last.
    const uint16_t writes = nvs.writeCount();
    TEST_ASSERT_TRUE(writes >= 4u);

    nvs.begin("pixelheart", false);
    const bool marker = nvs.getBool("wifi_valid", false);
    nvs.end();
    TEST_ASSERT_TRUE(marker);
}

void test_clear_removes_everything(void) {
    FakeNvs nvs;
    CredentialStoreLogic logic(nvs);
    TEST_ASSERT_TRUE(logic.save(makeCredentials("HomeNet", "pw")));
    TEST_ASSERT_TRUE(logic.clear());

    NetworkCredentials loaded;
    TEST_ASSERT_FALSE(logic.load(loaded));
    TEST_ASSERT_FALSE(nvs.isKey("wifi_ssid"));
    TEST_ASSERT_FALSE(nvs.isKey("wifi_pass"));
    TEST_ASSERT_FALSE(nvs.isKey("wifi_valid"));
}

void test_fingerprint_is_stable_and_discriminating(void) {
    const NetworkCredentials a = makeCredentials("HomeNet", "pw");
    const NetworkCredentials b = makeCredentials("HomeNet", "pw");
    const NetworkCredentials c = makeCredentials("HomeNet", "other");

    TEST_ASSERT_EQUAL_UINT32(
        CredentialStoreLogic::fingerprint(a),
        CredentialStoreLogic::fingerprint(b));
    TEST_ASSERT_TRUE(
        CredentialStoreLogic::fingerprint(a) != CredentialStoreLogic::fingerprint(c));

    // Serialisation order must not be ambiguous.
    const NetworkCredentials ab = makeCredentials("ab", "c");
    const NetworkCredentials a_bc = makeCredentials("a", "bc");
    TEST_ASSERT_TRUE(
        CredentialStoreLogic::fingerprint(ab) != CredentialStoreLogic::fingerprint(a_bc));
}

// ---------------------------------------------------------------------------
// TRUE reboot simulation over a shared persistent backend
// ---------------------------------------------------------------------------

void test_credentials_survive_a_simulated_power_cycle(void) {
    // One NVS backend that outlives every object built on it: the native
    // equivalent of flash storage persisting across a power cycle.
    FakeNvs nvs;

    {
        // ---------------- BOOT 1: provision and connect ----------------
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);

        manager.begin(0u);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(NetworkState::PROVISIONING),
            static_cast<uint8_t>(manager.state()));

        portal.submit("MikroTik-CB390D", "correct-horse");
        driver.connected = true;

        uint32_t t = 0u;
        for (uint16_t i = 0u; i < 100u; ++i) {
            manager.tick(t);
            t += 50u;
        }

        TEST_ASSERT_TRUE(manager.isConnected());
    }

    // The store and manager above are gone. Only the NVS bytes remain.

    {
        // ---------------- BOOT 2: must load without a portal --------------
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);

        manager.begin(0u);

        // Loaded from storage, NOT from RAM state.
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(NetworkState::WAIT_STA),
            static_cast<uint8_t>(manager.state()));
        TEST_ASSERT_EQUAL_UINT8(1u, driver.attempts);
        TEST_ASSERT_EQUAL_STRING("MikroTik-CB390D", driver.ssid);

        // The provisioning portal must NOT have been started.
        TEST_ASSERT_EQUAL_UINT8(0u, portal.starts);
        TEST_ASSERT_FALSE(portal.active_);
        TEST_ASSERT_FALSE(driver.apActive);

        driver.connected = true;
        manager.tick(1000u);
        TEST_ASSERT_TRUE(manager.isConnected());
    }
}

void test_reboot_survives_three_consecutive_power_cycles(void) {
    FakeNvs nvs;

    // Provision once.
    {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);
        manager.begin(0u);
        portal.submit("HomeNet", "hunter2");
        driver.connected = true;
        uint32_t t = 0u;
        for (uint16_t i = 0u; i < 100u; ++i) {
            manager.tick(t);
            t += 50u;
        }
        TEST_ASSERT_TRUE(manager.isConnected());
    }

    // Then reboot three times; every boot must reconnect with no portal.
    for (uint8_t boot = 0u; boot < 3u; ++boot) {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);

        manager.begin(1000u);

        TEST_ASSERT_EQUAL_UINT8(0u, portal.starts);
        TEST_ASSERT_EQUAL_UINT8(1u, driver.attempts);
        TEST_ASSERT_EQUAL_STRING("HomeNet", driver.ssid);

        driver.connected = true;
        manager.tick(2000u);
        TEST_ASSERT_TRUE(manager.isConnected());
    }
}

void test_second_manager_loads_credentials_written_by_the_first(void) {
    FakeNvs nvs;

    NvsBackedStore writer(nvs);
    TEST_ASSERT_TRUE(writer.save(makeCredentials("HomeNet", "hunter2")));

    // A completely separate store object, over the same backend bytes.
    NvsBackedStore reader(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(reader.load(loaded));
    TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("hunter2", loaded.password);
}

// ---------------------------------------------------------------------------
// Interaction with the state machine
// ---------------------------------------------------------------------------

void test_sta_timeout_preserves_stored_credentials(void) {
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("HomeNet", "hunter2")));
    }

    NullDriver driver;  // never connects: router is down
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    uint32_t t = 0u;
    for (uint16_t i = 0u; i < 500u; ++i) {
        manager.tick(t);
        t += 100u;
    }

    // Provisioning opened because the router is missing, but the stored
    // record must be untouched.
    TEST_ASSERT_FALSE(manager.isConnected());
    TEST_ASSERT_EQUAL_UINT8(1u, portal.starts);

    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.ssid);
}

void test_failed_new_credentials_leave_the_old_record_intact(void) {
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("GoodNet", "goodpass")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    // Boot with a working record, but the router is unreachable.
    manager.begin(0u);
    uint32_t t = 0u;
    for (uint16_t i = 0u; i < 300u; ++i) {
        manager.tick(t);
        t += 100u;
    }

    // User re-provisions with a wrong password.
    portal.submit("WrongNet", "badpass");
    for (uint16_t i = 0u; i < 300u; ++i) {
        manager.tick(t);
        t += 100u;
    }

    // The known-good record must be exactly as before.
    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("GoodNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("goodpass", loaded.password);
}

void test_no_stored_credentials_starts_provisioning(void) {
    FakeNvs nvs;
    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_EQUAL_UINT8(1u, portal.starts);
    TEST_ASSERT_EQUAL_UINT8(0u, driver.attempts);
}

// ---------------------------------------------------------------------------
// "Change Wi-Fi" from the LAN page
// ---------------------------------------------------------------------------

void test_start_provisioning_is_deferred_and_does_no_radio_work(void) {
    // The HTTP handler calls startProvisioning() from inside handleClient().
    // It must therefore be non-blocking AND must not touch the radio, or it
    // would tear down the server it is running inside.
    FakeNvs nvs;
    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    portal.submit("HomeNet", "hunter2");
    driver.connected = true;

    // Settle: connect, then let the post-success AP teardown grace elapse so
    // we start from a genuinely quiet state with no AP up.
    uint32_t t = 0u;
    for (uint16_t i = 0u; i < 300u; ++i) {
        manager.tick(t);
        t += 100u;
    }
    TEST_ASSERT_TRUE(manager.isConnected());
    TEST_ASSERT_FALSE(driver.apActive);

    const uint16_t apStartsBefore = driver.apStarts;
    const uint16_t portalStartsBefore = portal.starts;

    TEST_ASSERT_TRUE(manager.startProvisioning(t));

    // Deferred: no AP, no portal, no further work in the same call.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING_REQUESTED),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_EQUAL_UINT8(apStartsBefore, driver.apStarts);
    TEST_ASSERT_EQUAL_UINT8(portalStartsBefore, portal.starts);
    TEST_ASSERT_FALSE(driver.apActive);

    // The transition happens on a later tick.
    manager.tick(t + 1000u);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_TRUE(driver.apActive);
    TEST_ASSERT_EQUAL_UINT8(apStartsBefore + 1u, driver.apStarts);
    TEST_ASSERT_EQUAL_UINT8(portalStartsBefore + 1u, portal.starts);
}

void test_wifi_change_does_not_erase_stored_credentials(void) {
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("HomeNet", "hunter2")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    driver.connected = true;
    manager.tick(100u);
    TEST_ASSERT_TRUE(manager.isConnected());

    TEST_ASSERT_TRUE(manager.startProvisioning(1000u));
    manager.tick(1500u);

    // Entering provisioning must not touch the committed record.
    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("HomeNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("hunter2", loaded.password);
}

void test_user_provisioning_survives_a_live_old_connection(void) {
    // The critical case: while changing Wi-Fi the station is still associated
    // with the OLD network. That must NOT be mistaken for "already done", or
    // the portal would be torn down before the user could reach it.
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    driver.connected = true;
    manager.tick(100u);
    TEST_ASSERT_TRUE(manager.isConnected());

    manager.startProvisioning(1000u);
    manager.tick(1500u);

    // Old link still up, but the portal must remain available.
    TEST_ASSERT_TRUE(driver.connected);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_TRUE(portal.active_);
    TEST_ASSERT_TRUE(driver.apActive);

    // And it must stay available across many ticks.
    uint32_t t = 2000u;
    for (uint16_t i = 0u; i < 100u; ++i) {
        manager.tick(t);
        t += 100u;
    }
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_TRUE(portal.active_);
}

void test_user_provisioning_times_out_and_keeps_old_credentials(void) {
    // Case: the user opens Change Wi-Fi and never configures anything.
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    driver.connected = true;
    manager.tick(100u);
    TEST_ASSERT_TRUE(manager.isConnected());

    manager.startProvisioning(1000u);
    manager.tick(1500u);
    TEST_ASSERT_TRUE(portal.active_);

    // Cross the give-up deadline.
    uint32_t t = 2000u;
    const uint32_t deadline = 1500u + NetworkDefaults::kUserProvisioningTimeoutMs + 2000u;
    while (t <= deadline) {
        manager.tick(t);
        t += 500u;
    }

    // Provisioning is over, the AP is down, and normal operation resumed.
    TEST_ASSERT_FALSE(portal.active_);
    TEST_ASSERT_FALSE(driver.apActive);
    TEST_ASSERT_FALSE(manager.userProvisioningActive());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::CONNECTED),
        static_cast<uint8_t>(manager.state()));
    TEST_ASSERT_TRUE(manager.isConnected());

    // The old record is untouched and still loads.
    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("OldNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("oldpass", loaded.password);
}

void test_successful_change_replaces_the_stored_credentials(void) {
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    driver.connected = true;
    manager.tick(100u);
    TEST_ASSERT_TRUE(manager.isConnected());

    manager.startProvisioning(1000u);
    manager.tick(1500u);
    TEST_ASSERT_TRUE(portal.active_);

    // User submits a DIFFERENT network, which succeeds.
    portal.submit("NewNet", "newpass");
    manager.tick(2000u);
    TEST_ASSERT_TRUE(manager.isConnected());

    // The new record replaced the old one.
    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("NewNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("newpass", loaded.password);
}

void test_failed_change_keeps_old_credentials_and_portal(void) {
    // Case: wrong password for the new network.
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    NullDriver driver;
    NullPortal portal;
    NvsBackedStore store(nvs);
    NetworkManager manager(driver, store, &portal);

    manager.begin(0u);
    driver.connected = true;
    manager.tick(100u);
    TEST_ASSERT_TRUE(manager.isConnected());

    manager.startProvisioning(1000u);
    manager.tick(1500u);

    // Submit a bad network: the driver never reports connected.
    driver.connected = false;
    portal.submit("BadNet", "wrongpass");

    uint32_t t = 2000u;
    for (uint16_t i = 0u; i < 500u; ++i) {
        manager.tick(t);
        t += 200u;
    }

    // Portal stays available so the user can retry.
    TEST_ASSERT_TRUE(portal.active_);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(NetworkState::PROVISIONING),
        static_cast<uint8_t>(manager.state()));

    // Old credentials are untouched.
    NvsBackedStore verify(nvs);
    NetworkCredentials loaded;
    TEST_ASSERT_TRUE(verify.load(loaded));
    TEST_ASSERT_EQUAL_STRING("OldNet", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("oldpass", loaded.password);
}

void test_power_loss_during_provisioning_stays_recoverable(void) {
    // Case: power is cut while the setup portal is open.
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);

        manager.begin(0u);
        driver.connected = true;
        manager.tick(100u);

        manager.startProvisioning(1000u);
        manager.tick(1500u);

        // User starts typing, then the power goes.
        TEST_ASSERT_TRUE(portal.active_);
    }

    // ---------------- power cycle ----------------
    {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);

        manager.begin(5000u);

        // The old record survived and is used immediately.
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(NetworkState::WAIT_STA),
            static_cast<uint8_t>(manager.state()));
        TEST_ASSERT_EQUAL_UINT8(1u, driver.attempts);
        TEST_ASSERT_EQUAL_STRING("OldNet", driver.ssid);

        // No automatic portal: the user did not ask for one this boot.
        TEST_ASSERT_EQUAL_UINT8(0u, portal.starts);

        driver.connected = true;
        manager.tick(6000u);
        TEST_ASSERT_TRUE(manager.isConnected());
    }
}

void test_normal_boot_after_change_uses_the_new_network(void) {
    FakeNvs nvs;
    {
        NvsBackedStore seed(nvs);
        TEST_ASSERT_TRUE(seed.save(makeCredentials("OldNet", "oldpass")));
    }

    // Change to NewNet and succeed.
    {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);
        manager.begin(0u);
        driver.connected = true;
        manager.tick(100u);
        manager.startProvisioning(1000u);
        manager.tick(1500u);
        portal.submit("NewNet", "newpass");
        manager.tick(2000u);
        TEST_ASSERT_TRUE(manager.isConnected());
        // Let the AP teardown grace period elapse.
        uint32_t t = 2100u;
        for (uint16_t i = 0u; i < 200u; ++i) {
            manager.tick(t);
            t += 100u;
        }
    }

    // Power cycle: reconnect to the NEW network with no portal.
    {
        NullDriver driver;
        NullPortal portal;
        NvsBackedStore store(nvs);
        NetworkManager manager(driver, store, &portal);
        manager.begin(10000u);

        TEST_ASSERT_EQUAL_UINT8(1u, driver.attempts);
        TEST_ASSERT_EQUAL_STRING("NewNet", driver.ssid);
        TEST_ASSERT_EQUAL_UINT8(0u, portal.starts);

        driver.connected = true;
        manager.tick(11000u);
        TEST_ASSERT_TRUE(manager.isConnected());
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_save_then_load_roundtrip_is_exact);
    RUN_TEST(test_stored_representation_is_strlen_plus_one);
    RUN_TEST(test_max_length_credentials_survive);
    RUN_TEST(test_open_network_persists_an_empty_password);
    RUN_TEST(test_valid_marker_false_is_rejected);
    RUN_TEST(test_missing_valid_marker_is_rejected);
    RUN_TEST(test_missing_ssid_is_rejected);
    RUN_TEST(test_overlong_stored_ssid_is_rejected);
    RUN_TEST(test_overlong_stored_password_is_rejected);
    RUN_TEST(test_empty_stored_ssid_is_rejected);
    RUN_TEST(test_load_does_not_erase_the_record_on_failure);
    RUN_TEST(test_failed_ssid_write_never_sets_valid_true);
    RUN_TEST(test_failed_password_write_never_sets_valid_true);
    RUN_TEST(test_valid_marker_is_written_last);
    RUN_TEST(test_clear_removes_everything);
    RUN_TEST(test_fingerprint_is_stable_and_discriminating);
    RUN_TEST(test_credentials_survive_a_simulated_power_cycle);
    RUN_TEST(test_reboot_survives_three_consecutive_power_cycles);
    RUN_TEST(test_second_manager_loads_credentials_written_by_the_first);
    RUN_TEST(test_sta_timeout_preserves_stored_credentials);
    RUN_TEST(test_failed_new_credentials_leave_the_old_record_intact);
    RUN_TEST(test_no_stored_credentials_starts_provisioning);
    RUN_TEST(test_start_provisioning_is_deferred_and_does_no_radio_work);
    RUN_TEST(test_wifi_change_does_not_erase_stored_credentials);
    RUN_TEST(test_user_provisioning_survives_a_live_old_connection);
    RUN_TEST(test_user_provisioning_times_out_and_keeps_old_credentials);
    RUN_TEST(test_successful_change_replaces_the_stored_credentials);
    RUN_TEST(test_failed_change_keeps_old_credentials_and_portal);
    RUN_TEST(test_power_loss_during_provisioning_stays_recoverable);
    RUN_TEST(test_normal_boot_after_change_uses_the_new_network);
    return UNITY_END();
}
