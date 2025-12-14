FROM ubuntu:22.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN ./scripts/vp.sh build native --build-dir /app/build/native

CMD ["/app/build/native/pipeline_cli", "--help"]
