FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    cmake \
    ninja-build \
    build-essential \
    cppcheck \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
