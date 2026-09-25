FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk/arm64:3.0.20250408.124536 AS sdk

USER root
ARG CPP_COMPILER=clang++

# Valve Sniper SDK's default Clang predates the C++20 features used by xrlib glTF dependencies
RUN if [ "${CPP_COMPILER}" = "clang++" ]; then \
        apt-get update \
        && apt-get install -y --no-install-recommends clang-16 \
        && ln -s /usr/bin/clang++-16 /usr/local/bin/clang++ \
        && rm -rf /var/lib/apt/lists/*; \
    fi

FROM sdk AS build
WORKDIR /workspace

COPY CMakeLists.txt project_config.h.in LICENSE ./
COPY include/ include/
COPY src/ src/
COPY res/ res/
COPY third_party/ third_party/

ARG BUILD_TYPE=Release
ARG BUILD_AS_STATIC=OFF
ARG ENABLE_XRVK=ON

RUN test "$(uname -m)" = aarch64 \
    && cmake -S . -B build \
        -DCMAKE_CXX_COMPILER="${CPP_COMPILER}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DBUILD_SHADERS=OFF \
        -DBUILD_AS_STATIC="${BUILD_AS_STATIC}" \
        -DENABLE_XRVK="${ENABLE_XRVK}" \
        -DXRLIB_LIB_OUT=/output/lib \
        -DXRLIB_BIN_OUT=/output/bin \
    && cmake --build build --parallel 4

FROM scratch AS libraries
COPY --from=build /output/lib/ /
