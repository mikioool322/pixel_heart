#include "network/Esp32ProvisioningPortal.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "network/Esp32NetworkDriver.h"
#include "network/HttpPortOwnership.h"
#include "network/NetworkDefaults.h"

namespace {
DNSServer gDnsServer;
WebServer gServer(80);

// The portal must reach the driver to run an explicit scan. Set once by App
// so the portal stays independent of any particular driver type.
NetworkDriver* gScanDriver = nullptr;

constexpr uint8_t kMaxScanResults = NetworkDefaults::kMaxScanResults;

void appendEscaped(String& out, const char* text) {
    if (text == nullptr) {
        return;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        const char c = *p;
        if (c == '&' || c == '<' || c == '>' || c == '"' || c == '\'') {
            out += "&#";
            out += static_cast<int>(static_cast<unsigned char>(c));
            out += ';';
        } else {
            out += c;
        }
    }
}
}  // namespace

Esp32ProvisioningPortal::Esp32ProvisioningPortal()
    : active_(false),
      handler_(nullptr),
      context_(nullptr),
      pendingConnection_(false),
      routesRegistered_(false) {
    statusMessage_[0] = '\0';
    pendingSsid_[0] = '\0';
}

void Esp32ProvisioningPortal::setScanDriver(NetworkDriver* driver) {
    gScanDriver = driver;
}

void Esp32ProvisioningPortal::start(SubmitHandler handler, void* context) {
    if (active_) {
        return;
    }

    // Exactly one component may own the port-80 HTTP listener. Refuse to start
    // rather than fighting another server for the same port.
    if (!HttpPortOwnership::acquire(HttpPortOwnership::Owner::PROVISIONING)) {
        Serial.println(F("[HTTP] Provisioning server not started: port 80 already owned"));
        return;
    }

    handler_ = handler;
    context_ = context;

    // Register routes exactly once for the lifetime of the process. The
    // handler list on a WebServer is append-only, so a second provisioning
    // session (the "Change Wi-Fi" flow) must reuse the existing routes rather
    // than registering a duplicate set.
    if (!routesRegistered_) {
        gServer.on("/", HTTP_GET, [this]() { handleRoot(); });
        gServer.on("/connect", HTTP_POST, [this]() { handleConnect(); });
        gServer.on("/scan", HTTP_GET, [this]() { handleScan(); });

        // Captive-portal probe paths used by Android, iOS and Windows to detect
        // an open portal. Each is answered so the OS opens the setup page.
        gServer.on("/generate_204", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/gen_204", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/hotspot-detect.html", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/connecttest.txt", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/ncsi.txt", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/canonical.html", HTTP_ANY, [this]() { handleRoot(); });
        gServer.on("/success.txt", HTTP_ANY, [this]() { handleRoot(); });

        gServer.onNotFound([this]() { handleNotFound(); });

        routesRegistered_ = true;
    }

    // begin() is called only after every route exists: WebServer::begin()
    // internally calls close(), so calling it earlier would tear the listener
    // down again before the handlers were attached.
    gServer.begin();
    active_ = true;

    Serial.println(F("[HTTP] Provisioning server started"));

    // Answer every DNS query with the AP address so the phone's captive
    // portal probe resolves to us regardless of the hostname requested.
    const IPAddress apIp(192, 168, 4, 1);
    gDnsServer.start(53, "*", apIp);
}

void Esp32ProvisioningPortal::stop() {
    if (!active_) {
        return;
    }

    Serial.println(F("[HTTP] Provisioning server stopping"));

    // DNS first so no new client is steered to a dying portal.
    gDnsServer.stop();

    // stop() only closes the listening socket; close() additionally drops the
    // server state. Both are needed for the port to be genuinely free before
    // another WebServer binds it.
    gServer.stop();
    gServer.close();

    active_ = false;
    handler_ = nullptr;
    context_ = nullptr;
    pendingConnection_ = false;
    statusMessage_[0] = '\0';
    pendingSsid_[0] = '\0';

    // Release ownership only after the socket is actually closed, so nothing
    // else can start a port-80 server while this one is still listening.
    HttpPortOwnership::release(HttpPortOwnership::Owner::PROVISIONING);

    Serial.println(F("[HTTP] Provisioning server stopped"));
}

bool Esp32ProvisioningPortal::active() const {
    return active_;
}

void Esp32ProvisioningPortal::tick() {
    if (!active_) {
        return;
    }

    gDnsServer.processNextRequest();
    gServer.handleClient();
}

void Esp32ProvisioningPortal::setPendingConnection(bool pending) {
    pendingConnection_ = pending;
}

void Esp32ProvisioningPortal::setStatusMessage(const char* message) {
    if (message == nullptr) {
        statusMessage_[0] = '\0';
        return;
    }

    strncpy(statusMessage_, message, sizeof(statusMessage_) - 1u);
    statusMessage_[sizeof(statusMessage_) - 1u] = '\0';
}

void Esp32ProvisioningPortal::setPendingSsid(const char* ssid) {
    if (ssid == nullptr) {
        pendingSsid_[0] = '\0';
        return;
    }

    strncpy(pendingSsid_, ssid, sizeof(pendingSsid_) - 1u);
    pendingSsid_[sizeof(pendingSsid_) - 1u] = '\0';
}

void Esp32ProvisioningPortal::handleRoot() {
    sendPortalPage(200, "Pixel Heart Setup", pendingSsid_);
}

void Esp32ProvisioningPortal::handleNotFound() {
    sendPortalPage(404, "Not found", pendingSsid_);
}

void Esp32ProvisioningPortal::handleConnect() {
    if (pendingConnection_) {
        // A test is already in flight; do not start a second one.
        sendPortalPage(409, "Already trying to connect", pendingSsid_);
        return;
    }

    if (!gServer.hasArg("ssid")) {
        sendPortalPage(400, "Missing network name", "");
        return;
    }

    const String ssidArg = gServer.arg("ssid");
    const String passwordArg = gServer.arg("password");

    NetworkCredentials credentials;
    clearNetworkCredentials(credentials);

    // Shared validation: rejects empty / over-long SSID and over-long
    // passphrase before anything is attempted.
    if (!assignNetworkSsid(credentials, ssidArg.c_str()) ||
        !assignNetworkPassword(credentials, passwordArg.c_str())) {
        sendPortalPage(400, "Invalid network details", "");
        return;
    }

    if (handler_ != nullptr) {
        handler_(credentials, context_);
    }

    // Re-render with the pending state. The password is deliberately not
    // echoed back into the form.
    sendPortalPage(200, "Connecting...", credentials.ssid);
}

void Esp32ProvisioningPortal::handleScan() {
    // Explicit, user-initiated scan only. Never runs implicitly per request.
    char ssids[kMaxScanResults][33];
    uint8_t count = 0u;

    if (gScanDriver != nullptr) {
        count = gScanDriver->scanNetworks(&ssids[0], kMaxScanResults);
    }

    String page;
    page.reserve(2048);
    page += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    page += F("<title>Networks</title></head><body>");
    page += F("<h2>Networks found</h2><ul>");

    if (count == 0u) {
        page += F("<li>No networks found. Enter the name manually below.</li>");
    }

    for (uint8_t i = 0u; i < count; ++i) {
        page += F("<li><a href=\"/?ssid=");
        appendEscaped(page, ssids[i]);
        page += F("\">");
        appendEscaped(page, ssids[i]);
        page += F("</a></li>");
    }

    page += F("</ul><p><a href=\"/\">Back</a></p></body></html>");

    gServer.send(200, "text/html", page);
}

void Esp32ProvisioningPortal::sendPortalPage(
    int statusCode,
    const char* statusMessage,
    const char* pendingSsid) {
    String page;
    page.reserve(3072);

    page += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    page += F("<title>Pixel Heart Setup</title>");
    page += F("<style>body{font-family:sans-serif;margin:1.5rem;max-width:34rem}");
    page += F("input,button{width:100%;padding:.6rem;margin:.35rem 0;font-size:1rem}");
    page += F(".msg{padding:.6rem;background:#eee;margin:.75rem 0}</style>");
    page += F("</head><body><h1>Pixel Heart Setup</h1>");

    page += F("<p class=\"msg\">");
    appendEscaped(page, statusMessage != nullptr ? statusMessage : "");
    page += F("</p>");

    if (pendingConnection_) {
        page += F("<p>Testing the connection. This can take up to 20 seconds.</p>");
        // Reload so the page reflects the outcome once the test resolves.
        page += F("<script>setTimeout(function(){location.reload();},3000);</script>");
    }

    page += F("<p>Enter your home Wi-Fi details. Leave the password blank for an open network.</p>");
    page += F("<form method=\"POST\" action=\"/connect\">");
    page += F("<label>Network name (SSID)</label>");
    page += F("<input type=\"text\" name=\"ssid\" maxlength=\"32\" required value=\"");
    appendEscaped(page, pendingSsid != nullptr ? pendingSsid : "");
    page += F("\">");
    page += F("<label>Password</label>");
    // type=password: the value is never rendered back to the user.
    page += F("<input type=\"password\" name=\"password\" maxlength=\"63\" autocomplete=\"off\">");
    page += F("<button type=\"submit\">Connect</button>");
    page += F("</form>");

    page += F("<p><a href=\"/scan\">Scan networks</a></p>");
    page += F("<p>Setup address: ");
    appendEscaped(page, NetworkDefaults::kPortalAddress);
    page += F("</p></body></html>");

    gServer.send(statusCode, "text/html", page);
}

#endif  // defined(ARDUINO_ARCH_ESP32)
