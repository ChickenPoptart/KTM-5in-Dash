#pragma once

#include "chart_series.h"
#include "trip_stats.h"

// The screens you swipe to from the dash: trip meters and ride charts.

// ---- Trips: this ride, Trip 1 and Trip 2 side by side ----

enum TripColumn { TRIP_RIDE, TRIP_1, TRIP_2, TRIP_COLUMNS };

// full: lay the page out from scratch (else only changed values repaint)
void drawTripsPage(bool full, const TripStats* const trips[TRIP_COLUMNS]);
// TRIP_1 or TRIP_2 if (x, y) is on that column's reset button, else -1
int tripResetAt(int x, int y);

// ---- Charts: speed, RPM, coolant and elevation over the whole ride ----

const unsigned long CHART_SAMPLE_MS = 5000;  // one sample every 5 s
const int CHART_POINTS = 240;                // whole ride squeezed to fit

struct RideCharts {
    ChartSeries<CHART_POINTS> speed, rpm, coolant, elevation;
};

void drawChartsPage(bool full, const RideCharts& charts);

// ---- Which page you're on: dots along the bottom edge ----

void drawPageDots(int page, int count);
