# Стадия 1: Сборка
FROM ubuntu:22.04 AS builder

WORKDIR /app

# Установка зависимостей с добавлением CMake
RUN apt update && apt install -y \
    g++ \
    cmake \
    libboost1.74-all-dev \
    libpq-dev \
    libssl-dev \
    git \
    make \
    libev-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.sh && \
    chmod +x cmake-3.30.0-linux-x86_64.sh && \
    ./cmake-3.30.0-linux-x86_64.sh --prefix=/usr/local --exclude-subdir --skip-license

# Сборка AMQP-CPP (только один раз)
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
    cd AMQP-CPP && \
    cmake . -DAMQP-CPP_BUILD_SHARED=ON -DAMQP-CPP_LINUX_TCP=ON && \
    make && make install && \
    ls -l /usr/local/include/amqpcpp.h && \
    ls -l /usr/local/lib/libamqpcpp*

# Копирование исходников проекта
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Стадия 2: Финальный образ
#FROM ubuntu:22.04
#
#WORKDIR /app
#
#
## Установка runtime-зависимостей
#RUN apt update && apt install -y \
#    libpq5 \
#    libssl3 \
#    libboost1.74-all-dev \
#    libev4 \
#    && rm -rf /var/lib/apt/lists/*
#
#COPY --from=builder /app/build/credit_service .
COPY .env /app/build

ENTRYPOINT ["./build/credit_service"]