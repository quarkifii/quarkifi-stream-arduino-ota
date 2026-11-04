#!/bin/sh
# Run the unit tests in Docker — only Docker needs to be installed.
#
#   ./test/run-tests.sh --all        # ESP32 + ESP8266 tests (default)
#   ./test/run-tests.sh --esp32      # ESP32 only
#   ./test/run-tests.sh --esp8266    # ESP8266 only
#   ./test/run-tests.sh --sanitize   # all tests with AddressSanitizer + UBSan
#
# Exit code is non-zero if any test fails.
set -eu

usage() {
    sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
}

case "${1:---all}" in
    --all)      TARGET=all ;;
    --esp32)    TARGET=esp32 ;;
    --esp8266)  TARGET=esp8266 ;;
    --sanitize) TARGET=sanitize ;;
    -h|--help)  usage; exit 0 ;;
    *)          echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
esac

if [ $# -gt 1 ]; then
    echo "Only one option is allowed." >&2
    usage >&2
    exit 2
fi

IMAGE=quarkifi-ota-tests
ROOT=$(cd "$(dirname "$0")/.." && pwd)

docker build --quiet -f "$ROOT/test/Dockerfile" -t "$IMAGE" "$ROOT" >/dev/null
exec docker run --rm "$IMAGE" make "$TARGET"
