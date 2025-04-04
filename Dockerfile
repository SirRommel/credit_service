# Стадия 1: Сборка
# Стадия 1: Сборка
FROM alpine:latest AS builder

WORKDIR /app

# Установка зависимостей
RUN apk add --no-cache \
    autoconf \
    automake \
    build-base \
    linux-headers \
    g++ \
    cmake \
    make \
    git \
    openssl-dev \
    postgresql-dev \
    libpq \
    libev-dev \
    wget \
    curl \
    python3 \
    ninja \
    bash \
    tar \
    unzip
# Сборка Ninja 1.12.1
RUN wget https://github.com/ninja-build/ninja/archive/refs/tags/v1.12.1.zip && \
    unzip v1.12.1.zip && \
    cd ninja-1.12.1 && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build && \
    mv build/ninja /usr/bin/ninja && \
    cd .. && rm -rf ninja-1.12.1 v1.12.1.zip

# Установка vcpkg (с обновлением)
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg

WORKDIR /opt/vcpkg

# Компилируем vcpkg (используется скрипт bootstrap)
RUN ./bootstrap-vcpkg.sh

# Устанавливаем необходимые библиотеки Boost через vcpkg
RUN ./vcpkg install boost-asio boost-beast boost-property-tree boost-algorithm boost-uuid
# Пути к vcpkg
ENV VCPKG_ROOT=/app/vcpkg
ENV CMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake

WORKDIR /app
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
    cd AMQP-CPP && \
    cmake -G Ninja . \
          -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
          -DAMQP-CPP_BUILD_SHARED=ON \
          -DAMQP-CPP_LINUX_TCP=ON && \
    ninja && ninja install && \
    ls -l /usr/local/include/amqpcpp.h && ls -l /usr/local/lib/libamqpcpp*


# Стадия 2: Финальный образ
FROM alpine:latest

WORKDIR /app

# Установка runtime-зависимостей
RUN apk add --no-cache \
    libstdc++ \
    libssl3 \
    libpq \
    libev \
    && rm -rf /var/cache/apk/*

# Копирование артефактов из builder
COPY --from=builder /usr/local/lib/libamqpcpp* /usr/lib/
COPY --from=builder /app/build/credit_service .

ENTRYPOINT ["./credit_service"]