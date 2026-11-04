// Tests for ESP32OtaPlatform / ESP8266OtaPlatform (selected at build time).
// Shared behaviour first, then platform-specific sections.
#include "TestHelpers.h"

using mock::state;
using S = std::vector<int>;

static const char *URL = "https://example.com/fw.bin";
static const char *HTTP_URL = "http://example.com/fw.bin";
static const char *ROOT_CA =
    "-----BEGIN CERTIFICATE-----\nMIIBroot\n-----END CERTIFICATE-----\n";
static const char *MD5 = "0123456789abcdef0123456789abcdef";

// Body of `n` bytes, delivered in chunks of `chunk`, each read taking `ms`.
static mock::HttpResponse body(size_t n, size_t chunk = 4096, unsigned long ms = 0) {
    mock::HttpResponse r = mock::okResponse(n);
    r.maxAvailable = chunk;
    r.msPerRead = ms;
    return r;
}

static mock::HttpResponse status(int code) {
    mock::HttpResponse r = body(10);
    r.code = code;
    return r;
}

static void expectAllHttpEnded() {
    for (size_t i = 0; i < state.http.size(); i++) EXPECT_TRUE(state.http[i].ended);
    EXPECT_EQ(state.httpLive, 0);
}

// Transient failure on every attempt: (STARTING, err) x3, then ERR_RETRIES.
static S retriedRun(int err) {
    S s;
    for (int i = 0; i < 3; i++) { s.push_back(OtaStatus::STARTING); s.push_back(err); }
    s.push_back(OtaStatus::ERR_RETRIES);
    return s;
}

static const S SUCCESS_LIFECYCLE = {OtaStatus::STARTING, OtaStatus::INSTALLING,
                                    OtaStatus::SUCCESS, OtaStatus::RESTARTING};

// ================================================================ guards

TEST(platform_initial_state) {
    Platform p;
    EXPECT_EQ((int)p.getState(), (int)OtaState::IDLE);
    EXPECT_FALSE(p.isBusy());
}

TEST(platform_no_wifi_fails_fast) {
    Platform p;
    p.setCallback(th::record);
    state.wifiStatus = WL_DISCONNECTED;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::ERR_NO_WIFI}));
    EXPECT_EQ(state.http.size(), (size_t)0);
    EXPECT_EQ((int)p.getState(), (int)OtaState::IDLE);
}

static Platform *g_reentrant = nullptr;
static byte g_reentrantOn = 0;
static bool g_reentrantResult = true;
static void reentrantCallback(const byte status, const byte progress) {
    th::record(status, progress);
    if (status == g_reentrantOn && g_reentrant) {
        Platform *p = g_reentrant;
        g_reentrant = nullptr;  // only once
        g_reentrantResult = p->startOTA(URL, "", "");
    }
}

TEST(platform_busy_rejects_reentrant_start_during_download) {
    Platform p;
    g_reentrant = &p;
    g_reentrantOn = OtaStatus::INSTALLING;
    g_reentrantResult = true;
    p.setCallback(reentrantCallback);
    state.responses.push_back(body(2000));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_FALSE(g_reentrantResult);
    EXPECT_EQ(th::count(OtaStatus::ERR_BUSY), 1);
    EXPECT_EQ(state.http.size(), (size_t)1);
}

TEST(platform_busy_rejects_reentrant_start_from_error_callback) {
    // Previously the state was FAILED inside the error callback, so a nested
    // OTA could start in the middle of the retry loop.
    Platform p;
    g_reentrant = &p;
    g_reentrantOn = OtaStatus::ERR_HTTP;
    g_reentrantResult = true;
    p.setCallback(reentrantCallback);
    state.responses.push_back(status(500));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_FALSE(g_reentrantResult);
    EXPECT_EQ(th::count(OtaStatus::ERR_BUSY), 1);
    EXPECT_EQ(state.http.size(), (size_t)3);  // only the outer attempts
}

TEST(platform_busy_rejects_reentrant_start_after_retries_exhausted) {
    // State is already FAILED when ERR_RETRIES is published; startOTA must
    // still count as running until it returns.
    Platform p;
    g_reentrant = &p;
    g_reentrantOn = OtaStatus::ERR_RETRIES;
    g_reentrantResult = true;
    p.setCallback(reentrantCallback);
    state.responses.push_back(status(500));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_FALSE(g_reentrantResult);
    EXPECT_EQ(th::count(OtaStatus::ERR_BUSY), 1);
    EXPECT_EQ(state.http.size(), (size_t)3);
}

TEST(platform_busy_rejects_reentrant_start_while_restarting) {
    Platform p;
    g_reentrant = &p;
    g_reentrantOn = OtaStatus::RESTARTING;
    g_reentrantResult = true;
    p.setCallback(reentrantCallback);
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_FALSE(g_reentrantResult);
    EXPECT_EQ(th::count(OtaStatus::ERR_BUSY), 1);
    EXPECT_EQ(state.restartCount, 1);
}

// ================================================================ happy path

TEST(platform_success_flow_and_status_order) {
    Platform p;
    p.setCallback(th::record);
    state.nowMs = 10000;
    state.responses.push_back(body(5000, 700, 400));
    EXPECT_TRUE(p.startOTA(URL, "", ""));

    EXPECT_EQ(th::lifecycle(), SUCCESS_LIFECYCLE);
    // DOWNLOADING (0..100) sits between STARTING and INSTALLING.
    S st = th::statuses();
    ASSERT_TRUE(st.size() >= 6);
    EXPECT_EQ(st[0], (int)OtaStatus::STARTING);
    EXPECT_EQ(th::events()[1], (th::Event{OtaStatus::DOWNLOADING, 0}));
    size_t inst = 0;
    for (size_t i = 0; i < st.size(); i++) if (st[i] == OtaStatus::INSTALLING) inst = i;
    EXPECT_EQ(th::events()[inst - 1], (th::Event{OtaStatus::DOWNLOADING, 100}));

    EXPECT_EQ(state.updateBeginSize, (size_t)5000);
    EXPECT_TRUE(state.updateData == mock::makeBody(5000));
    EXPECT_TRUE(state.updateFinalized);
    EXPECT_EQ(state.restartCount, 1);
    EXPECT_EQ((int)p.getState(), (int)OtaState::SUCCESS);
    EXPECT_FALSE(p.isBusy());
    expectAllHttpEnded();
}

TEST(platform_request_configuration) {
    Platform p;
    state.responses.push_back(body(10));
    p.startOTA(URL, "Basic dXNlcjpwYXNz", "");
    ASSERT_TRUE(state.http.size() == 1);
    const mock::HttpRecord &h = state.http[0];
    EXPECT_EQ(h.url, std::string(URL));
    EXPECT_EQ(h.method, std::string("GET"));
    EXPECT_EQ(h.timeout, 60000);
    EXPECT_TRUE(h.followRedirects);
    EXPECT_TRUE(h.http10);  // no chunked encoding
    EXPECT_EQ(h.headers.at("Accept-Encoding"), std::string("identity"));
    EXPECT_EQ(h.headers.at("Authorization"), std::string("Basic dXNlcjpwYXNz"));
}

TEST(platform_no_auth_header_when_auth_empty) {
    Platform p;
    state.responses.push_back(body(10));
    p.startOTA(URL, "", "");
    ASSERT_TRUE(state.http.size() == 1);
    EXPECT_EQ(state.http[0].headers.count("Authorization"), (size_t)0);
}

TEST(platform_https_without_root_ca_is_insecure) {
    Platform p;
    state.responses.push_back(body(10));
    p.startOTA(URL, "", "");
    ASSERT_TRUE(state.http.size() == 1);
    EXPECT_TRUE(state.http[0].secureClient);
    EXPECT_TRUE(state.http[0].insecure);
}

TEST(platform_http_url_uses_plain_client) {
    Platform p;
    state.responses.push_back(body(10));
    EXPECT_TRUE(p.startOTA(HTTP_URL, "", ""));
    ASSERT_TRUE(state.http.size() == 1);
    EXPECT_FALSE(state.http[0].secureClient);
}

TEST(platform_reads_in_buffer_sized_chunks) {
    Platform p;
    state.responses.push_back(body(5000, 4096));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    for (size_t n : state.updateWriteSizes) EXPECT_TRUE(n <= (size_t)CommonOtaPlatform::BUFFER_SIZE);
    EXPECT_EQ(state.updateWriteSizes.size(), (size_t)5);  // 4*1024 + 904
}

TEST(platform_never_reads_past_content_length) {
    Platform p;
    mock::HttpResponse r = body(3000, 4096);
    r.sizeOverride = 2500;  // server sends trailing junk beyond Content-Length
    state.responses.push_back(r);
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(state.updateData.size(), (size_t)2500);
}

TEST(platform_progress_monotonic_throttled_and_complete) {
    Platform p;
    p.setCallback(th::record);
    state.nowMs = 10000;
    state.responses.push_back(body(10240, 1024, 600));  // 10 reads, 600 ms each
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    S prog = th::progressValues();
    ASSERT_TRUE(prog.size() >= 3);
    EXPECT_TRUE(prog.size() <= 8);  // ~6 s => at most 0, ~5 ticks, 100
    EXPECT_EQ(prog.front(), 0);
    EXPECT_EQ(prog.back(), 100);
    for (size_t i = 1; i < prog.size(); i++) EXPECT_TRUE(prog[i] > prog[i - 1]);
}

TEST(platform_fast_download_still_reports_0_and_100) {
    Platform p;
    p.setCallback(th::record);
    state.nowMs = 300;  // right after boot, sub-second download
    state.responses.push_back(body(3000, 1024, 0));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::progressValues(), (S{0, 100}));
}

TEST(platform_progress_restarts_each_attempt) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse partial = body(3000, 1024);
    partial.readZeroAt = 1024;
    state.responses.push_back(partial);
    state.responses.push_back(body(3000, 1024));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    S prog = th::progressValues();
    ASSERT_TRUE(prog.size() >= 3);
    EXPECT_EQ(prog[0], 0);
    EXPECT_EQ(prog.back(), 100);
    EXPECT_EQ(std::count(prog.begin(), prog.end(), 0), 2);  // one 0% per attempt
}

TEST(platform_md5_passed_to_update) {
    Platform p;
    state.responses.push_back(body(100));
    state.imageMd5 = MD5;
    EXPECT_TRUE(p.startOTA(URL, "", MD5));
    EXPECT_EQ(state.updateExpectedMd5, std::string(MD5));
    EXPECT_TRUE(state.updateFinalized);
}

TEST(platform_md5_uppercase_accepted) {
    Platform p;
    state.responses.push_back(body(100));
    state.imageMd5 = MD5;
    EXPECT_TRUE(p.startOTA(URL, "", "0123456789ABCDEF0123456789ABCDEF"));
}

TEST(platform_md5_mismatch_not_committed) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(body(100));
    state.imageMd5 = "ffffffffffffffffffffffffffffffff";
    EXPECT_FALSE(p.startOTA(URL, "", MD5));
    EXPECT_EQ(th::count(OtaStatus::ERR_FINALIZE), 3);
    EXPECT_FALSE(state.updateFinalized);
    EXPECT_EQ(state.restartCount, 0);
}

// ================================================================ transient failures (retried)

TEST(platform_retry_delay_between_attempts_only) {
    Platform p;
    state.responses.push_back(status(503));
    p.startOTA(URL, "", "");
    EXPECT_EQ(state.delays, (std::vector<unsigned long>{2000, 2000}));  // none after last
}

TEST(platform_5xx_and_transport_errors_are_retried) {
    const int codes[] = {500, 502, 503, 408, 429, HTTPC_ERROR_CONNECTION_REFUSED};
    for (int code : codes) {
        mock::reset();
        th::clear();
        Platform p;
        p.setCallback(th::record);
        state.responses.push_back(status(code));
        EXPECT_FALSE(p.startOTA(URL, "", ""));
        EXPECT_EQ(th::statuses(), retriedRun(OtaStatus::ERR_HTTP));
        EXPECT_EQ(state.http.size(), (size_t)3);
        expectAllHttpEnded();
    }
}

TEST(platform_flash_write_failure_retried) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(body(3000, 1024));
    state.updateWriteShortAt = 2000;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), retriedRun(OtaStatus::ERR_WRITE));
    EXPECT_EQ(state.updateEndCount, 3);  // update aborted each attempt
    EXPECT_FALSE(state.updateFinalized);
    expectAllHttpEnded();
}

TEST(platform_stream_read_zero_is_write_error) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse r = body(3000, 1024);
    r.readZeroAt = 1024;
    state.responses.push_back(r);
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), retriedRun(OtaStatus::ERR_WRITE));
}

TEST(platform_stalled_stream_times_out) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse r = body(3000, 1024);
    r.stallAfter = 1024;
    state.responses.push_back(r);
    unsigned long t0 = state.nowMs;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), retriedRun(OtaStatus::ERR_TIMEOUT));
    unsigned long elapsed = state.nowMs - t0;  // 3 x 20 s window + 2 x 2 s retry delay
    EXPECT_TRUE(elapsed >= 3 * 20000UL + 4000UL);
    EXPECT_TRUE(elapsed <= 3 * 20100UL + 4000UL);
    EXPECT_FALSE(state.updateFinalized);
    expectAllHttpEnded();
}

TEST(platform_yields_while_waiting_for_data) {
    Platform p;
    mock::HttpResponse r = body(3000, 1024);
    r.stallAfter = 1024;
    state.responses.push_back(r);
    p.startOTA(URL, "", "");
    EXPECT_TRUE(state.yieldCount > 1000);  // watchdog fed in the wait loop
}

TEST(platform_disconnect_mid_download_is_http_error_fast) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse r = body(3000, 1024);
    r.disconnectAfter = 1024;
    r.stallAfter = 1024;
    state.responses.push_back(r);
    unsigned long t0 = state.nowMs;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), retriedRun(OtaStatus::ERR_HTTP));
    EXPECT_EQ(th::count(OtaStatus::ERR_FINALIZE), 0);
    EXPECT_EQ(th::count(OtaStatus::ERR_TIMEOUT), 0);
    EXPECT_TRUE(state.nowMs - t0 < 5000UL);  // no 20 s wait
    EXPECT_FALSE(state.updateFinalized);
}

TEST(platform_buffered_data_after_disconnect_is_still_read) {
    Platform p;
    mock::HttpResponse r = body(3000, 1024);
    r.disconnectAfter = 0;  // server already closed, but all data is buffered
    state.responses.push_back(r);
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_TRUE(state.updateData == mock::makeBody(3000));
}

TEST(platform_finalize_failure_retried) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(body(500));
    state.updateEndOk = false;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    S expect;
    for (int i = 0; i < 3; i++) {
        expect.push_back(OtaStatus::STARTING);
        expect.push_back(OtaStatus::INSTALLING);
        expect.push_back(OtaStatus::ERR_FINALIZE);
    }
    expect.push_back(OtaStatus::ERR_RETRIES);
    EXPECT_EQ(th::lifecycle(), expect);
    EXPECT_EQ(state.restartCount, 0);
}

TEST(platform_recovers_on_second_attempt) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(status(500));
    state.responses.push_back(body(2048, 1024));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), (S{OtaStatus::STARTING, OtaStatus::ERR_HTTP,
                                  OtaStatus::STARTING, OtaStatus::INSTALLING,
                                  OtaStatus::SUCCESS, OtaStatus::RESTARTING}));
    EXPECT_TRUE(state.updateData == mock::makeBody(2048));
    EXPECT_EQ(state.restartCount, 1);
}

TEST(platform_can_start_again_after_failure) {
    Platform p;
    state.responses.push_back(status(500));
    state.responses.push_back(status(500));
    state.responses.push_back(status(500));
    state.responses.push_back(body(100));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_FALSE(p.isBusy());
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(state.restartCount, 1);
}

// ================================================================ permanent failures (not retried)

TEST(platform_4xx_not_retried) {
    const int codes[] = {400, 401, 403, 404, 301};
    for (int code : codes) {
        mock::reset();
        th::clear();
        Platform p;
        p.setCallback(th::record);
        state.responses.push_back(status(code));
        EXPECT_FALSE(p.startOTA(URL, "", ""));
        EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_HTTP}));
        EXPECT_EQ(state.http.size(), (size_t)1);
        EXPECT_TRUE(state.delays.empty());
        EXPECT_EQ((int)p.getState(), (int)OtaState::FAILED);
        EXPECT_FALSE(p.isBusy());
        expectAllHttpEnded();
    }
}

TEST(platform_invalid_url_begin_failure_not_retried) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse r;
    r.beginOk = false;
    state.responses.push_back(r);
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_HTTP}));
    EXPECT_EQ(state.http.size(), (size_t)1);
    EXPECT_EQ(state.updateBeginCount, 0);
}

TEST(platform_missing_content_length_not_retried) {
    Platform p;
    p.setCallback(th::record);
    mock::HttpResponse r = body(10);
    r.sizeOverride = -1;
    state.responses.push_back(r);
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_HTTP}));
    expectAllHttpEnded();
}

TEST(platform_zero_content_length_not_retried) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(body(0));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_HTTP}));
}

TEST(platform_no_space_not_retried) {
    Platform p;
    p.setCallback(th::record);
    state.responses.push_back(body(100));
    state.updateBeginOk = false;
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_NO_SPACE}));
    EXPECT_EQ(state.updateBeginCount, 1);
    expectAllHttpEnded();
}

// ================================================================ root CA

TEST(platform_root_ca_disables_insecure) {
    Platform p;
    p.setRootCA(ROOT_CA);
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    const mock::HttpRecord &h = state.http.back();
    EXPECT_TRUE(h.secureClient);
    EXPECT_FALSE(h.insecure);
    EXPECT_EQ(h.method, std::string("GET"));
}

TEST(platform_root_ca_rejects_plain_http) {
    Platform p;
    p.setCallback(th::record);
    p.setRootCA(ROOT_CA);
    EXPECT_FALSE(p.startOTA(HTTP_URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::ERR_BAD_ARGS}));
    EXPECT_EQ(state.http.size(), (size_t)0);
}

TEST(platform_tls_failure_with_root_ca_is_retried_http_error) {
    // A certificate that doesn't chain to the root surfaces as a negative
    // HTTPClient code (connection failed).
    Platform p;
    p.setCallback(th::record);
    p.setRootCA(ROOT_CA);
    state.responses.push_back(status(HTTPC_ERROR_CONNECTION_REFUSED));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::count(OtaStatus::ERR_HTTP), 3);
    EXPECT_EQ(state.updateBeginCount, 0);
}

// ================================================================ platform specific

#if defined(ESP32)

TEST(esp32_root_ca_uses_setCACert_and_no_clock) {
    Platform p;
    p.setRootCA(ROOT_CA);
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    ASSERT_TRUE(state.http.size() == 1);  // no HEAD probe on ESP32
    EXPECT_EQ(state.http[0].caCert, std::string(ROOT_CA));
}

TEST(esp32_restart_sequence) {
    Platform p;
    state.responses.push_back(body(10));
    p.startOTA(URL, "", "");
    EXPECT_EQ(state.delays, (std::vector<unsigned long>{500, 1000}));
    EXPECT_EQ(state.restartCount, 1);
    EXPECT_EQ(state.resetCount, 0);
    EXPECT_FALSE(state.lastEndEvenIfRemaining);
}

#elif defined(ESP8266)

static mock::HttpResponse headResponse(int code, const char *date) {
    mock::HttpResponse r;
    r.code = code;
    if (date) r.headers["Date"] = date;
    return r;
}

TEST(esp8266_root_ca_with_synced_clock_skips_probe) {
    Platform p;
    p.setRootCA(ROOT_CA);
    state.epoch = 1793787330UL;  // clock set by the sketch (e.g. configTime)
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    ASSERT_TRUE(state.http.size() == 1);
    EXPECT_EQ(state.http[0].method, std::string("GET"));
    EXPECT_TRUE(state.http[0].trustAnchors != nullptr);
    EXPECT_EQ(state.http[0].x509Time, 1793787330L);
}

TEST(esp8266_root_ca_without_clock_uses_server_date) {
    Platform p;
    p.setRootCA(ROOT_CA);
    state.epoch = 0;  // never synced (1970)
    state.responses.push_back(headResponse(200, "Wed, 04 Nov 2026 10:15:30 GMT"));
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "Basic abc", ""));
    ASSERT_TRUE(state.http.size() == 2);

    const mock::HttpRecord &probe = state.http[0];
    EXPECT_EQ(probe.method, std::string("HEAD"));
    EXPECT_EQ(probe.url, std::string(URL));
    EXPECT_TRUE(probe.insecure);
    EXPECT_EQ(probe.collected, (std::vector<std::string>{"Date"}));
    EXPECT_EQ(probe.headers.count("Authorization"), (size_t)0);  // creds never sent unverified
    EXPECT_TRUE(probe.ended);

    const mock::HttpRecord &get = state.http[1];
    EXPECT_EQ(get.method, std::string("GET"));
    EXPECT_FALSE(get.insecure);
    EXPECT_TRUE(get.trustAnchors != nullptr);
    EXPECT_EQ(get.x509Time, 1793787330L);
    EXPECT_EQ(state.httpLive, 0);
}

TEST(esp8266_server_date_from_error_response_is_used) {
    // e.g. S3 signed-for-GET URL answers HEAD with 403 — Date is still present.
    Platform p;
    p.setRootCA(ROOT_CA);
    state.epoch = 0;
    state.responses.push_back(headResponse(403, "Wed, 04 Nov 2026 10:15:30 GMT"));
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(state.http.back().x509Time, 1793787330L);
}

TEST(esp8266_missing_date_is_no_time_error) {
    Platform p;
    p.setCallback(th::record);
    p.setRootCA(ROOT_CA);
    state.epoch = 0;
    state.responses.push_back(headResponse(200, nullptr));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), retriedRun(OtaStatus::ERR_NO_TIME));
    for (const mock::HttpRecord &h : state.http) EXPECT_EQ(h.method, std::string("HEAD"));
    EXPECT_EQ(state.updateBeginCount, 0);
}

TEST(esp8266_garbage_or_old_date_is_no_time_error) {
    const char *dates[] = {"not a date", "Thu, 01 Jan 2015 00:00:00 GMT"};
    for (const char *d : dates) {
        mock::reset();
        th::clear();
        Platform p;
        p.setCallback(th::record);
        p.setRootCA(ROOT_CA);
        state.epoch = 0;
        state.responses.push_back(headResponse(200, d));
        EXPECT_FALSE(p.startOTA(URL, "", ""));
        EXPECT_EQ(th::count(OtaStatus::ERR_NO_TIME), 3);
    }
}

TEST(esp8266_probe_connection_failure_is_no_time_error) {
    Platform p;
    p.setCallback(th::record);
    p.setRootCA(ROOT_CA);
    state.epoch = 0;
    state.responses.push_back(headResponse(HTTPC_ERROR_CONNECTION_REFUSED, "Wed, 04 Nov 2026 10:15:30 GMT"));
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::count(OtaStatus::ERR_NO_TIME), 3);
}

TEST(esp8266_no_time_then_recovers) {
    Platform p;
    p.setCallback(th::record);
    p.setRootCA(ROOT_CA);
    state.epoch = 0;
    state.responses.push_back(headResponse(200, nullptr));
    state.responses.push_back(headResponse(200, "Wed, 04 Nov 2026 10:15:30 GMT"));
    state.responses.push_back(body(100));
    EXPECT_TRUE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::lifecycle(), (S{OtaStatus::STARTING, OtaStatus::ERR_NO_TIME,
                                  OtaStatus::STARTING, OtaStatus::INSTALLING,
                                  OtaStatus::SUCCESS, OtaStatus::RESTARTING}));
}

TEST(esp8266_unparseable_root_ca_is_bad_args) {
    Platform p;
    p.setCallback(th::record);
    p.setRootCA("this is not PEM");
    EXPECT_FALSE(p.startOTA(URL, "", ""));
    EXPECT_EQ(th::statuses(), (S{OtaStatus::STARTING, OtaStatus::ERR_BAD_ARGS}));
    EXPECT_EQ(state.http.size(), (size_t)0);
}

TEST(esp8266_trust_anchors_lifetime) {
    {
        Platform p;
        p.setRootCA(ROOT_CA);
        EXPECT_EQ(state.x509Live, 1);
        p.setRootCA(ROOT_CA);   // replaced, not leaked
        EXPECT_EQ(state.x509Live, 1);
        p.setRootCA(nullptr);
        EXPECT_EQ(state.x509Live, 0);
        p.setRootCA(ROOT_CA);
    }
    EXPECT_EQ(state.x509Live, 0);  // freed by destructor
}

TEST(esp8266_restart_sequence) {
    Platform p;
    state.responses.push_back(body(10));
    p.startOTA(URL, "", "");
    EXPECT_EQ(state.delays, (std::vector<unsigned long>{500, 500, 200, 2000}));
    EXPECT_EQ(state.wifiDisconnectCount, 1);
    EXPECT_EQ(state.restartCount, 1);
    EXPECT_EQ(state.resetCount, 1);
    EXPECT_TRUE(state.lastEndEvenIfRemaining);
}

TEST(esp8266_real_system_epoch_hook_compiles) {
    ESP8266OtaPlatform real;  // uses time(nullptr); host clock is set
    state.responses.push_back(body(10));
    real.setRootCA(ROOT_CA);
    EXPECT_TRUE(real.startOTA(URL, "", ""));
    EXPECT_TRUE(state.http.back().x509Time > (long)ESP8266OtaPlatform::MIN_VALID_EPOCH);
}

#endif
