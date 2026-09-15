// ============================================================================
// ratio_format.h —— dimensionless Q/D display formatting
// ----------------------------------------------------------------------------
// Q and D are ratios, not SI-valued quantities. Keep them in fixed decimal
// notation so the UI never turns them into engineering-prefix values such as
// m/u/k. The formatter also bounds extreme/small values so a 128 px display
// cannot be overrun by a very long decimal expansion.
// ============================================================================

#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace ui {

inline void trimRatioFixed(char* s)
{
    if (!s) return;
    char* dot = strchr(s, '.');
    if (!dot) return;
    char* end = s + strlen(s);
    while (end > dot + 1 && end[-1] == '0') *--end = '\0';
    if (end > dot && end[-1] == '.') *--end = '\0';
}

inline const char* fmtRatio(double v, char* buf, int len)
{
    if (!buf || len <= 0) return buf;
    if (!isfinite(v)) {
        snprintf(buf, len, "--");
        return buf;
    }
    if (v == 0.0) {
        snprintf(buf, len, "0");
        return buf;
    }

    // Explicit fixed-decimal bounds: preserve the fact that a value is beyond
    // the compact display range without falling back to e-notation or SI
    // engineering prefixes.
    if (v >= 1000000.0) {
        snprintf(buf, len, ">999999");
        return buf;
    }
    if (v <= -1000000.0) {
        snprintf(buf, len, "<-999999");
        return buf;
    }
    if (v > 0.0 && v < 0.000001) {
        snprintf(buf, len, "<0.000001");
        return buf;
    }
    if (v < 0.0 && v > -0.000001) {
        snprintf(buf, len, ">-0.000001");
        return buf;
    }

    const double av = fabs(v);
    int decimals = 6;
    if (av >= 10000.0) decimals = 0;
    else if (av >= 1000.0) decimals = 1;
    else if (av >= 100.0) decimals = 2;
    else if (av >= 10.0) decimals = 3;
    else if (av >= 1.0) decimals = 4;
    else if (av >= 0.1) decimals = 5;

    snprintf(buf, len, "%.*f", decimals, v);
    trimRatioFixed(buf);
    return buf;
}

}  // namespace ui
