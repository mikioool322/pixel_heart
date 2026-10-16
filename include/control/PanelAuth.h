#pragma once

#include <stdint.h>
#include <string.h>

// LAN panel access control.
//
// The state-changing endpoints are protected with HTTP Basic Auth, performed
// by the ESP32 WebServer's own authenticate()/requestAuthentication(). This
// header holds only the part that is worth testing on the host: the policy
// deciding which requests are protected, and whether the configured
// credentials are representable in an Authorization header at all.
//
// The password below is this version's configured credential. It is not
// repeated anywhere else in the source, and it is never logged, rendered or
// returned in a response.
namespace PanelAuth {

constexpr const char* kUsername = "wiki";
constexpr const char* kPassword = "061000";

// Realm shown in the browser's credential dialog.
constexpr const char* kRealm = "Pixel Heart";

// Body of the 401 response.
//
// This is NOT a redirect: the response stays a real HTTP 401 carrying
// WWW-Authenticate, which is what makes Basic Auth work and what curl sees.
// The text below is only what a browser renders AFTER the user dismisses the
// dialog, instead of a bare "Unauthorized" page that strands them with no way
// back to the panel.
//
// A 1 second refresh is used rather than 0 so the message is actually
// readable; the panel is reloaded straight afterwards either way, and the
// manual link covers a browser that declines to honour a refresh inside a 401
// response. No JavaScript is involved.
constexpr const char* kAuthFailureBody =
    "<!doctype html><meta charset=\"utf-8\">"
    "<meta http-equiv=\"refresh\" content=\"1;url=/\">"
    "<title>Logowanie anulowane</title>"
    "<p>Logowanie anulowane.</p>"
    "<a href=\"/\">Wr\u00f3\u0107 do panelu</a>";

// HTTP methods that cannot change device state, and are therefore public.
//
// This is expressed as "everything except these methods is protected" rather
// than as a list of protected paths, so a route added later is authenticated
// by default instead of silently exposed.
inline bool isReadOnlyMethod(const char* methodToken) {
    if (methodToken == nullptr) {
        return false;
    }

    return strcmp(methodToken, "GET") == 0 ||
           strcmp(methodToken, "HEAD") == 0 ||
           strcmp(methodToken, "OPTIONS") == 0;
}

// True when the request needs credentials before it may be acted on.
// Fails closed: an unrecognised or missing method token is treated as
// state-changing.
inline bool requiresAuthForMethodToken(const char* methodToken) {
    if (methodToken == nullptr) {
        return true;
    }

    return !isReadOnlyMethod(methodToken);
}

// The complete gate: read-only methods are always allowed, everything else
// requires valid credentials.
//
// This is the decision the LAN service makes for every request. Keeping it
// here means the security property "an unauthenticated state-changing request
// is never allowed through" is a tested function rather than an audit claim,
// and it is the reason no handler can mutate state before authentication.
inline bool allowRequest(const char* methodToken, bool authenticated) {
    return !requiresAuthForMethodToken(methodToken) || authenticated;
}

// True when the configured credentials can actually be used.
//
// A ':' in the username would make the "user:password" wire format ambiguous,
// and an empty value can never match, so both are treated as a configuration
// error. This is checked once at startup rather than per request.
inline bool credentialsConfigured() {
    if (kUsername == nullptr || kPassword == nullptr) {
        return false;
    }

    if (kUsername[0] == '\0' || kPassword[0] == '\0') {
        return false;
    }

    return strchr(kUsername, ':') == nullptr;
}

}  // namespace PanelAuth
