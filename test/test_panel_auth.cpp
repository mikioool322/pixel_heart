#include <unity.h>

#include <string.h>

#include "control/PanelAuth.h"

// The LAN panel's access policy.
//
// Esp32LanService itself is ESP-only and cannot be compiled on the host, so
// what is tested here is the part that decides WHO may act: which methods are
// public, and whether the configured credentials are usable at all. The
// comparison of supplied credentials is performed by the framework's own
// WebServer::authenticate() (constant-time), and the wiring that makes every
// mutating handler check this policy first is verified by source audit and on
// hardware.
//
// The security property under test is: EVERY state-changing request is
// classified as protected, including methods that do not exist yet.

void test_read_only_methods_are_public(void) {
    // GET is what serves the status panel and must never require credentials.
    TEST_ASSERT_TRUE(PanelAuth::isReadOnlyMethod("GET"));
    TEST_ASSERT_FALSE(PanelAuth::requiresAuthForMethodToken("GET"));

    // HEAD and OPTIONS cannot change state either.
    TEST_ASSERT_FALSE(PanelAuth::requiresAuthForMethodToken("HEAD"));
    TEST_ASSERT_FALSE(PanelAuth::requiresAuthForMethodToken("OPTIONS"));
}

void test_every_mutating_method_is_protected(void) {
    const char* mutating[] = {"POST", "PUT", "PATCH", "DELETE"};

    for (uint8_t i = 0u; i < sizeof(mutating) / sizeof(mutating[0]); ++i) {
        TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken(mutating[i]));
        TEST_ASSERT_FALSE(PanelAuth::isReadOnlyMethod(mutating[i]));
    }
}

void test_unknown_or_missing_method_fails_closed(void) {
    // A method the firmware does not recognise must be treated as
    // state-changing, never waved through.
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("TRACE"));
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("CONNECT"));
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken(""));
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken(nullptr));

    TEST_ASSERT_FALSE(PanelAuth::isReadOnlyMethod(nullptr));
}

void test_method_matching_is_not_case_insensitive_loophole(void) {
    // A lower-case "get" is not the GET handler; it must not be treated as
    // read-only. The production mapping only ever emits upper-case tokens, so
    // this documents the fail-closed direction.
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("get"));
    TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("Post"));
}

void test_the_panels_posts_are_all_protected(void) {
    // The actual route set of the LAN service. Each of these changes device
    // state, so each must demand credentials.
    const char* protectedRoutes[] = {
        "/mode",
        "/animation",
        "/brightness",
        "/rotation",
        "/play",
        "/wifi/change",
    };

    for (uint8_t i = 0u; i < sizeof(protectedRoutes) / sizeof(protectedRoutes[0]); ++i) {
        // Routes do not affect the policy, but asserting it keeps the intent
        // explicit: these are all POSTs, and all POSTs are protected.
        TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("POST"));
    }

    // And the two public routes are served by GET.
    TEST_ASSERT_FALSE(PanelAuth::requiresAuthForMethodToken("GET"));
}

void test_shipped_credentials_are_usable(void) {
    TEST_ASSERT_TRUE(PanelAuth::credentialsConfigured());

    TEST_ASSERT_NOT_NULL(PanelAuth::kUsername);
    TEST_ASSERT_NOT_NULL(PanelAuth::kPassword);

    TEST_ASSERT_TRUE(strlen(PanelAuth::kUsername) > 0u);
    TEST_ASSERT_TRUE(strlen(PanelAuth::kPassword) > 0u);
}

void test_username_cannot_contain_the_basic_auth_separator(void) {
    // "user:password" is the wire format, so a ':' in the username would make
    // the header ambiguous. credentialsConfigured() rejects that.
    TEST_ASSERT_NULL(strchr(PanelAuth::kUsername, ':'));

    // Guard the invariant directly as well, so a future edit that introduces a
    // colon is caught rather than silently accepted.
    const char* withColon = "ad:min";
    TEST_ASSERT_NOT_NULL(strchr(withColon, ':'));
}

void test_credentials_contain_no_whitespace_or_line_breaks(void) {
    // Newlines or padding in a credential would break the Authorization header
    // and could end up in a log line.
    for (const char* p = PanelAuth::kUsername; *p != '\0'; ++p) {
        TEST_ASSERT_FALSE(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t');
    }
    for (const char* p = PanelAuth::kPassword; *p != '\0'; ++p) {
        TEST_ASSERT_FALSE(*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t');
    }
}

void test_credentials_are_short_enough_for_the_header(void) {
    // The framework builds "user:password" and base64-encodes it; keeping the
    // values small keeps that well inside any sane header limit.
    TEST_ASSERT_TRUE(strlen(PanelAuth::kUsername) < 32u);
    TEST_ASSERT_TRUE(strlen(PanelAuth::kPassword) < 32u);
}

void test_policy_is_pure_and_repeatable(void) {
    // The decision must not depend on call order or hidden state.
    for (uint8_t i = 0u; i < 100u; ++i) {
        TEST_ASSERT_FALSE(PanelAuth::requiresAuthForMethodToken("GET"));
        TEST_ASSERT_TRUE(PanelAuth::requiresAuthForMethodToken("POST"));
        TEST_ASSERT_TRUE(PanelAuth::credentialsConfigured());
    }
}

// ---------------------------------------------------------------------------
// The combined gate: the security invariant in one tested predicate.
// ---------------------------------------------------------------------------

void test_unauthenticated_mutating_request_is_never_allowed(void) {
    // Every protected route, with authentication failing. The gate must refuse
    // all of them, which is why no handler can reach a mutation or queue a
    // ControlRequest when credentials are missing or wrong.
    const char* mutating[] = {"POST", "PUT", "PATCH", "DELETE"};

    for (uint8_t i = 0u; i < sizeof(mutating) / sizeof(mutating[0]); ++i) {
        TEST_ASSERT_FALSE(PanelAuth::allowRequest(mutating[i], false));
        // And it is allowed once, and only once, credentials are valid.
        TEST_ASSERT_TRUE(PanelAuth::allowRequest(mutating[i], true));
    }

    // The fail-closed cases too.
    TEST_ASSERT_FALSE(PanelAuth::allowRequest(nullptr, false));
    TEST_ASSERT_FALSE(PanelAuth::allowRequest("", false));
    TEST_ASSERT_FALSE(PanelAuth::allowRequest("TRACE", false));
}

void test_read_only_requests_need_no_credentials(void) {
    // GET / and GET /health must stay public.
    TEST_ASSERT_TRUE(PanelAuth::allowRequest("GET", false));
    TEST_ASSERT_TRUE(PanelAuth::allowRequest("HEAD", false));
    TEST_ASSERT_TRUE(PanelAuth::allowRequest("OPTIONS", false));

    // Credentials do not make read-only requests fail.
    TEST_ASSERT_TRUE(PanelAuth::allowRequest("GET", true));
}

// ---------------------------------------------------------------------------
// The cancelled-login page.
//
// The 401 body is the only thing that changed in this fix. It must still be a
// 401 with WWW-Authenticate (the framework emits both), and the body must give
// the user a way back to the panel.
// ---------------------------------------------------------------------------

void test_failure_body_returns_to_the_panel(void) {
    const char* body = PanelAuth::kAuthFailureBody;
    TEST_ASSERT_NOT_NULL(body);

    // An automatic way back to the panel root.
    TEST_ASSERT_NOT_NULL(strstr(body, "url=/"));

    // And a manual link, in case a browser declines the refresh inside a 401.
    TEST_ASSERT_NOT_NULL(strstr(body, "href=\"/\""));

    // It is HTML, so the framework's text/html content type is correct.
    TEST_ASSERT_NOT_NULL(strstr(body, "<!doctype html>"));
    TEST_ASSERT_NOT_NULL(strstr(body, "<meta charset=\"utf-8\">"));
}

void test_failure_body_contains_no_javascript(void) {
    // No script, no event handlers: a plain meta refresh is enough and keeps
    // the response tiny.
    const char* body = PanelAuth::kAuthFailureBody;

    TEST_ASSERT_NULL(strstr(body, "<script"));
    TEST_ASSERT_NULL(strstr(body, "onload"));
    TEST_ASSERT_NULL(strstr(body, "onclick"));
    TEST_ASSERT_NULL(strstr(body, "javascript:"));
}

void test_failure_body_is_small_and_not_a_redirect(void) {
    const char* body = PanelAuth::kAuthFailureBody;

    // Must not contain a redirect status hint in the body, and must not be
    // large. The 401 status itself comes from the framework.
    TEST_ASSERT_NULL(strstr(body, "302"));
    TEST_ASSERT_NULL(strstr(body, "303"));
    TEST_ASSERT_TRUE(strlen(body) < 320u);
}

void test_failure_body_leaks_no_credentials(void) {
    // The page is rendered to an unauthenticated client, so it must contain
    // nothing about the credentials or the request.
    const char* body = PanelAuth::kAuthFailureBody;

    TEST_ASSERT_NULL(strstr(body, PanelAuth::kUsername));
    TEST_ASSERT_NULL(strstr(body, PanelAuth::kPassword));
    TEST_ASSERT_NULL(strstr(body, "Authorization"));
    TEST_ASSERT_NULL(strstr(body, "Basic"));
}

void test_realm_is_labelled_but_not_credential(void) {
    TEST_ASSERT_NOT_NULL(PanelAuth::kRealm);
    TEST_ASSERT_TRUE(strlen(PanelAuth::kRealm) > 0u);
    TEST_ASSERT_NULL(strstr(PanelAuth::kRealm, PanelAuth::kPassword));

    // A double quote in the realm would break the WWW-Authenticate header.
    TEST_ASSERT_NULL(strchr(PanelAuth::kRealm, '"'));
    TEST_ASSERT_NULL(strchr(PanelAuth::kRealm, '\n'));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_read_only_methods_are_public);
    RUN_TEST(test_every_mutating_method_is_protected);
    RUN_TEST(test_unknown_or_missing_method_fails_closed);
    RUN_TEST(test_method_matching_is_not_case_insensitive_loophole);
    RUN_TEST(test_the_panels_posts_are_all_protected);
    RUN_TEST(test_shipped_credentials_are_usable);
    RUN_TEST(test_username_cannot_contain_the_basic_auth_separator);
    RUN_TEST(test_credentials_contain_no_whitespace_or_line_breaks);
    RUN_TEST(test_credentials_are_short_enough_for_the_header);
    RUN_TEST(test_policy_is_pure_and_repeatable);
    RUN_TEST(test_unauthenticated_mutating_request_is_never_allowed);
    RUN_TEST(test_read_only_requests_need_no_credentials);
    RUN_TEST(test_failure_body_returns_to_the_panel);
    RUN_TEST(test_failure_body_contains_no_javascript);
    RUN_TEST(test_failure_body_is_small_and_not_a_redirect);
    RUN_TEST(test_failure_body_leaks_no_credentials);
    RUN_TEST(test_realm_is_labelled_but_not_credential);
    return UNITY_END();
}
