#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "control/BrightnessLevels.h"
#include "control/AnimationCatalog.h"
#include "control/UserSettings.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"
#include "core/DisplayRotation.h"

// Everything the panel needs to draw itself. Plain POD filled in by
// Esp32LanService, so the renderer has no knowledge of HTTP, Wi-Fi or NVS.
struct PanelView {
    ControlMode mode;
    AnimationId currentAnimation;   // what is actually on the matrix
    uint8_t brightnessPercent;
    DisplayRotation rotation;
    bool connected;
    const char* ssid;
    const char* ipAddress;
    const char* localTime;
};

// Server-rendered control panel. Plain HTML forms, one small embedded
// stylesheet, ZERO JavaScript, no external resources of any kind.
//
// The output is produced through a one-method sink, which is what makes the
// real markup testable on the host:
//
//     struct Sink { void append(const char* text); };
//
// On the device the sink appends to the Arduino String that WebServer sends;
// in the native tests it appends to a fixed buffer. Because the renderer is a
// template there is no virtual call, no heap and no extra flash on the device
// for the sake of testability.
namespace PanelRenderer {

// The "which option is selected" decision, factored out so the highlight logic
// is explicit and can be asserted directly instead of by scraping markup.
inline bool isActiveMode(ControlMode option, ControlMode current) {
    return option == current;
}

inline bool isActiveRotation(DisplayRotation option, DisplayRotation current) {
    return option == current;
}

inline const char* modeLabel(ControlMode mode) {
    return (mode == ControlMode::MANUAL) ? "Ręczny" : "Automatyczny";
}

inline const char* connectionLabel(bool connected) {
    return connected ? "Połączono" : "Brak połączenia";
}

// Buffer the device reserves for the rendered page.
//
// One number, used by the ESP32 service to size its String and by the native
// tests to size their capture buffer, so a page that would be truncated on
// hardware fails a test instead of shipping.
//
// Sized from measurement: the panel with the full animation list renders at
// ~6.6 kB, so this leaves headroom for longer SSID/time values and for a few
// more animations. A future growth past this bound fails
// test_render_matches_the_advertised_content_length rather than silently
// truncating the page on the device.
constexpr size_t kPageCapacity = 8192u;

// The stylesheet. Kept to the minimum that produces a coherent panel:
// system font, one surface colour, one accent (the project's dark green),
// comfortable touch targets. No shadows, no images, no web fonts.
constexpr const char* kStyle =
    "*{box-sizing:border-box}"
    "body{margin:0;padding:16px;background:#f1f4f2;color:#182420;"
    "font:16px/1.45 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}"
    ".wrap{max-width:420px;margin:0 auto}"
    "header{display:flex;align-items:center;justify-content:space-between;margin:2px 0 14px}"
    "h1{font-size:1.3rem;margin:0}"
    ".net{font-size:.88rem;color:#4a5b54}"
    ".dot{display:inline-block;width:9px;height:9px;border-radius:50%;"
    "background:#c0392b;margin-right:6px;vertical-align:middle}"
    ".dot.on{background:#1e8e4e}"
    ".card,details{background:#fff;border:1px solid #e0e6e3;border-radius:12px}"
    ".card{padding:14px;margin:0 0 12px}"
    "details{padding:12px 14px}"
    "h2{font-size:.78rem;letter-spacing:.07em;text-transform:uppercase;"
    "color:#5d6f67;margin:0 0 10px;font-weight:600}"
    ".label{font-size:.8rem;color:#5d6f67;margin:0}"
    ".value{text-align:center;font-size:1.45rem;font-weight:600;margin:2px 0 12px}"
    ".value.small{font-size:1.05rem;margin-bottom:10px}"
    ".hint{font-size:.8rem;color:#7a8a83;margin:10px 0}"
    ".row{display:flex;gap:8px;flex-wrap:wrap}"
    ".row form{flex:1 1 0;min-width:0}"
    ".grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}"
    ".stack{margin-top:10px}"
    "form{margin:0}"
    "button{font:inherit;font-size:.95rem;min-height:42px;width:100%;padding:0 6px;"
    "border:1px solid #c9d3ce;border-radius:9px;background:#fff;color:#182420;"
    "cursor:pointer;overflow-wrap:break-word}"
    "button.on,button.primary{background:#003d2b;border-color:#003d2b;color:#fff;font-weight:600}"
    "button.primary{font-size:1.02rem}"
    "summary{cursor:pointer;font-weight:600}"
    "table{width:100%;border-collapse:collapse;margin-top:10px;font-size:.92rem}"
    "td{padding:3px 0;vertical-align:top}"
    "td:first-child{color:#5d6f67;padding-right:12px}"
    "td:last-child{text-align:right;overflow-wrap:anywhere}";

// Minimal escaping for values that come from the network (SSID) or the clock.
template <typename Sink>
inline void appendEscaped(Sink& out, const char* text) {
    if (text == nullptr) {
        return;
    }

    char single[2];
    single[1] = '\0';

    for (const char* p = text; *p != '\0'; ++p) {
        switch (*p) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            default:
                single[0] = *p;
                out.append(single);
                break;
        }
    }
}

// One POST button. Every control on the panel is one of these, so the markup
// stays consistent and there is exactly one place that decides how an active
// control looks.
//
// Active controls get BOTH the dark green styling and a trailing check mark,
// so the state does not depend on colour alone.
template <typename Sink>
inline void postButton(
    Sink& out,
    const char* action,
    const char* name,
    const char* value,
    const char* label,
    bool active,
    bool primary) {
    out.append("<form method=\"POST\" action=\"");
    out.append(action);
    out.append("\"><button name=\"");
    out.append(name);
    out.append("\" value=\"");
    out.append(value);
    out.append("\" class=\"");
    out.append(primary ? "btn primary" : (active ? "btn on" : "btn"));
    out.append("\"");
    if (active) {
        out.append(" aria-pressed=\"true\"");
    }
    out.append(">");
    out.append(label);
    if (active) {
        out.append(" ✓");
    }
    out.append("</button></form>");
}

template <typename Sink>
inline void rotationButton(Sink& out, DisplayRotation option, DisplayRotation current) {
    char value[8];
    char label[12];
    const unsigned degrees = static_cast<unsigned>(displayRotationDegrees(option));

    snprintf(value, sizeof(value), "%u", degrees);
    snprintf(label, sizeof(label), "%u°", degrees);

    postButton(out, "/rotation", "rotation", value, label,
               isActiveRotation(option, current), false);
}

// The whole page.
template <typename Sink>
inline void render(Sink& out, const PanelView& view) {
    char number[8];

    out.append("<!doctype html><html lang=\"pl\"><head><meta charset=\"utf-8\">");
    out.append("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
    out.append("<title>Pixel Heart</title><style>");
    out.append(kStyle);
    out.append("</style></head><body><div class=\"wrap\">");

    // --- Header: identity + connection state, nothing else ------------------
    out.append("<header><h1>Pixel Heart</h1><span class=\"net\"><i class=\"dot");
    out.append(view.connected ? " on" : "");
    out.append("\"></i>");
    out.append(connectionLabel(view.connected));
    out.append("</span></header>");

    // --- Operating mode ----------------------------------------------------
    out.append("<section class=\"card\"><h2>Tryb pracy</h2><div class=\"row\">");
    postButton(out, "/mode", "m", "auto", modeLabel(ControlMode::AUTO),
               isActiveMode(ControlMode::AUTO, view.mode), false);
    postButton(out, "/mode", "m", "manual", modeLabel(ControlMode::MANUAL),
               isActiveMode(ControlMode::MANUAL, view.mode), false);
    out.append("</div></section>");

    // --- Animation ---------------------------------------------------------
    // One click on a name is the whole intent: switch to manual and start it.
    // There is no separate selection-then-play step, so there is no stepper and
    // no play button here.
    out.append("<section class=\"card\"><h2>Animacja</h2>");
    out.append("<p class=\"label\">Teraz wyświetlane</p>");
    out.append("<p class=\"value\">");
    out.append(animationNameOf(view.currentAnimation));
    out.append("</p>");

    // The list is derived from the registry through AnimationCatalog, so it can
    // never drift from the animations that actually exist, and ids the panel
    // must not offer (NONE, the diagnostic TEST pattern, and anything with no
    // implementation) are filtered out automatically.
    out.append("<p class=\"label\">Wybierz animację</p><div class=\"grid\">");

    const uint8_t idCount = static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT);
    for (uint8_t raw = 1u; raw < idCount; ++raw) {
        const AnimationId candidate = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(candidate)) {
            continue;
        }

        char value[4];
        snprintf(value, sizeof(value), "%u", static_cast<unsigned>(raw));

        // Highlighted on what is ACTUALLY on the panel, not on a stored
        // preference, so this always agrees with "Teraz wyświetlane" above.
        postButton(out, "/animation", "a", value, animationNameOf(candidate),
                   candidate == view.currentAnimation, false);
    }

    out.append("</div>");

    // Switching to MANUAL is real behaviour the user should be told about.
    out.append("<p class=\"hint\">Wybór animacji włącza tryb ręczny.</p>");
    out.append("</section>");

    // --- Brightness --------------------------------------------------------
    // Five levels, not a percentage: the safe range of this device is a small
    // fraction of full scale, so "13%" would be misleading. Direct selection -
    // one press reaches the level, there is nothing to step up or down.
    const uint8_t brightnessLevel = BrightnessLevels::levelForPercent(view.brightnessPercent);
    out.append("<section class=\"card\"><h2>Jasność</h2><p class=\"label\">Poziom ");
    snprintf(number, sizeof(number), "%u", static_cast<unsigned>(brightnessLevel));
    out.append(number);
    out.append(" — ");
    out.append(BrightnessLevels::labelForLevel(brightnessLevel));
    out.append("</p><div class=\"row\">");

    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        char value[4];
        snprintf(value, sizeof(value), "%u", static_cast<unsigned>(level));
        postButton(out, "/brightness", "level", value, value,
                   level == brightnessLevel, false);
    }

    out.append("</div></section>");

    // --- Orientation -------------------------------------------------------
    // Four direct choices instead of a dropdown plus a submit button. There is
    // deliberately no play/restart control anywhere in this card.
    out.append("<section class=\"card\"><h2>Orientacja</h2><div class=\"row\">");
    rotationButton(out, DisplayRotation::DEG_0, view.rotation);
    rotationButton(out, DisplayRotation::DEG_90, view.rotation);
    rotationButton(out, DisplayRotation::DEG_180, view.rotation);
    rotationButton(out, DisplayRotation::DEG_270, view.rotation);
    out.append("</div></section>");

    // --- Device information ------------------------------------------------
    // Secondary detail, collapsed by default. Native <details>, no JavaScript.
    out.append("<details><summary>Informacje o urządzeniu</summary><table>");
    out.append("<tr><td>Sieć Wi-Fi</td><td>");
    appendEscaped(out, view.ssid);
    out.append("</td></tr><tr><td>Adres IP</td><td>");
    appendEscaped(out, view.ipAddress);
    out.append("</td></tr><tr><td>Czas</td><td>");
    appendEscaped(out, view.localTime);
    out.append("</td></tr></table>");
    out.append("<div class=\"stack\">");
    postButton(out, "/wifi/change", "w", "1", "Zmień sieć Wi-Fi", false, false);
    out.append("</div></details>");

    out.append("</div></body></html>");
}

}  // namespace PanelRenderer
