FROM ubuntu:latest as builder
WORKDIR /app

RUN apt update
RUN apt install -y g++ git cmake libpq-dev libboost-dev libboost-system-dev libboost-filesystem-dev libcurl4-openssl-dev

# AMQP-CPP
RUN git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git && \
    cd AMQP-CPP && \
    cmake -B build-ampq -DCMAKE_BUILD_TYPE=Release -DAMQP-CPP_BUILD_SHARED=OFF -DAMQP-CPP_LINUX_TCP=ON && \
    cmake --build build-ampq --target install

RUN git clone --recurse-submodules https://github.com/jupp0r/prometheus-cpp.git && \
    cd prometheus-cpp && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DENABLE_TESTING=OFF && \
    cmake --build build --target install

COPY src src
COPY CMakeLists.txt main.cpp thread_config.h ./
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

FROM ubuntu:latest
RUN apt update && apt install -y libpq-dev
WORKDIR /app

COPY .env ./
COPY --from=builder /app/build/credit_service ./

CMD ["./credit_service"]
