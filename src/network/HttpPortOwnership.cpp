#include "network/HttpPortOwnership.h"

namespace {
// File-scope, so the invariant holds across the provisioning portal and the
// LAN status service, which live in separate translation units.
HttpPortOwnership::Owner gOwner = HttpPortOwnership::Owner::NONE;
}  // namespace

void HttpPortOwnership::releaseAll() {
    gOwner = Owner::NONE;
}

bool HttpPortOwnership::acquire(Owner owner) {
    if (owner == Owner::NONE) {
        return false;
    }

    // Already ours: idempotent, so a repeated begin() is harmless.
    if (gOwner == owner) {
        return true;
    }

    if (gOwner != Owner::NONE) {
        return false;
    }

    gOwner = owner;
    return true;
}

void HttpPortOwnership::release(Owner owner) {
    if (gOwner == owner) {
        gOwner = Owner::NONE;
    }
}

HttpPortOwnership::Owner HttpPortOwnership::current() {
    return gOwner;
}

bool HttpPortOwnership::isOwnedBy(Owner owner) {
    return gOwner == owner;
}

bool HttpPortOwnership::canAcquire(Owner owner) {
    return owner != Owner::NONE && gOwner == owner;
}
