#include "network/Esp32LanService.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <ESPmDNS.h>
#include <WebServer.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "network/HttpPortOwnership.h"
#include "network/NetworkDefaults.h"
#include "control/BrightnessLevels.h"
#include "control/PanelAuth.h"

namespace {
WebServer gLanServer(80);
Esp32LanService* gLanService = nullptr;

void copyBounded(char* out, uint8_t capacity, const char* value) {
    if (out == nullptr || capacity == 0u) {
        return;
    }

    if (value == nullptr) {
        out[0] = '\0';
        return;
    }

    strncpy(out, value, static_cast<size_t>(capacity) - 1u);
    out[static_cast<size_t>(capacity) - 1u] = '\0';
}

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

Esp32LanService::Esp32LanService()
    : routesRegistered_(false),
      started_(false),
      address_(0u),
      tickCount_(0u),
      handleClientCount_(0u),
      healthRequestCount_(0u),
      statusRequestCount_(0u),
      lastHeartbeatMs_(0u),
      wifiChangeRequested_(false),
      wifiChangeRequests_(0u),
      pendingControl_(makeIdleControlRequest()),
      mode_(ControlMode::AUTO),
      brightnessPercent_(0u),
      rotation_(DisplayRotation::DEG_0),
      currentAnimation_(AnimationId::NONE),
      connected_(false) {
    ssid_[0] = '\0';
    time_[0] = '\0';
}

void Esp32LanService::begin() {
    if (started_) {
        return;
    }

    // Claim the shared HTTP port before touching the server. If the
    // provisioning portal still owns it, do nothing: App will retry, and this
    // prevents two WebServer(80) instances from existing at once.
    if (!HttpPortOwnership::acquire(HttpPortOwnership::Owner::LAN)) {
        return;
    }

    gLanService = this;

    Serial.println(F("[HTTP] LAN begin requested"));

    // A ':' in the username or an empty value would make Basic Auth
    // impossible or ambiguous. Reported once, never with the values.
    if (!PanelAuth::credentialsConfigured()) {
        Serial.println(F("[HTTP] WARNING: panel credentials are not usable"));
    }

    // One-time server setup. WebServer::on() APPENDS to a handler list that
    // stop()/close() never clears, so re-registering on every begin() would
    // duplicate every route and leak a handler object each time. With the
    // "Change Wi-Fi" flow the LAN server is legitimately stopped and later
    // restarted, so this guard is required, not merely an optimisation.
    if (!routesRegistered_) {
        // mDNS is best-effort: a failure here must not affect the display or
        // the scheduler, so the return value is not treated as fatal.
        if (MDNS.begin(NetworkDefaults::kMdnsHostname)) {
            MDNS.addService("http", "tcp", HttpPortOwnership::kHttpPort);
        }

        // Register every route FIRST. WebServer::begin() internally calls
        // close(), so calling begin() before the handlers are attached would
        // leave the server listening with an empty handler list.
        gLanServer.on("/", HTTP_GET, [this]() { sendStatusPage(); });

        // Minimal liveness route. Deliberately touches nothing: no NTP, no
        // scheduler, no HTML. If this responds, the WebServer lifecycle and
        // handleClient() are sound.
        gLanServer.on("/health", HTTP_GET, [this]() { sendHealth(); });

        // Change-Wi-Fi entry point. POST only: this mutates device state and
        // must not be triggerable by a link prefetch or a crawler.
        gLanServer.on("/wifi/change", HTTP_POST, [this]() { handleWifiChange(); });

        // Control surface. Every mutating route is POST, so a GET can never
        // change state and a prefetching browser cannot disturb playback.
        gLanServer.on("/mode", HTTP_POST, [this]() { handleMode(); });
        gLanServer.on("/animation", HTTP_POST, [this]() { handleAnimation(); });
        gLanServer.on("/brightness", HTTP_POST, [this]() { handleBrightness(); });
        gLanServer.on("/rotation", HTTP_POST, [this]() { handleRotation(); });
        gLanServer.on("/play", HTTP_POST, [this]() { handlePlay(); });

        gLanServer.onNotFound([this]() { sendNotFoundPage(); });

        routesRegistered_ = true;
    }

    gLanServer.begin();

    // Only mark started once the listener is genuinely up.
    started_ = true;
    Serial.println(F("[HTTP] LAN routes registered"));
    Serial.println(F("[HTTP] LAN server started"));
}

void Esp32LanService::sendHealth() {
    ++healthRequestCount_;
    Serial.println(F("[HTTP] GET /health"));
    gLanServer.send(200, "text/plain", "OK");
    Serial.println(F("[HTTP] /health sent"));
}

void Esp32LanService::handleWifiChange() {
    if (!authorize()) {
        return;
    }

    ++wifiChangeRequests_;

    // The request is only RECORDED here. This handler runs inside
    // gLanServer.handleClient(), so tearing the server down from inside it
    // would pull the listener out from under the in-flight response. The owner
    // consumes the flag on a later tick and sequences:
    //   finish response -> stop LAN server -> release port 80 -> start portal.
    wifiChangeRequested_ = true;

    Serial.println(F("[HTTP] POST /wifi/change"));

    String page;
    page.reserve(1024);
    page += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    page += F("<title>Pixel Heart</title>");
    page += F("<style>body{font-family:sans-serif;margin:1.5rem;max-width:26rem}");
    page += F("</style></head><body>");
    page += F("<h1>Pixel Heart</h1>");
    page += F("<p>Uruchamianie konfiguracji Wi-Fi...</p>");
    page += F("<p>Połącz się z siecią <b>PixelHeart-Setup-XXXX</b> "
              "i wpisz dane nowej sieci Wi-Fi.</p>");
    page += F("<p>Ta strona nie jest już dostępna.</p>");
    page += F("</body></html>");

    gLanServer.send(200, "text/html", page);
    Serial.println(F("[HTTP] /wifi/change response sent"));
}

bool Esp32LanService::consumeWifiChangeRequest() {
    if (!wifiChangeRequested_) {
        return false;
    }

    wifiChangeRequested_ = false;
    return true;
}

uint16_t Esp32LanService::wifiChangeRequests() const {
    return wifiChangeRequests_;
}

void Esp32LanService::queueControl(ControlAction action) {
    pendingControl_ = makeIdleControlRequest();
    pendingControl_.action = action;
}

namespace {
// Method name for the platform-neutral access policy. Used so the decision
// goes through PanelAuth rather than being re-implemented per handler.
const char* methodToken(HTTPMethod method) {
    switch (method) {
        case HTTP_GET:
            return "GET";
        case HTTP_HEAD:
            return "HEAD";
        case HTTP_OPTIONS:
            return "OPTIONS";
        case HTTP_POST:
            return "POST";
        case HTTP_PUT:
            return "PUT";
        case HTTP_PATCH:
            return "PATCH";
        case HTTP_DELETE:
            return "DELETE";
        default:
            return nullptr;  // unknown -> PanelAuth fails closed
    }
}
}  // namespace

bool Esp32LanService::authorize() {
    const char* token = methodToken(gLanServer.method());

    // Authenticate BEFORE the caller does anything: an unauthenticated request
    // must not reach the state change, even partially. Read-only methods skip
    // the credential check but are handled by the same gate below.
    bool authenticated = false;
    if (PanelAuth::requiresAuthForMethodToken(token)) {
        authenticated = gLanServer.authenticate(PanelAuth::kUsername, PanelAuth::kPassword);
    }

    if (PanelAuth::allowRequest(token, authenticated)) {
        return true;
    }

    // Still a genuine 401 with WWW-Authenticate, so Basic Auth and curl behave
    // exactly as before. The extra argument is only the body a browser shows
    // once the user dismisses the dialog: a tiny page that returns to the
    // panel, instead of a bare "Unauthorized" that strands them there. The
    // supplied credentials are never logged.
    Serial.println(F("[HTTP] 401 unauthorized"));
    gLanServer.requestAuthentication(
        BASIC_AUTH,
        PanelAuth::kRealm,
        String(PanelAuth::kAuthFailureBody));
    return false;
}

ControlRequest Esp32LanService::consumeControlRequest() {
    const ControlRequest request = pendingControl_;
    pendingControl_ = makeIdleControlRequest();
    return request;
}

void Esp32LanService::redirectToPanel() {
    // A 303 back to the panel makes the browser re-GET the applied state.
    // This is the whole reason no JavaScript is needed here.
    gLanServer.sendHeader(F("Location"), F("/"));
    gLanServer.send(303, F("text/plain"), F(""));
}

void Esp32LanService::handleMode() {
    if (!authorize()) {
        return;
    }

    const bool manual = gLanServer.arg(F("m")) == F("manual");
    queueControl(ControlAction::SET_MODE);
    pendingControl_.mode = manual ? ControlMode::MANUAL : ControlMode::AUTO;
    Serial.println(manual ? F("[HTTP] POST /mode manual") : F("[HTTP] POST /mode auto"));
    redirectToPanel();
}

void Esp32LanService::handleAnimation() {
    if (!authorize()) {
        return;
    }

    // One click on a list entry IS the whole user intent: switch to manual and
    // start this animation. A single authenticated request, so there is no
    // select-then-play pair that could arrive out of order.
    //
    // The raw integer is range-checked BEFORE narrowing to AnimationId.
    // AnimationId is a uint8_t enum, so casting a hand-crafted value such as
    // 9999 straight to it would silently wrap onto a different animation.
    const int requested = gLanServer.arg(F("a")).toInt();
    if (requested > 0 && requested < static_cast<int>(AnimationId::ANIMATION_ID_COUNT)) {
        const AnimationId candidate = static_cast<AnimationId>(requested);
        if (AnimationCatalog::isUserSelectable(candidate)) {
            queueControl(ControlAction::SELECT_ANIMATION);
            pendingControl_.animation = candidate;
        }
    }

    Serial.println(F("[HTTP] POST /animation"));
    redirectToPanel();
}

void Esp32LanService::handleBrightness() {
    if (!authorize()) {
        return;
    }

    // The panel sends a 1-based level. The level -> percent mapping lives in
    // BrightnessLevels, so the endpoint itself stays dumb. An out-of-range
    // level is ignored rather than snapped to the brightest stepping stone.
    const uint8_t level = static_cast<uint8_t>(gLanServer.arg(F("level")).toInt());
    if (BrightnessLevels::isValidLevel(level)) {
        queueControl(ControlAction::SET_BRIGHTNESS);
        pendingControl_.brightnessPercent = BrightnessLevels::percentForLevel(level);
    }

    Serial.println(F("[HTTP] POST /brightness"));
    redirectToPanel();
}

void Esp32LanService::handleRotation() {
    if (!authorize()) {
        return;
    }

    const uint16_t degrees = static_cast<uint16_t>(gLanServer.arg(F("rotation")).toInt());
    queueControl(ControlAction::SET_ROTATION);
    pendingControl_.rotation = displayRotationFromDegrees(degrees);
    Serial.println(F("[HTTP] POST /rotation"));
    redirectToPanel();
}

void Esp32LanService::handlePlay() {
    if (!authorize()) {
        return;
    }

    queueControl(ControlAction::PLAY);
    Serial.println(F("[HTTP] POST /play"));
    redirectToPanel();
}

void Esp32LanService::setStatusMode(ControlMode mode) {
    mode_ = mode;
}

void Esp32LanService::setStatusBrightness(uint8_t percent) {
    brightnessPercent_ = percent;
}

void Esp32LanService::setStatusRotation(DisplayRotation rotation) {
    rotation_ = rotation;
}

void Esp32LanService::stop() {
    if (!started_) {
        return;
    }

    Serial.println(F("[HTTP] LAN server stopping"));

    gLanServer.stop();
    gLanServer.close();

    started_ = false;
    gLanService = nullptr;

    HttpPortOwnership::release(HttpPortOwnership::Owner::LAN);
    Serial.println(F("[HTTP] LAN server stopped"));
}

bool Esp32LanService::started() const {
    return started_;
}

void Esp32LanService::tick() {
    if (!started_) {
        return;
    }

    ++tickCount_;
    ++handleClientCount_;

    // Instrumented on the one and only active WebServer instance.
    gLanServer.handleClient();

    // Rate-limited heartbeat: proves the loop is alive without flooding the
    // serial port, which could itself perturb timing.
    const uint32_t now = millis();
    if (now - lastHeartbeatMs_ >= 2000u || lastHeartbeatMs_ == 0u) {
        lastHeartbeatMs_ = now;
        Serial.printf(
            "[HTTP] LAN tick alive count=%lu handleClient=%lu freeHeap=%lu\n",
            static_cast<unsigned long>(tickCount_),
            static_cast<unsigned long>(handleClientCount_),
            static_cast<unsigned long>(ESP.getFreeHeap()));
    }
}

void Esp32LanService::setStatusSsid(const char* ssid) {
    copyBounded(ssid_, sizeof(ssid_), ssid);
}

void Esp32LanService::setStatusAddress(uint32_t address) {
    address_ = address;
}

void Esp32LanService::setStatusTime(const char* formattedTime) {
    copyBounded(time_, sizeof(time_), formattedTime);
}

void Esp32LanService::setStatusAnimation(AnimationId animation) {
    currentAnimation_ = animation;
}

void Esp32LanService::setStatusConnected(bool connected) {
    connected_ = connected;
}

bool Esp32LanService::routesRegistered() const {
    return routesRegistered_;
}

uint32_t Esp32LanService::tickCount() const {
    return tickCount_;
}

uint32_t Esp32LanService::handleClientCount() const {
    return handleClientCount_;
}



void Esp32LanService::sendStatusPage() {
    ++statusRequestCount_;
    Serial.println(F("[HTTP] GET /"));
    char addressText[20];
    if (address_ == 0u) {
        strncpy(addressText, "not available", sizeof(addressText) - 1u);
        addressText[sizeof(addressText) - 1u] = '\0';
    } else {
        snprintf(
            addressText,
            sizeof(addressText),
            "%u.%u.%u.%u",
            static_cast<unsigned>((address_ >> 24) & 0xFFu),
            static_cast<unsigned>((address_ >> 16) & 0xFFu),
            static_cast<unsigned>((address_ >> 8) & 0xFFu),
            static_cast<unsigned>(address_ & 0xFFu));
    }

    // One-method sink so the renderer stays free of String and Arduino types.
    // This is what makes the real markup testable on the host.
    struct StringSink {
        String& target;
        void append(const char* text) { target += text; }
    };

    PanelView view;
    view.mode = mode_;
    view.currentAnimation = currentAnimation_;
    view.brightnessPercent = brightnessPercent_;
    view.rotation = rotation_;
    view.connected = connected_;
    view.ssid = ssid_;
    view.ipAddress = addressText;
    view.localTime = time_;

    String page;
    page.reserve(PanelRenderer::kPageCapacity);
    StringSink sink{page};
    PanelRenderer::render(sink, view);

    gLanServer.send(200, "text/html", page);
    Serial.println(F("[HTTP] / sent"));
}

void Esp32LanService::sendNotFoundPage() {
    // A controlled 404 rather than a dropped connection, so an unknown route
    // behaves predictably for a browser or curl.
    String page;
    page.reserve(512);
    page += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
    page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    page += F("<title>Pixel Heart - Not Found</title></head><body>");
    page += F("<h1>404 - Not Found</h1>");
    page += F("<p>The requested page does not exist on this device.</p>");
    page += F("<p><a href=\"/\">Status page</a></p>");
    page += F("</body></html>");

    gLanServer.send(404, "text/html", page);
    Serial.println(F("[HTTP] 404 for unknown route"));
}

#endif  // defined(ARDUINO_ARCH_ESP32)
