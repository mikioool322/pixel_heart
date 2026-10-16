#pragma once

#include <stdint.h>

// Single-owner bookkeeping for the shared TCP port 80 HTTP listener.
//
// The ESP32 Arduino WebServer binds a real socket in WiFiServer::begin(), so
// two live WebServer(80) instances are a genuine resource conflict, not just
// a conceptual one. Exactly one component may own the HTTP listener at a
// time, and ownership must be released before another component claims it.
//
// This header is deliberately free of Arduino and ESP includes so the
// ownership ordering can be unit tested in the native environment.
namespace HttpPortOwnership {

constexpr uint16_t kHttpPort = 80u;

enum class Owner : uint8_t {
    NONE = 0,
    PROVISIONING,
    LAN
};

void releaseAll();

bool acquire(Owner owner);
void release(Owner owner);

Owner current();
bool isOwnedBy(Owner owner);

// True when the HTTP listener is free for the given owner to take.
bool canAcquire(Owner owner);

}  // namespace HttpPortOwnership
