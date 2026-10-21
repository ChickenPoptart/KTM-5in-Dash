#pragma once

// What Google Maps' direction text ("Turn right onto Main St") asks for,
// so the dash can draw its own arrow instead of the phone's tiny icon.

#include <stdlib.h>
#include <string.h>

enum ManeuverKind {
    MANEUVER_UNKNOWN,  // not recognised: show the phone's icon
    MANEUVER_TURN,
    MANEUVER_SLIGHT,
    MANEUVER_SHARP,
    MANEUVER_UTURN,
    MANEUVER_STRAIGHT,
    MANEUVER_KEEP,      // at a fork
    MANEUVER_RAMP,      // off the road: an exit or a ramp
    MANEUVER_ROUNDABOUT,
    MANEUVER_MERGE,
    MANEUVER_ARRIVE,
};

struct Maneuver {
    ManeuverKind kind;
    int side;  // -1 left, 1 right, 0 neither
    int exit;  // roundabouts: which exit (0 if not said)
};

inline Maneuver maneuverFor(const char* directions) {
    if (strstr(directions, "destination") || strncmp(directions, "Arrive", 6) == 0) {
        return {MANEUVER_ARRIVE, 0};
    }
    if (strstr(directions, "roundabout")) {
        const char* take = strstr(directions, "take the ");
        return {MANEUVER_ROUNDABOUT, 0, take ? atoi(take + 9) : 0};  // "2nd exit" -> 2
    }
    if (strstr(directions, "U-turn")) return {MANEUVER_UTURN, -1};  // riding on the right
    // Left or right, whichever the instruction says first, as a whole word
    // (so "Turn right onto Cleft Ridge Rd" is still a right)
    const char* left = strstr(directions, " left");
    const char* right = strstr(directions, " right");
    int side = left && (!right || left < right) ? -1 : 1;
    if (strstr(directions, "Slight ")) return {MANEUVER_SLIGHT, side};
    if (strstr(directions, "Sharp ")) return {MANEUVER_SHARP, side};
    if (strstr(directions, "Turn ")) return {MANEUVER_TURN, side};
    if (strstr(directions, "Keep ")) return {MANEUVER_KEEP, side};
    if (strstr(directions, "Merge ")) return {MANEUVER_MERGE, 0};
    if (strstr(directions, "ramp") || strstr(directions, "Take exit") || strstr(directions, "Take the exit")) {
        return {MANEUVER_RAMP, side};  // (US exits are on the right unless it says left)
    }
    if (strncmp(directions, "Head ", 5) == 0 || strncmp(directions, "Continue", 8) == 0) {
        return {MANEUVER_STRAIGHT, 0};
    }
    return {MANEUVER_UNKNOWN, 0};
}
