// Tests for CommonOtaPlatform: callback, state, validation, progress reporting.
#include "TestHelpers.h"
#include "common/CommonOtaPlatform.h"

namespace {

// Exposes CommonOtaPlatform's protected API with trivial platform hooks.
class TestablePlatform : public CommonOtaPlatform {
public:
    using CommonOtaPlatform::publishStatus;
    using CommonOtaPlatform::reportProgress;
    using CommonOtaPlatform::resetProgress;
    void setState(OtaState s) { _state = s; }
    int maxRetries() const { return _maxRetries; }
    int timeoutMs() const { return _timeoutMs; }
    const String &rootCA() const { return _rootCA; }
    int restarts = 0;
    int rootChanges = 0;

protected:
    Attempt configureTls(WiFiClientSecure &c, const String &) override {
        c.setInsecure();
        return Attempt::OK;
    }
    bool finalizeUpdate() override { return Update.end(); }
    void restartDevice() override { restarts++; }
    void onRootCAChanged() override { rootChanges++; }
};

}  // namespace

TEST(common_defaults) {
    TestablePlatform p;
    EXPECT_EQ((int)p.getState(), (int)OtaState::IDLE);
    EXPECT_FALSE(p.isBusy());
    EXPECT_EQ(p.maxRetries(), 3);
    EXPECT_EQ(p.timeoutMs(), 20000);
    EXPECT_EQ((int)CommonOtaPlatform::BUFFER_SIZE, 1024);
    EXPECT_TRUE(p.rootCA().isEmpty());
}

TEST(common_not_copyable) {
    EXPECT_FALSE(std::is_copy_constructible<TestablePlatform>::value);
    EXPECT_FALSE(std::is_copy_assignable<TestablePlatform>::value);
}

TEST(common_publishStatus_without_callback_is_noop) {
    TestablePlatform p;
    p.publishStatus(OtaStatus::STARTING);  // must not crash
    EXPECT_EQ(th::events().size(), (size_t)0);
}

TEST(common_publishStatus_invokes_callback_with_default_progress_zero) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.publishStatus(OtaStatus::ERR_HTTP);
    p.publishStatus(OtaStatus::DOWNLOADING, 42);
    ASSERT_TRUE(th::events().size() == 2);
    EXPECT_EQ(th::events()[0], (th::Event{OtaStatus::ERR_HTTP, 0}));
    EXPECT_EQ(th::events()[1], (th::Event{OtaStatus::DOWNLOADING, 42}));
}

TEST(common_setCallback_replaces_and_clears) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.setCallback(nullptr);
    p.publishStatus(OtaStatus::STARTING);
    EXPECT_EQ(th::events().size(), (size_t)0);
}

TEST(common_setRootCA_copies_and_clears) {
    TestablePlatform p;
    char pem[] = "-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n";
    p.setRootCA(pem);
    pem[0] = 'X';  // caller's buffer may change/disappear
    EXPECT_TRUE(p.rootCA().startsWith("-----BEGIN"));
    p.setRootCA(nullptr);
    EXPECT_TRUE(p.rootCA().isEmpty());
    p.setRootCA("");
    EXPECT_TRUE(p.rootCA().isEmpty());
    EXPECT_EQ(p.rootChanges, 3);
}

TEST(common_isBusy_per_state) {
    TestablePlatform p;
    p.setState(OtaState::IDLE);        EXPECT_FALSE(p.isBusy());
    p.setState(OtaState::DOWNLOADING); EXPECT_TRUE(p.isBusy());
    p.setState(OtaState::UPDATING);    EXPECT_TRUE(p.isBusy());
    p.setState(OtaState::SUCCESS);     EXPECT_FALSE(p.isBusy());
    p.setState(OtaState::FAILED);      EXPECT_FALSE(p.isBusy());
}

TEST(common_isValidMd5) {
    EXPECT_TRUE(CommonOtaPlatform::isValidMd5("d41d8cd98f00b204e9800998ecf8427e"));
    EXPECT_TRUE(CommonOtaPlatform::isValidMd5("D41D8CD98F00B204E9800998ECF8427E"));
    EXPECT_FALSE(CommonOtaPlatform::isValidMd5(""));
    EXPECT_FALSE(CommonOtaPlatform::isValidMd5("d41d8cd98f00b204e9800998ecf8427"));    // 31
    EXPECT_FALSE(CommonOtaPlatform::isValidMd5("d41d8cd98f00b204e9800998ecf8427e0"));  // 33
    EXPECT_FALSE(CommonOtaPlatform::isValidMd5("g41d8cd98f00b204e9800998ecf8427e"));   // non-hex
}

TEST(common_isPermanentHttpError) {
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(HTTPC_ERROR_CONNECTION_REFUSED));
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(-11));
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(500));
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(503));
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(408));
    EXPECT_FALSE(CommonOtaPlatform::isPermanentHttpError(429));
    EXPECT_TRUE(CommonOtaPlatform::isPermanentHttpError(301));  // unfollowable redirect
    EXPECT_TRUE(CommonOtaPlatform::isPermanentHttpError(400));
    EXPECT_TRUE(CommonOtaPlatform::isPermanentHttpError(401));
    EXPECT_TRUE(CommonOtaPlatform::isPermanentHttpError(403));
    EXPECT_TRUE(CommonOtaPlatform::isPermanentHttpError(404));
}

// ---------------------------------------------------------------- progress

TEST(common_reportProgress_ignores_zero_total) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.reportProgress(10, 0);  // would divide by zero otherwise
    EXPECT_EQ(th::events().size(), (size_t)0);
}

TEST(common_reportProgress_first_report_immediate_even_at_boot) {
    TestablePlatform p;
    p.setCallback(th::record);
    mock::state.nowMs = 200;  // device booted 200 ms ago
    p.reportProgress(0, 1000);
    EXPECT_EQ(th::progressValues(), (std::vector<int>{0}));
}

TEST(common_reportProgress_throttles_between_first_and_last) {
    TestablePlatform p;
    p.setCallback(th::record);
    mock::state.nowMs = 1000;
    p.reportProgress(0, 1000);     // 0   (first — always)
    mock::state.nowMs = 1500;
    p.reportProgress(500, 1000);   // throttled (<1 s)
    mock::state.nowMs = 2000;
    p.reportProgress(600, 1000);   // 60
    mock::state.nowMs = 2100;
    p.reportProgress(1000, 1000);  // 100 (final — always, even within 1 s)
    EXPECT_EQ(th::progressValues(), (std::vector<int>{0, 60, 100}));
}

TEST(common_reportProgress_deduplicates_same_percent) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.reportProgress(0, 100000);
    mock::state.nowMs = 5000;
    p.reportProgress(10, 100000);   // still 0%
    EXPECT_EQ(th::progressValues(), (std::vector<int>{0}));
}

TEST(common_reportProgress_clamps_overshoot) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.reportProgress(2000, 1000);
    EXPECT_EQ(th::progressValues(), (std::vector<int>{100}));
}

TEST(common_reportProgress_large_image_no_overflow) {
    TestablePlatform p;
    p.setCallback(th::record);
    // 16 MB image (max ESP32 flash) — written*100 must not overflow 32-bit.
    p.reportProgress(8u * 1024 * 1024, 16u * 1024 * 1024);
    EXPECT_EQ(th::progressValues(), (std::vector<int>{50}));
}

TEST(common_resetProgress_restarts_sequence) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.reportProgress(0, 100);
    p.reportProgress(100, 100);
    p.resetProgress();
    p.reportProgress(0, 100);  // next attempt starts at 0 again, immediately
    EXPECT_EQ(th::progressValues(), (std::vector<int>{0, 100, 0}));
}

// ---------------------------------------------------------------- startOTA validation

TEST(common_rejects_bad_urls) {
    TestablePlatform p;
    p.setCallback(th::record);
    EXPECT_FALSE(p.startOTA("", "", ""));
    EXPECT_FALSE(p.startOTA("ftp://example.com/fw.bin", "", ""));
    EXPECT_FALSE(p.startOTA("example.com/fw.bin", "", ""));
    EXPECT_EQ(th::statuses(), (std::vector<int>{OtaStatus::ERR_BAD_ARGS, OtaStatus::ERR_BAD_ARGS,
                                                OtaStatus::ERR_BAD_ARGS}));
    EXPECT_EQ(mock::state.http.size(), (size_t)0);
}

TEST(common_rejects_bad_md5) {
    TestablePlatform p;
    p.setCallback(th::record);
    EXPECT_FALSE(p.startOTA("https://example.com/fw.bin", "", "not-an-md5"));
    EXPECT_EQ(th::statuses(), (std::vector<int>{OtaStatus::ERR_BAD_ARGS}));
    EXPECT_EQ(mock::state.http.size(), (size_t)0);
}

TEST(common_rejects_http_when_root_ca_set) {
    TestablePlatform p;
    p.setCallback(th::record);
    p.setRootCA("-----BEGIN CERTIFICATE-----\nAAAA\n-----END CERTIFICATE-----\n");
    EXPECT_FALSE(p.startOTA("http://example.com/fw.bin", "", ""));
    EXPECT_EQ(th::statuses(), (std::vector<int>{OtaStatus::ERR_BAD_ARGS}));
    EXPECT_EQ(mock::state.http.size(), (size_t)0);
}

TEST(common_validation_precedes_wifi_check) {
    TestablePlatform p;
    p.setCallback(th::record);
    mock::state.wifiStatus = WL_DISCONNECTED;
    p.startOTA("bogus", "", "");
    EXPECT_EQ(th::statuses(), (std::vector<int>{OtaStatus::ERR_BAD_ARGS}));
}
