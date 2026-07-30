FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG VCPKG_COMMIT=8e8dfb4ba483886936ded5ca201b500b8d8b0096

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential bison ca-certificates cmake curl flex git \
       ninja-build python3 \
       pkg-config tar unzip zip \
    && rm -rf /var/lib/apt/lists/*

COPY server/cpp_gateway/netfree-ca-bundle.crt \
    /usr/local/share/ca-certificates/netfree-ca-bundle.crt
RUN cat /usr/local/share/ca-certificates/netfree-ca-bundle.crt \
    >> /etc/ssl/certs/ca-certificates.crt

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && git -C /opt/vcpkg checkout "${VCPKG_COMMIT}" \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /src/server/cpp_gateway
COPY server/cpp_gateway/vcpkg.json ./
RUN /opt/vcpkg/vcpkg install \
    --triplet x64-linux \
    --x-manifest-root=/src/server/cpp_gateway \
    --x-install-root=/src/server/cpp_gateway/vcpkg_installed

COPY shared/cpp/include /src/shared/cpp/include
COPY server/cpp_gateway /src/server/cpp_gateway
RUN cmake -S /src/server/cpp_gateway -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux \
    -DVCPKG_INSTALLED_DIR=/src/server/cpp_gateway/vcpkg_installed \
    && cmake --build /build --target ctd_cpp_gateway \
    && ldd /build/ctd_cpp_gateway | tee /build/ldd.txt \
    && ! grep -q "not found" /build/ldd.txt

FROM ubuntu:24.04 AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/ctd_cpp_gateway /usr/local/bin/ctd_cpp_gateway
RUN ldd /usr/local/bin/ctd_cpp_gateway | tee /tmp/ldd.txt \
    && ! grep -q "not found" /tmp/ldd.txt \
    && rm /tmp/ldd.txt

EXPOSE 8000
CMD ["ctd_cpp_gateway"]
