#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "control/BrightnessLevels.h"
#include "control/PanelAuth.h"
#include "control/PanelRenderer.h"
#include "core/AnimationId.h"
#include "core/AnimationNames.h"

namespace {

// The native sink: appends into a fixed buffer. This is the same renderer the
// device uses, reached through the same one-method interface, so these tests
// exercise the shipping markup rather than a copy of it.
struct BufferSink {
    char* buffer;
    size_t capacity;
    size_t used;

    void append(const char* text) {
        while (*text != '\0' && used + 1u < capacity) {
            buffer[used++] = *text++;
        }
        buffer[used] = '\0';
    }
};

// Two independent static buffers so a test can render one page, change the
// view, render again and compare. No heap, matching the firmware's rules.
// Sized to match the String the device reserves for the page, so these
// tests also prove the markup fits without being truncated on hardware.
constexpr size_t kPageCapacity = PanelRenderer::kPageCapacity;
char g_html[kPageCapacity];
char g_htmlA[kPageCapacity];
char g_htmlB[kPageCapacity];
size_t g_used = 0u;

const char* renderInto(char* buffer, size_t capacity, const PanelView& view) {
    BufferSink sink{buffer, capacity, 0u};
    PanelRenderer::render(sink, view);
    return buffer;
}

const char* renderPanel(const PanelView& view) {
    BufferSink sink{g_html, sizeof(g_html), 0u};
    PanelRenderer::render(sink, view);
    g_used = sink.used;
    return g_html;
}

PanelView makeView() {
    PanelView view;
    view.mode = ControlMode::AUTO;
    view.currentAnimation = AnimationId::AURORA_NEON;
    view.brightnessPercent = 18u;
    view.rotation = DisplayRotation::DEG_0;
    view.connected = true;
    view.ssid = "MikroTik-CB390D";
    view.ipAddress = "192.168.88.31";
    view.localTime = "16:46:35";
    return view;
}

bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != nullptr;
}

// Position of a marker in the rendered page, for order assertions.
int positionOf(const char* html, const char* needle) {
    const char* found = strstr(html, needle);
    return (found == nullptr) ? -1 : static_cast<int>(found - html);
}

uint8_t countOf(const char* html, const char* needle) {
    uint8_t count = 0u;
    const char* cursor = html;
    const size_t length = strlen(needle);

    while ((cursor = strstr(cursor, needle)) != nullptr) {
        ++count;
        cursor += length;
    }

    return count;
}

// Copies the card starting at `heading` into a static buffer, so a test can
// assert on one section without another section's controls interfering.
// Sized for the animation card, which is the largest: 22 selectable ids at
// ~105 bytes of button markup each.
const char* cardAt(const char* html, const char* heading, const char* nextHeading) {
    static char card[4096];

    const int start = positionOf(html, heading);
    if (start < 0) {
        card[0] = '\0';
        return card;
    }

    int end = positionOf(html, nextHeading);
    if (end < 0) {
        end = static_cast<int>(strlen(html));
    }

    size_t length = static_cast<size_t>(end - start);
    if (length >= sizeof(card)) {
        length = sizeof(card) - 1u;
    }
    memcpy(card, html + start, length);
    card[length] = '\0';
    return card;
}

// The page body only, skipping the embedded stylesheet. Percentages legitimately
// appear in CSS (width:100%), so "no percentage is shown to the user" has to be
// asserted against the content, not the style block.
const char* bodyOf(const char* html) {
    const char* body = strstr(html, "</style>");
    return (body == nullptr) ? html : body;
}

}  // namespace

// ---------------------------------------------------------------------------
// Stage 1: Polish terminology
// ---------------------------------------------------------------------------

void test_panel_uses_polish_section_headings(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "Tryb pracy"));
    TEST_ASSERT_TRUE(contains(html, "Animacja"));
    TEST_ASSERT_TRUE(contains(html, "Jasność"));
    TEST_ASSERT_TRUE(contains(html, "Orientacja"));
    TEST_ASSERT_TRUE(contains(html, "Informacje o urządzeniu"));
}

void test_panel_uses_polish_control_labels(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "Automatyczny"));
    TEST_ASSERT_TRUE(contains(html, "Ręczny"));
    TEST_ASSERT_TRUE(contains(html, "Teraz wyświetlane"));
    TEST_ASSERT_TRUE(contains(html, "Wybierz animację"));
    TEST_ASSERT_TRUE(contains(html, "Zmień sieć Wi-Fi"));
    TEST_ASSERT_TRUE(contains(html, "Sieć Wi-Fi"));
    TEST_ASSERT_TRUE(contains(html, "Adres IP"));
    TEST_ASSERT_TRUE(contains(html, "Czas"));

    // The old English/debug labels must be gone.
    TEST_ASSERT_FALSE(contains(html, ">PLAY<"));
    TEST_ASSERT_FALSE(contains(html, "Mode:"));
    TEST_ASSERT_FALSE(contains(html, "Brightness:"));
    TEST_ASSERT_FALSE(contains(html, "IP address"));
}

void test_panel_has_no_javascript_and_no_external_resources(void) {
    const char* html = renderPanel(makeView());

    // Zero JavaScript.
    TEST_ASSERT_FALSE(contains(html, "<script"));
    TEST_ASSERT_FALSE(contains(html, "javascript:"));
    TEST_ASSERT_FALSE(contains(html, "onclick"));
    TEST_ASSERT_FALSE(contains(html, "onchange"));

    // No external CSS, fonts, or images of any kind.
    TEST_ASSERT_FALSE(contains(html, "<link"));
    TEST_ASSERT_FALSE(contains(html, "@import"));
    TEST_ASSERT_FALSE(contains(html, "http://"));
    TEST_ASSERT_FALSE(contains(html, "https://"));
    TEST_ASSERT_FALSE(contains(html, "<img"));
    TEST_ASSERT_FALSE(contains(html, "url("));
}

// ---------------------------------------------------------------------------
// Stage 4: header / connection status
// ---------------------------------------------------------------------------

void test_header_shows_connection_state(void) {
    PanelView view = makeView();

    view.connected = true;
    TEST_ASSERT_TRUE(contains(renderPanel(view), "Połączono"));
    TEST_ASSERT_TRUE(contains(renderPanel(view), "class=\"dot on\""));

    view.connected = false;
    TEST_ASSERT_TRUE(contains(renderPanel(view), "Brak połączenia"));
    TEST_ASSERT_TRUE(contains(renderPanel(view), "class=\"dot\""));
}

// ---------------------------------------------------------------------------
// Stage 5: operating mode
// ---------------------------------------------------------------------------

void test_mode_highlight_matches_the_active_mode(void) {
    // The highlight decision itself, assertable without scraping markup.
    TEST_ASSERT_TRUE(PanelRenderer::isActiveMode(ControlMode::AUTO, ControlMode::AUTO));
    TEST_ASSERT_FALSE(PanelRenderer::isActiveMode(ControlMode::MANUAL, ControlMode::AUTO));
    TEST_ASSERT_TRUE(PanelRenderer::isActiveMode(ControlMode::MANUAL, ControlMode::MANUAL));
    TEST_ASSERT_EQUAL_STRING("Automatyczny", PanelRenderer::modeLabel(ControlMode::AUTO));
    TEST_ASSERT_EQUAL_STRING("Ręczny", PanelRenderer::modeLabel(ControlMode::MANUAL));

    // And the rendered page really does change with the mode.
    PanelView view = makeView();

    view.mode = ControlMode::AUTO;
    renderInto(g_htmlA, sizeof(g_htmlA), view);

    view.mode = ControlMode::MANUAL;
    renderInto(g_htmlB, sizeof(g_htmlB), view);

    TEST_ASSERT_TRUE(strcmp(g_htmlA, g_htmlB) != 0);
}

// ---------------------------------------------------------------------------
// Stage 6: the animation card, and PLAY belonging to it
// ---------------------------------------------------------------------------

void test_animation_choices_are_inside_the_animation_card(void) {
    const char* html = renderPanel(makeView());

    const int animation = positionOf(html, "Animacja");
    const int choices = positionOf(html, "Wybierz animację");
    const int brightness = positionOf(html, "Jasność");
    const int orientation = positionOf(html, "Orientacja");

    TEST_ASSERT_TRUE(animation >= 0);
    TEST_ASSERT_TRUE(choices >= 0);
    TEST_ASSERT_TRUE(brightness >= 0);
    TEST_ASSERT_TRUE(orientation >= 0);

    // The list belongs to its own card and cannot be read as part of another.
    TEST_ASSERT_TRUE(choices > animation);
    TEST_ASSERT_TRUE(choices < brightness);
    TEST_ASSERT_TRUE(choices < orientation);
}

void test_previous_next_and_play_are_gone_from_the_panel(void) {
    // One click on a name IS the play action now, so the stepper and the
    // separate play button must not exist anywhere in the rendered page.
    const char* html = renderPanel(makeView());

    TEST_ASSERT_FALSE(contains(html, "Poprzednia"));
    TEST_ASSERT_FALSE(contains(html, "Następna"));
    TEST_ASSERT_FALSE(contains(html, "Uruchom"));

    // Nor their backend hooks.
    TEST_ASSERT_FALSE(contains(html, "name=\"d\""));
    TEST_ASSERT_FALSE(contains(html, "action=\"/play\""));
}

void test_every_user_selectable_animation_is_offered(void) {
    const char* html = renderPanel(makeView());

    uint8_t expected = 0u;
    const uint8_t idCount = static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT);

    for (uint8_t raw = 1u; raw < idCount; ++raw) {
        const AnimationId candidate = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(candidate)) {
            continue;
        }
        ++expected;

        // Present, with its human label, posting its own numeric id.
        TEST_ASSERT_TRUE(contains(html, animationNameOf(candidate)));

        char value[16];
        snprintf(value, sizeof(value), "value=\"%u\"", static_cast<unsigned>(raw));
        TEST_ASSERT_TRUE(contains(html, value));
    }

    // The list is not empty and matches the catalog exactly (no second list).
    TEST_ASSERT_TRUE(expected > 0u);
    TEST_ASSERT_EQUAL_UINT8(expected, countOf(html, "action=\"/animation\""));
}

void test_new_animations_are_each_offered_exactly_once(void) {
    const char* html = renderPanel(makeView());

    // The names appear only on their own animation buttons, so counting them in
    // the whole page proves there is exactly one entry each.
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Duszek"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Lisek"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Rudy kot"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Palma"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Meduza"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Latarnia morska"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Kula disco"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Kwiat na łące"));
    TEST_ASSERT_EQUAL_UINT8(1u, countOf(html, "Błękitne niebo"));

    // And they post their own appended numeric ids.
    TEST_ASSERT_TRUE(contains(html, "value=\"19\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"20\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"21\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"22\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"23\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"24\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"25\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"26\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"27\""));
}

void test_non_user_facing_animations_are_not_offered(void) {
    const char* html = renderPanel(makeView());
    // Scoped to the animation card: value="0" legitimately exists elsewhere
    // (the 0° rotation choice).
    const char* card = cardAt(html, "Animacja", "Jasność");

    // NONE means "nothing playing" and must never be a choice.
    TEST_ASSERT_FALSE(contains(card, "value=\"0\""));

    // Declared and scheduler-referenced, but with no implementation: offering
    // them would blank the panel, so the registry filter must hide them.
    TEST_ASSERT_FALSE(AnimationCatalog::isUserSelectable(AnimationId::BREATHING_HEART));
    TEST_ASSERT_FALSE(AnimationCatalog::isUserSelectable(AnimationId::LOVE_MODE));
    TEST_ASSERT_FALSE(contains(html, animationNameOf(AnimationId::BREATHING_HEART)));
    TEST_ASSERT_FALSE(contains(html, animationNameOf(AnimationId::LOVE_MODE)));
}

void test_exactly_the_current_animation_is_highlighted(void) {
    PanelView view = makeView();
    view.currentAnimation = AnimationId::CAMPFIRE;

    const char* html = renderPanel(view);

    // Exactly one highlighted choice inside the animation card. Scoped, because
    // mode, brightness and rotation each carry their own active control.
    TEST_ASSERT_EQUAL_UINT8(
        1u, countOf(cardAt(html, "Animacja", "Jasność"), "aria-pressed=\"true\""));

    TEST_ASSERT_TRUE(contains(html, "Ognisko ✓"));
    TEST_ASSERT_TRUE(contains(html, "Teraz wyświetlane"));

    // A different running animation moves the highlight.
    view.currentAnimation = AnimationId::SWIMMING_FISH;
    const char* other = renderPanel(view);
    TEST_ASSERT_TRUE(contains(other, "Pływająca rybka ✓"));
}

void test_teraz_wyswietlane_agrees_with_the_highlight(void) {
    // The heading and the highlighted row must never disagree. Both are fed
    // from the same value, so this pins that invariant for every selectable id.
    PanelView view = makeView();
    const uint8_t idCount = static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT);

    for (uint8_t raw = 1u; raw < idCount; ++raw) {
        const AnimationId candidate = static_cast<AnimationId>(raw);
        if (!AnimationCatalog::isUserSelectable(candidate)) {
            continue;
        }

        view.currentAnimation = candidate;
        const char* html = renderPanel(view);

        char marked[64];
        snprintf(marked, sizeof(marked), "%s ✓", animationNameOf(candidate));
        TEST_ASSERT_TRUE(contains(html, marked));

        // And "Teraz wyświetlane" names the same animation.
        const int heading = positionOf(html, "Teraz wyświetlane");
        TEST_ASSERT_TRUE(heading >= 0);
        TEST_ASSERT_NOT_NULL(strstr(html + heading, animationNameOf(candidate)));
    }
}

void test_nothing_is_highlighted_when_nothing_is_playing(void) {
    // Quiet hours / no valid clock: NONE is genuinely on the panel, so no
    // animation choice may claim to be the current one.
    PanelView view = makeView();
    view.currentAnimation = AnimationId::NONE;

    const char* html = renderPanel(view);

    TEST_ASSERT_TRUE(contains(html, "Wyłączone"));
    // Mode, brightness level and rotation still carry their own selections;
    // the animation list does not, because nothing is playing.
    TEST_ASSERT_EQUAL_UINT8(3u, countOf(html, "aria-pressed=\"true\""));
    TEST_ASSERT_EQUAL_UINT8(
        0u, countOf(cardAt(html, "Animacja", "Jasność"), "aria-pressed=\"true\""));
}

void test_each_choice_is_one_atomic_post_of_its_own_id(void) {
    // No select-then-play pair, and no JavaScript: every row is a single POST
    // carrying the numeric AnimationId.
    const char* html = renderPanel(makeView());

    TEST_ASSERT_FALSE(contains(html, "<script"));
    TEST_ASSERT_FALSE(contains(html, "<select"));

    char posted[32];
    snprintf(posted, sizeof(posted), "<form method=\"POST\" action=\"/animation\"><button name=\"a\" value=\"%u\"",
             static_cast<unsigned>(AnimationId::CAMPFIRE));
    TEST_ASSERT_TRUE(contains(html, posted));
}

void test_animation_card_explains_that_selecting_switches_mode(void) {
    // Selecting switches to MANUAL, which is real behaviour the user must be
    // told about.
    const char* html = renderPanel(makeView());
    TEST_ASSERT_TRUE(contains(html, "tryb ręczny"));
}

// ---------------------------------------------------------------------------
// Stage 7: brightness
// ---------------------------------------------------------------------------

void test_brightness_is_five_direct_levels(void) {
    PanelView view = makeView();
    const char* html = renderPanel(view);

    TEST_ASSERT_TRUE(contains(html, "/brightness"));
    TEST_ASSERT_TRUE(contains(html, "name=\"level\""));

    // Exactly five level values, 1..5, each a direct selection.
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        char expected[16];
        snprintf(expected, sizeof(expected), "value=\"%u\"", static_cast<unsigned>(level));
        TEST_ASSERT_TRUE(contains(html, expected));
    }
}

void test_old_step_controls_are_gone(void) {
    // The old brightness stepper used name="d" with -10/-1/+1/+10 values.
    // Those value strings are still used by the ANIMATION stepper, so the check
    // has to be scoped to the brightness card rather than the whole page.
    const char* html = renderPanel(makeView());
    const char* card = cardAt(html, "Jasność", "Orientacja");

    TEST_ASSERT_TRUE(strlen(card) > 0u);
    TEST_ASSERT_FALSE(contains(card, "name=\"d\""));
    TEST_ASSERT_FALSE(contains(card, "value=\"-10\""));
    TEST_ASSERT_FALSE(contains(card, "value=\"+10\""));
    TEST_ASSERT_FALSE(contains(card, "+10%"));
    TEST_ASSERT_FALSE(contains(card, "‒10%"));
    TEST_ASSERT_FALSE(contains(card, "Jasność matrycy"));

    // The old animation stepper also used name="d" and is gone from the whole
    // page, so nothing uses that payload key any more.
    TEST_ASSERT_FALSE(contains(html, "name=\"d\""));
}

void test_panel_shows_no_brightness_percentage(void) {
    // Any percentage in the visible content would be misleading, because the
    // safe range is only a small fraction of full scale.
    PanelView view = makeView();

    const uint8_t percents[] = {0u, 3u, 12u, 18u, 30u, 50u, 100u};
    for (uint8_t i = 0u; i < sizeof(percents) / sizeof(percents[0]); ++i) {
        view.brightnessPercent = percents[i];
        const char* body = bodyOf(renderPanel(view));

        for (uint8_t p = 0u; p <= 100u; ++p) {
            char needle[8];
            snprintf(needle, sizeof(needle), "%u%%", static_cast<unsigned>(p));
            TEST_ASSERT_FALSE(contains(body, needle));
        }
    }
}

void test_panel_shows_the_current_level_label(void) {
    PanelView view = makeView();

    // Each canonical level renders its own Polish label and highlights itself.
    for (uint8_t level = 1u; level <= BrightnessLevels::kCount; ++level) {
        view.brightnessPercent = BrightnessLevels::percentForLevel(level);
        const char* html = renderPanel(view);

        TEST_ASSERT_TRUE(contains(html, BrightnessLevels::labelForLevel(level)));
        TEST_ASSERT_TRUE(contains(html, "Poziom "));
    }
}

// ---------------------------------------------------------------------------
// Stage 8: orientation
// ---------------------------------------------------------------------------

void test_orientation_offers_four_direct_choices(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "value=\"0\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"90\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"180\""));
    TEST_ASSERT_TRUE(contains(html, "value=\"270\""));
    TEST_ASSERT_TRUE(contains(html, "/rotation"));

    // No dropdown, no separate submit step.
    TEST_ASSERT_FALSE(contains(html, "<select"));
    TEST_ASSERT_FALSE(contains(html, "Ustaw"));
}

void test_active_rotation_is_represented(void) {
    TEST_ASSERT_TRUE(PanelRenderer::isActiveRotation(DisplayRotation::DEG_0, DisplayRotation::DEG_0));
    TEST_ASSERT_FALSE(PanelRenderer::isActiveRotation(DisplayRotation::DEG_90, DisplayRotation::DEG_0));

    PanelView view = makeView();

    // The rendered page must genuinely depend on the rotation.
    view.rotation = DisplayRotation::DEG_0;
    renderInto(g_htmlA, sizeof(g_htmlA), view);

    view.rotation = DisplayRotation::DEG_270;
    renderInto(g_htmlB, sizeof(g_htmlB), view);

    TEST_ASSERT_TRUE(strcmp(g_htmlA, g_htmlB) != 0);
}

void test_exactly_one_control_per_group_is_marked_active(void) {
    // Active controls carry aria-pressed, so the state does not rely on colour
    // alone. Four groups have a selection: mode, brightness level, rotation and
    // the current animation.
    const char* html = renderPanel(makeView());
    TEST_ASSERT_EQUAL_UINT8(4u, countOf(html, "aria-pressed=\"true\""));
}

// ---------------------------------------------------------------------------
// Stage 9: device information
// ---------------------------------------------------------------------------

void test_device_information_uses_native_details(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "<details>"));
    TEST_ASSERT_TRUE(contains(html, "<summary>Informacje o urządzeniu</summary>"));
    TEST_ASSERT_TRUE(contains(html, "MikroTik-CB390D"));
    TEST_ASSERT_TRUE(contains(html, "192.168.88.31"));
    TEST_ASSERT_TRUE(contains(html, "16:46:35"));
}

void test_device_information_is_below_the_controls(void) {
    const char* html = renderPanel(makeView());

    const int lastControl = positionOf(html, "Orientacja");
    const int details = positionOf(html, "Informacje o urządzeniu");

    TEST_ASSERT_TRUE(lastControl >= 0);
    TEST_ASSERT_TRUE(details > lastControl);
}

// ---------------------------------------------------------------------------
// Stage 10 + 11: Wi-Fi change and credentials
// ---------------------------------------------------------------------------

void test_wifi_change_control_remains_present_and_authenticated_route(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "Zmień sieć Wi-Fi"));
    TEST_ASSERT_TRUE(contains(html, "action=\"/wifi/change\""));
    TEST_ASSERT_TRUE(contains(html, "POST"));
}

void test_panel_renders_no_credentials(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_FALSE(contains(html, PanelAuth::kUsername));
    TEST_ASSERT_FALSE(contains(html, PanelAuth::kPassword));
    TEST_ASSERT_FALSE(contains(html, "Authorization"));
    TEST_ASSERT_FALSE(contains(html, "WWW-Authenticate"));
}

void test_untrusted_values_are_escaped(void) {
    // The SSID comes from the network and must not be able to inject markup.
    PanelView view = makeView();
    view.ssid = "<script>x</script>";

    const char* html = renderPanel(view);
    TEST_ASSERT_FALSE(contains(html, "<script"));
    TEST_ASSERT_TRUE(contains(html, "&lt;script&gt;"));
}

// ---------------------------------------------------------------------------
// Stage 12/13/14: layout, mobile, semantics
// ---------------------------------------------------------------------------

void test_page_is_mobile_first_and_bounded(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "width=device-width"));
    TEST_ASSERT_TRUE(contains(html, "box-sizing:border-box"));
    TEST_ASSERT_TRUE(contains(html, "max-width:420px"));
    TEST_ASSERT_TRUE(contains(html, "flex-wrap:wrap"));
}

void test_uses_semantic_elements_not_clickable_divs(void) {
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(contains(html, "<form method=\"POST\""));
    TEST_ASSERT_TRUE(contains(html, "<button"));
    TEST_ASSERT_FALSE(contains(html, "<div onclick"));
    TEST_ASSERT_FALSE(contains(html, "role=\"button\""));
}

void test_active_controls_do_not_rely_on_colour_alone(void) {
    // The dark green identity is used for the active state...
    TEST_ASSERT_TRUE(contains(renderPanel(makeView()), "#003d2b"));

    // ...and every active control also carries a textual marker.
    const char* html = renderPanel(makeView());
    TEST_ASSERT_TRUE(contains(html, "✓"));
}

void test_render_is_deterministic(void) {
    // Same view in, same markup out. Guards against uninitialised state.
    PanelView view = makeView();
    renderInto(g_htmlA, sizeof(g_htmlA), view);
    renderInto(g_htmlB, sizeof(g_htmlB), view);

    TEST_ASSERT_EQUAL_STRING(g_htmlA, g_htmlB);
}

void test_render_matches_the_advertised_content_length(void) {
    // The page must actually fit the buffer the device reserves for it, or the
    // panel would be silently truncated on hardware.
    const char* html = renderPanel(makeView());

    TEST_ASSERT_TRUE(g_used > 0u);
    TEST_ASSERT_TRUE(g_used < kPageCapacity);

    // Not truncated: the sink would have stopped mid-tag if it overflowed.
    TEST_ASSERT_TRUE(strstr(html, "</html>") != nullptr);
    const size_t length = strlen(html);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(length), static_cast<uint32_t>(g_used));
}

// ---------------------------------------------------------------------------
// Animation display names
// ---------------------------------------------------------------------------

void test_every_animation_id_has_a_display_name(void) {
    // NONE included: the panel renders it when nothing is playing.
    TEST_ASSERT_TRUE(strlen(animationNameOf(AnimationId::NONE)) > 0u);

    const uint8_t count = static_cast<uint8_t>(AnimationId::ANIMATION_ID_COUNT);
    for (uint8_t raw = 1u; raw < count; ++raw) {
        const char* name = animationNameOf(static_cast<AnimationId>(raw));
        TEST_ASSERT_NOT_NULL(name);
        TEST_ASSERT_TRUE(strlen(name) > 0u);
        // A displayed name must never be the raw developer identifier.
        TEST_ASSERT_TRUE(strcmp(name, "unknown") != 0);
        TEST_ASSERT_TRUE(strcmp(name, "Nieznana") != 0);
    }

    // Out-of-range ids degrade safely.
    TEST_ASSERT_TRUE(strlen(animationNameOf(static_cast<AnimationId>(200u))) > 0u);
}

void test_display_names_are_human_readable_polish(void) {
    TEST_ASSERT_EQUAL_STRING("Bicie serca", animationNameOf(AnimationId::HEARTBEAT));
    TEST_ASSERT_EQUAL_STRING("Fajerwerki", animationNameOf(AnimationId::FIREWORKS));
    TEST_ASSERT_EQUAL_STRING("Ognisko", animationNameOf(AnimationId::CAMPFIRE));
    TEST_ASSERT_EQUAL_STRING("Zorza neonowa", animationNameOf(AnimationId::AURORA_NEON));
    TEST_ASSERT_EQUAL_STRING("Księżyc i gwiazdy", animationNameOf(AnimationId::MOON_STARS));
    TEST_ASSERT_EQUAL_STRING(
        "Słońce nad górami", animationNameOf(AnimationId::SUN_OVER_MOUNTAINS));
    TEST_ASSERT_EQUAL_STRING("Duszek", animationNameOf(AnimationId::HALLOWEEN_GHOST));
    TEST_ASSERT_EQUAL_STRING("Lisek", animationNameOf(AnimationId::FOX));
    TEST_ASSERT_EQUAL_STRING("Rudy kot", animationNameOf(AnimationId::ORANGE_CAT));

    // The raw developer-style names must not reach the panel.
    TEST_ASSERT_FALSE(strcmp("aurora neon", animationNameOf(AnimationId::AURORA_NEON)) == 0);
    TEST_ASSERT_FALSE(strcmp("none", animationNameOf(AnimationId::NONE)) == 0);
}

void test_panel_never_shows_a_raw_animation_identifier(void) {
    PanelView view = makeView();
    view.currentAnimation = AnimationId::SUN_OVER_MOUNTAINS;

    const char* html = renderPanel(view);

    TEST_ASSERT_TRUE(contains(html, "Słońce nad górami"));
    TEST_ASSERT_FALSE(contains(html, "sun over mountains"));
    TEST_ASSERT_FALSE(contains(html, "SUN_OVER_MOUNTAINS"));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_panel_uses_polish_section_headings);
    RUN_TEST(test_panel_uses_polish_control_labels);
    RUN_TEST(test_panel_has_no_javascript_and_no_external_resources);
    RUN_TEST(test_header_shows_connection_state);
    RUN_TEST(test_mode_highlight_matches_the_active_mode);
    RUN_TEST(test_animation_choices_are_inside_the_animation_card);
    RUN_TEST(test_previous_next_and_play_are_gone_from_the_panel);
    RUN_TEST(test_every_user_selectable_animation_is_offered);
    RUN_TEST(test_new_animations_are_each_offered_exactly_once);
    RUN_TEST(test_non_user_facing_animations_are_not_offered);
    RUN_TEST(test_exactly_the_current_animation_is_highlighted);
    RUN_TEST(test_teraz_wyswietlane_agrees_with_the_highlight);
    RUN_TEST(test_nothing_is_highlighted_when_nothing_is_playing);
    RUN_TEST(test_each_choice_is_one_atomic_post_of_its_own_id);
    RUN_TEST(test_animation_card_explains_that_selecting_switches_mode);
    RUN_TEST(test_brightness_is_five_direct_levels);
    RUN_TEST(test_old_step_controls_are_gone);
    RUN_TEST(test_panel_shows_no_brightness_percentage);
    RUN_TEST(test_panel_shows_the_current_level_label);
    RUN_TEST(test_orientation_offers_four_direct_choices);
    RUN_TEST(test_active_rotation_is_represented);
    RUN_TEST(test_exactly_one_control_per_group_is_marked_active);
    RUN_TEST(test_device_information_uses_native_details);
    RUN_TEST(test_device_information_is_below_the_controls);
    RUN_TEST(test_wifi_change_control_remains_present_and_authenticated_route);
    RUN_TEST(test_panel_renders_no_credentials);
    RUN_TEST(test_untrusted_values_are_escaped);
    RUN_TEST(test_page_is_mobile_first_and_bounded);
    RUN_TEST(test_uses_semantic_elements_not_clickable_divs);
    RUN_TEST(test_active_controls_do_not_rely_on_colour_alone);
    RUN_TEST(test_render_is_deterministic);
    RUN_TEST(test_render_matches_the_advertised_content_length);
    RUN_TEST(test_every_animation_id_has_a_display_name);
    RUN_TEST(test_display_names_are_human_readable_polish);
    RUN_TEST(test_panel_never_shows_a_raw_animation_identifier);
    return UNITY_END();
}
