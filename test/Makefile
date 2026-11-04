# Host-side unit tests for QuarkifiStreamOTA using Arduino mocks.
#
#   make            # build + run ESP32 and ESP8266 test binaries
#   make esp32      # ESP32 only
#   make esp8266    # ESP8266 only
#   make sanitize   # rebuild + run with AddressSanitizer/UBSan
#   make clean
#
# No toolchain installed? Use ./run-tests.sh (Docker) [--all|--esp32|--esp8266|--sanitize] instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g -O0
override CXXFLAGS += -I. -Imocks -Iframework -I../src

SRC   := $(wildcard ../src/*.cpp ../src/*/*.cpp)
MOCKS := mocks/MockState.cpp framework/TestMain.cpp
TESTS := $(wildcard test_*.cpp)
DEPS  := $(wildcard ../src/*.h ../src/*/*.h mocks/*.h mocks/*/*.h framework/*.h *.h)

BUILD := build

.PHONY: all test esp32 esp8266 sanitize clean
all test: esp32 esp8266

$(BUILD)/test_esp32: $(SRC) $(MOCKS) $(TESTS) $(DEPS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DESP32 -DPLATFORM_NAME='"ESP32"' $(SRC) $(MOCKS) $(TESTS) -o $@

$(BUILD)/test_esp8266: $(SRC) $(MOCKS) $(TESTS) $(DEPS)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -DESP8266 -DPLATFORM_NAME='"ESP8266"' $(SRC) $(MOCKS) $(TESTS) -o $@

esp32: $(BUILD)/test_esp32
	./$(BUILD)/test_esp32

esp8266: $(BUILD)/test_esp8266
	./$(BUILD)/test_esp8266

SANFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g -O1 -fno-omit-frame-pointer \
            -fsanitize=address,undefined -fno-sanitize-recover=all

sanitize:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="$(SANFLAGS)" all

clean:
	rm -rf $(BUILD)
