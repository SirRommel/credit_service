# Стадия 1: Сборка
FROM ubuntu:22.04 AS builder

WORKDIR /app

# Установка зависимостей с правильными версиями Boost
RUN apt update && apt install -y \
    g++ \
    libboost1.74-all-dev \
    libpq-dev \
    libssl-dev \
    git \
    make \
    libev-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.sh \
    && chmod +x cmake-3.30.0-linux-x86_64.sh \
    && ./cmake-3.30.0-linux-x86_64.sh --prefix=/usr/local --exclude-subdir \
    && rm cmake-3.30.0-linux-x86_64.sh


# Сборка AMQP-CPP
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
WORKDIR /app/AMQP-CPP
RUN cmake . && make && make install

# Копирование исходников проекта
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Стадия 2: Финальный образ
FROM ubuntu:22.04

WORKDIR /app

# Установка runtime-зависимостей
RUN apt update && apt install -y \
    libpq5 \
    libssl3 \
    libboost1.74-all-dev \
    libev4 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/credit_service .
COPY .env .

ENTRYPOINT ["./credit_service"]