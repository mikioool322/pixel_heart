#include <unity.h>

#include <string.h>

#include "network/NetworkCredentials.h"

namespace {
constexpr uint8_t kMaxSsid = NetworkCredentials::kMaxSsidLength;
constexpr uint8_t kMaxPassword = NetworkCredentials::kMaxPasswordLength;
}  // namespace

void test_clear_produces_terminated_empty_strings(void) {
    NetworkCredentials credentials;
    memset(&credentials, 0xAB, sizeof(credentials));

    clearNetworkCredentials(credentials);

    TEST_ASSERT_EQUAL_UINT8('\0', credentials.ssid[0]);
    TEST_ASSERT_EQUAL_UINT8('\0', credentials.password[0]);
    TEST_ASSERT_FALSE(credentials.valid);
}

void test_ssid_accepts_valid_lengths(void) {
    NetworkCredentials credentials;

    // 1 character.
    TEST_ASSERT_TRUE(assignNetworkSsid(credentials, "A"));
    TEST_ASSERT_EQUAL_STRING("A", credentials.ssid);
    TEST_ASSERT_TRUE(credentials.valid);

    // Exactly the 32-byte maximum.
    char longest[kMaxSsid + 1u];
    for (uint8_t i = 0u; i < kMaxSsid; ++i) {
        longest[i] = 'N';
    }
    longest[kMaxSsid] = '\0';

    TEST_ASSERT_TRUE(assignNetworkSsid(credentials, longest));
    TEST_ASSERT_EQUAL_UINT8(kMaxSsid, static_cast<uint8_t>(strlen(credentials.ssid)));
    TEST_ASSERT_EQUAL_UINT8('\0', credentials.ssid[kMaxSsid]);
}

void test_ssid_rejects_empty_and_over_long(void) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    assignNetworkSsid(credentials, "KnownGood");

    // 33 characters must be rejected and must not disturb the previous value.
    char tooLong[kMaxSsid + 2u];
    for (uint8_t i = 0u; i <= kMaxSsid; ++i) {
        tooLong[i] = 'X';
    }
    tooLong[kMaxSsid + 1u] = '\0';

    TEST_ASSERT_FALSE(assignNetworkSsid(credentials, tooLong));
    TEST_ASSERT_EQUAL_STRING("KnownGood", credentials.ssid);

    TEST_ASSERT_FALSE(assignNetworkSsid(credentials, ""));
    TEST_ASSERT_EQUAL_STRING("KnownGood", credentials.ssid);

    TEST_ASSERT_FALSE(assignNetworkSsid(credentials, nullptr));
    TEST_ASSERT_EQUAL_STRING("KnownGood", credentials.ssid);
}

void test_password_accepts_empty_for_open_network(void) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    TEST_ASSERT_TRUE(assignNetworkSsid(credentials, "OpenNet"));

    TEST_ASSERT_TRUE(assignNetworkPassword(credentials, ""));
    TEST_ASSERT_EQUAL_STRING("", credentials.password);
    TEST_ASSERT_TRUE(isOpenNetwork(credentials));

    TEST_ASSERT_TRUE(assignNetworkPassword(credentials, nullptr));
    TEST_ASSERT_EQUAL_STRING("", credentials.password);
}

void test_password_accepts_max_length_and_rejects_over_long(void) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    assignNetworkSsid(credentials, "Net");
    assignNetworkPassword(credentials, "secret");

    char longest[kMaxPassword + 1u];
    for (uint8_t i = 0u; i < kMaxPassword; ++i) {
        longest[i] = 'p';
    }
    longest[kMaxPassword] = '\0';

    TEST_ASSERT_TRUE(assignNetworkPassword(credentials, longest));
    TEST_ASSERT_EQUAL_UINT8(kMaxPassword, static_cast<uint8_t>(strlen(credentials.password)));
    TEST_ASSERT_EQUAL_UINT8('\0', credentials.password[kMaxPassword]);

    char tooLong[kMaxPassword + 2u];
    for (uint8_t i = 0u; i <= kMaxPassword; ++i) {
        tooLong[i] = 'p';
    }
    tooLong[kMaxPassword + 1u] = '\0';

    TEST_ASSERT_FALSE(assignNetworkPassword(credentials, tooLong));
    // The rejected value must not have been partially written.
    TEST_ASSERT_EQUAL_UINT8(kMaxPassword, static_cast<uint8_t>(strlen(credentials.password)));
}

void test_record_validation_matches_assignment_rules(void) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);

    // valid flag not set yet.
    TEST_ASSERT_FALSE(areNetworkCredentialsValid(credentials));

    assignNetworkSsid(credentials, "HomeNet");
    assignNetworkPassword(credentials, "hunter2");
    TEST_ASSERT_TRUE(areNetworkCredentialsValid(credentials));

    // A record whose valid flag is false must never be trusted, even with
    // well-formed strings present.
    credentials.valid = false;
    TEST_ASSERT_FALSE(areNetworkCredentialsValid(credentials));
}

void test_record_with_unterminated_ssid_is_rejected(void) {
    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);
    assignNetworkSsid(credentials, "HomeNet");
    assignNetworkPassword(credentials, "pw");

    // Simulate a corrupt / truncated stored record: fill past the terminator.
    for (uint8_t i = 0u; i <= kMaxSsid; ++i) {
        credentials.ssid[i] = 'Z';
    }

    // No terminator within the scan window means the SSID is not valid.
    TEST_ASSERT_FALSE(areNetworkCredentialsValid(credentials));
}

void test_credentials_are_plain_pod_without_dynamic_state(void) {
    // sizeof must stay trivially small: this struct is held by value in the
    // state machine and mirrored into NVS.
    TEST_ASSERT_TRUE(sizeof(NetworkCredentials) <= 128u);
    TEST_ASSERT_EQUAL_UINT8('\0', NetworkCredentials{}.ssid[0]);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_clear_produces_terminated_empty_strings);
    RUN_TEST(test_ssid_accepts_valid_lengths);
    RUN_TEST(test_ssid_rejects_empty_and_over_long);
    RUN_TEST(test_password_accepts_empty_for_open_network);
    RUN_TEST(test_password_accepts_max_length_and_rejects_over_long);
    RUN_TEST(test_record_validation_matches_assignment_rules);
    RUN_TEST(test_record_with_unterminated_ssid_is_rejected);
    RUN_TEST(test_credentials_are_plain_pod_without_dynamic_state);
    return UNITY_END();
}
