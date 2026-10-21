#pragma once

#include <stdio.h>

// How long a text or app notification stays on screen.
const unsigned long ALERT_MESSAGE_MS = 8000;

enum AlertKind { ALERT_CALL, ALERT_MESSAGE };

// The banner for calls and messages from the phone.
class AlertBanner {
public:
    void message(const char* app, const char* from, const char* text, unsigned long nowMs) {
        if (ringing) return;  // the call matters more
        shownKind = ALERT_MESSAGE;
        snprintf(shownApp, sizeof(shownApp), "%s", app);
        snprintf(shownTitle, sizeof(shownTitle), "%s", from);
        snprintf(shownText, sizeof(shownText), "%s", text);
        shownMs = nowMs;
        showing = true;
    }

    // Incoming call: up until the phone stops ringing
    void callRinging(const char* caller, unsigned long nowMs) {
        shownKind = ALERT_CALL;
        snprintf(shownTitle, sizeof(shownTitle), "%s", caller);
        showing = true;
        ringing = true;
    }

    // Answered, declined or missed
    void callEnded() {
        showing = false;
        ringing = false;
    }

    // Tapped away. A dismissed call still keeps texts from popping up
    // until it stops ringing.
    void dismiss() { showing = false; }

    bool visible(unsigned long nowMs) const {
        return showing && (ringing || nowMs - shownMs < ALERT_MESSAGE_MS);
    }

    AlertKind kind() const { return shownKind; }
    const char* app() const { return shownApp; }
    const char* title() const { return shownTitle; }  // caller, or who sent it
    const char* text() const { return shownText; }

private:
    bool showing = false;
    bool ringing = false;
    unsigned long shownMs = 0;
    AlertKind shownKind = ALERT_MESSAGE;
    char shownApp[32] = "";
    char shownTitle[64] = "";
    char shownText[128] = "";
};
