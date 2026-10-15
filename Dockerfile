# UltimateCraft in a container: builds the Linux binary from source and runs the game headless
# (software OpenGL on a virtual display). Default command runs the smoke test script.
#   docker run --rm ghcr.io/rajdipdhar2007/ultimatecraft
#   docker run --rm ghcr.io/rajdipdhar2007/ultimatecraft tests/redstone.txt
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN make -j"$(nproc)"

FROM ubuntu:24.04
LABEL org.opencontainers.image.source="https://github.com/RajdipDhar2007/UltimateCraft" \
      org.opencontainers.image.description="UltimateCraft - a voxel survival sandbox in C, by RAJDIP DHAR (headless build)" \
      org.opencontainers.image.licenses="LicenseRef-UltimateCraft"
RUN apt-get update && apt-get install -y --no-install-recommends xvfb libgl1-mesa-dri libgl1 libx11-6 libxrandr2 \
    libxinerama1 libxcursor1 libxi6 && rm -rf /var/lib/apt/lists/*
WORKDIR /game
COPY --from=build /src/ultimatecraft .
COPY res ./res
COPY tests ./tests
COPY datapacks ./datapacks
ENV LIBGL_ALWAYS_SOFTWARE=1
ENTRYPOINT ["xvfb-run","-a","-s","-screen 0 1280x720x24","./ultimatecraft","--no-audio","--script"]
CMD ["tests/smoke.txt"]
