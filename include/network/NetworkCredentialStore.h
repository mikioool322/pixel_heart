#pragma once

#include "network/NetworkCredentials.h"

// Persistence boundary for Wi-Fi credentials.
//
// Implementations must be non-blocking and must never log secrets.
class NetworkCredentialStore {
public:
    virtual ~NetworkCredentialStore() = default;

    // Reads the stored credentials. Returns false when nothing usable is
    // stored, leaving `out` cleared.
    virtual bool load(NetworkCredentials& out) = 0;

    // Writes credentials durably. Only ever called after a successful
    // connection, never for an unvalidated or failed attempt.
    virtual bool save(const NetworkCredentials& credentials) = 0;

    // Forgets stored credentials. Used only on an explicit user request.
    virtual bool clear() = 0;
};
