# Self-contained environment for the host-side unit tests.
# Build from the repository root:
#   docker build -f test/Dockerfile -t quarkifi-ota-tests .
#   docker run --rm quarkifi-ota-tests                 # all tests
#   docker run --rm quarkifi-ota-tests make sanitize   # with ASan/UBSan
# Or simply: ./test/run-tests.sh [--all|--esp32|--esp8266|--sanitize]

FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /lib
COPY src/  src/
COPY test/ test/

WORKDIR /lib/test
CMD ["make"]
