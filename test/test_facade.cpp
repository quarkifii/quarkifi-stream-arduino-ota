// Tests for the public QuarkifiStreamArduinoOTA facade.
#include "TestHelpers.h"
#include "QuarkifiStreamOTA.h"

static const char *ROOT_CA =
    "-----BEGIN CERTIFICATE-----\nMIIBroot\n-----END CERTIFICATE-----\n";

TEST(facade_buildAuth_known_vectors) {
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("user", "pass").str(),
              std::string("Basic dXNlcjpwYXNz"));
    // RFC 7617 example
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("Aladdin", "open sesame").str(),
              std::string("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="));
}

TEST(facade_buildAuth_padding_variants) {
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("a", "b").str(), std::string("Basic YTpi"));
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("ab", "c").str(), std::string("Basic YWI6Yw=="));
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("ab", "cd").str(), std::string("Basic YWI6Y2Q="));
}

TEST(facade_buildAuth_long_credentials_single_line) {
    String user = "a-really-long-user-name-for-testing-base64";
    String pass = "and-an-equally-long-password-value-0123456789";
    String auth = QuarkifiStreamArduinoOTA::buildAuth(user, pass);
    EXPECT_TRUE(auth.startsWith("Basic "));
    EXPECT_EQ(auth.indexOf('\n'), -1);
    EXPECT_EQ(auth.length(), 6u + 4u * ((user.length() + 1 + pass.length() + 2) / 3));
}

TEST(facade_buildAuth_empty_username_returns_empty) {
    EXPECT_TRUE(QuarkifiStreamArduinoOTA::buildAuth("", "pass").isEmpty());
    EXPECT_TRUE(QuarkifiStreamArduinoOTA::buildAuth("", "").isEmpty());
}

TEST(facade_buildAuth_empty_password_allowed) {
    // RFC 7617 allows an empty password ("user:").
    EXPECT_EQ(QuarkifiStreamArduinoOTA::buildAuth("user", "").str(), std::string("Basic dXNlcjo="));
}

TEST(facade_not_copyable) {
    EXPECT_FALSE(std::is_copy_constructible<QuarkifiStreamArduinoOTA>::value);
    EXPECT_FALSE(std::is_copy_assignable<QuarkifiStreamArduinoOTA>::value);
}

TEST(facade_startOTA_without_wifi) {
    QuarkifiStreamArduinoOTA ota;
    ota.setCallback(th::record);
    mock::state.wifiStatus = WL_DISCONNECTED;
    EXPECT_FALSE(ota.startOTA("https://example.com/fw.bin"));
    EXPECT_EQ(th::statuses(), (std::vector<int>{OtaStatus::ERR_NO_WIFI}));
    EXPECT_EQ(mock::state.http.size(), (size_t)0);
}

TEST(facade_startOTA_forwards_url_and_auth) {
    QuarkifiStreamArduinoOTA ota;
    ota.setCallback(th::record);
    mock::state.responses.push_back(mock::okResponse(100));
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin", "Basic abc"));
    ASSERT_TRUE(mock::state.http.size() == 1);
    EXPECT_EQ(mock::state.http[0].url, std::string("https://example.com/fw.bin"));
    EXPECT_EQ(mock::state.http[0].headers["Authorization"], std::string("Basic abc"));
    EXPECT_EQ(mock::state.restartCount, 1);
}

TEST(facade_startOTA_default_auth_sends_no_header) {
    QuarkifiStreamArduinoOTA ota;
    mock::state.responses.push_back(mock::okResponse(100));
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin"));
    ASSERT_TRUE(mock::state.http.size() == 1);
    EXPECT_EQ(mock::state.http[0].headers.count("Authorization"), (size_t)0);
}

TEST(facade_startOTA_forwards_md5) {
    QuarkifiStreamArduinoOTA ota;
    mock::state.responses.push_back(mock::okResponse(100));
    mock::state.imageMd5 = "0123456789abcdef0123456789abcdef";
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin", "", "0123456789abcdef0123456789abcdef"));
    EXPECT_EQ(mock::state.updateExpectedMd5, std::string("0123456789abcdef0123456789abcdef"));
}

TEST(facade_setRootCA_forwards_to_platform) {
    QuarkifiStreamArduinoOTA ota;
    ota.setRootCA(ROOT_CA);
    mock::state.responses.push_back(mock::okResponse(100));
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin"));
    ASSERT_TRUE(mock::state.http.size() >= 1);
    const mock::HttpRecord &h = mock::state.http.back();
    EXPECT_FALSE(h.insecure);
#if defined(ESP32)
    EXPECT_EQ(h.caCert, std::string(ROOT_CA));
#elif defined(ESP8266)
    EXPECT_TRUE(h.trustAnchors != nullptr);
#endif
}

TEST(facade_setRootCA_null_reverts_to_insecure) {
    QuarkifiStreamArduinoOTA ota;
    ota.setRootCA(ROOT_CA);
    ota.setRootCA(nullptr);
    mock::state.responses.push_back(mock::okResponse(100));
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin"));
    EXPECT_TRUE(mock::state.http.back().insecure);
}

TEST(facade_works_without_callback) {
    QuarkifiStreamArduinoOTA ota;
    mock::state.responses.push_back(mock::okResponse(100));
    EXPECT_TRUE(ota.startOTA("https://example.com/fw.bin"));
}
