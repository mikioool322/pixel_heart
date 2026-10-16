#pragma once

#include "network/NetworkCredentials.h"

// Single place for every compile-time network constant.
//
// Kept free of Arduino / ESP headers so it can be asserted from native tests.
namespace NetworkDefaults {

// Namespace and keys used in ESP32 NVS (Preferences).
constexpr const char* kNvsNamespace = "pixelheart";
constexpr const char* kKeySsid = "wifi_ssid";
constexpr const char* kKeyPassword = "wifi_pass";
constexpr const char* kKeyValid = "wifi_valid";

// Setup access point.
constexpr const char* kSetupApSsidPrefix = "PixelHeart-Setup";

// Setup AP password only.
// This is NOT the user's home Wi-Fi password.
constexpr const char* kSetupApPassword = "pixelheart16";

// Captive portal address (first address of the default softAP DHCP range).
constexpr const char* kPortalAddress = "192.168.4.1";

// Bounded, non-blocking connection budgets (milliseconds).
constexpr uint32_t kStaConnectTimeoutMs = 20000UL;
constexpr uint32_t kPendingCredentialTimeoutMs = 20000UL;
constexpr uint32_t kStaRetryIntervalMs = 60000UL;

// Grace period before the setup AP is torn down after a successful join.
constexpr uint32_t kApTeardownGraceMs = 5000UL;

// How long a USER-INITIATED provisioning session may stay open before the
// device gives up and returns to normal operation with the previous
// credentials. Only applies when provisioning was requested from the LAN
// page, never to the automatic first-boot portal.
constexpr uint32_t kUserProvisioningTimeoutMs = 5UL * 60UL * 1000UL;

// Maximum number of scan results cached for the portal page.
constexpr uint8_t kMaxScanResults = 12;

// mDNS hostname advertised on the home network.
constexpr const char* kMdnsHostname = "pixelheart";

}  // namespace NetworkDefaults
