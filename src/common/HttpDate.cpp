// The MIT License (MIT)
//
// Copyright (c) 2024 Quarkifi Technologies Pvt Ltd
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "HttpDate.h"

#include <stdio.h>
#include <string.h>

namespace {

int monthIndex(const char *m) {
    static const char *names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++) {
        if (strcmp(m, names[i]) == 0) return i + 1;
    }
    return 0;
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeap(y)) ? 29 : d[m - 1];
}

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's algorithm).
long daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

bool build(int year, const char *mon, int day, int hh, int mm, int ss, uint32_t &out) {
    int month = monthIndex(mon);
    if (!month || year < 1970 || year > 2105) return false;
    if (day < 1 || day > daysInMonth(year, month)) return false;
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 60) return false;
    long days = daysFromCivil(year, month, day);
    out = (uint32_t)days * 86400UL + (uint32_t)(hh * 3600 + mm * 60 + ss);
    return true;
}

}  // namespace

bool HttpDate::parse(const char *v, uint32_t &epochOut) {
    if (!v || !*v) return false;
    char mon[4] = {0};
    char tz[4] = {0};
    int day, year, hh, mm, ss;

    // IMF-fixdate: "Sun, 06 Nov 1994 08:49:37 GMT"
    if (sscanf(v, "%*[A-Za-z], %2d %3s %4d %2d:%2d:%2d %3s", &day, mon, &year, &hh, &mm, &ss, tz) == 7 &&
        strcmp(tz, "GMT") == 0) {
        return build(year, mon, day, hh, mm, ss, epochOut);
    }

    // RFC 850: "Sunday, 06-Nov-94 08:49:37 GMT"
    memset(mon, 0, sizeof(mon));
    memset(tz, 0, sizeof(tz));
    if (sscanf(v, "%*[A-Za-z], %2d-%3[A-Za-z]-%2d %2d:%2d:%2d %3s", &day, mon, &year, &hh, &mm, &ss, tz) == 7 &&
        strcmp(tz, "GMT") == 0) {
        year += (year < 70) ? 2000 : 1900;
        return build(year, mon, day, hh, mm, ss, epochOut);
    }

    // asctime: "Sun Nov  6 08:49:37 1994"
    memset(mon, 0, sizeof(mon));
    if (sscanf(v, "%*3[A-Za-z] %3[A-Za-z] %2d %2d:%2d:%2d %4d", mon, &day, &hh, &mm, &ss, &year) == 6) {
        return build(year, mon, day, hh, mm, ss, epochOut);
    }

    return false;
}
