// Tests for HttpDate::parse (RFC 9110 Date header formats).
#include "TestHelpers.h"
#include "common/HttpDate.h"

static uint32_t parseOk(const char *s) {
    uint32_t t = 0;
    EXPECT_TRUE(HttpDate::parse(s, t));
    return t;
}

static bool parses(const char *s) {
    uint32_t t = 0;
    return HttpDate::parse(s, t);
}

TEST(httpdate_imf_fixdate) {
    EXPECT_EQ(parseOk("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777u);  // RFC example
    EXPECT_EQ(parseOk("Wed, 04 Nov 2026 10:15:30 GMT"), 1793787330u);
    EXPECT_EQ(parseOk("Thu, 01 Jan 1970 00:00:00 GMT"), 0u);
}

TEST(httpdate_rfc850) {
    EXPECT_EQ(parseOk("Sunday, 06-Nov-94 08:49:37 GMT"), 784111777u);
    EXPECT_EQ(parseOk("Wednesday, 04-Nov-26 10:15:30 GMT"), 1793787330u);  // 2-digit year < 70 => 20xx
}

TEST(httpdate_asctime) {
    EXPECT_EQ(parseOk("Sun Nov  6 08:49:37 1994"), 784111777u);
    EXPECT_EQ(parseOk("Wed Nov 4 10:15:30 2026"), 1793787330u);
}

TEST(httpdate_leap_years) {
    EXPECT_EQ(parseOk("Thu, 29 Feb 2024 12:00:00 GMT"), 1709208000u);
    EXPECT_EQ(parseOk("Tue, 29 Feb 2000 00:00:00 GMT"), 951782400u);
    EXPECT_FALSE(parses("Wed, 29 Feb 2023 00:00:00 GMT"));
    EXPECT_FALSE(parses("Thu, 29 Feb 1900 00:00:00 GMT"));  // also < 1970
}

TEST(httpdate_end_of_year_and_month) {
    EXPECT_EQ(parseOk("Thu, 31 Dec 2026 23:59:59 GMT"), 1798761599u);
    EXPECT_EQ(parseOk("Fri, 01 Jan 2027 00:00:00 GMT"), 1798761600u);
}

TEST(httpdate_rejects_invalid) {
    EXPECT_FALSE(parses(""));
    EXPECT_FALSE(parses(nullptr));
    EXPECT_FALSE(parses("garbage"));
    EXPECT_FALSE(parses("Sun, 06 Nov 1994 08:49:37 UTC"));  // must be GMT
    EXPECT_FALSE(parses("Sun, 06 Nov 1994 08:49:37"));
    EXPECT_FALSE(parses("Sun, 31 Nov 1994 08:49:37 GMT"));  // Nov has 30 days
    EXPECT_FALSE(parses("Sun, 00 Nov 1994 08:49:37 GMT"));
    EXPECT_FALSE(parses("Sun, 06 Foo 1994 08:49:37 GMT"));
    EXPECT_FALSE(parses("Sun, 06 Nov 1994 24:00:00 GMT"));
    EXPECT_FALSE(parses("Sun, 06 Nov 1994 08:60:00 GMT"));
    EXPECT_FALSE(parses("Sun, 06 Nov 1969 08:49:37 GMT"));
    EXPECT_FALSE(parses("Sun, 06 nov 1994 08:49:37 GMT"));  // month is case-sensitive
}

TEST(httpdate_string_overload) {
    uint32_t t = 0;
    EXPECT_TRUE(HttpDate::parse(String("Sun, 06 Nov 1994 08:49:37 GMT"), t));
    EXPECT_EQ(t, 784111777u);
}
