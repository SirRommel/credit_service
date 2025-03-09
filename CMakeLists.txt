cmake_minimum_required(VERSION 3.30)
project(credit_service LANGUAGES CXX)

# Установка флагов компиляции
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
file(COPY ${CMAKE_SOURCE_DIR}/.env DESTINATION ${CMAKE_BINARY_DIR})

# Нахождение необходимых библиотек
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(PostgreSQL REQUIRED)

if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR "PostgreSQL not found")
endif()

# Поиск заголовочных файлов AMQP-CPP
set(AMQP_CPP_INCLUDE_DIR "/usr/local/include")
set(AMQP_CPP_LIBRARY "/usr/local/lib/libamqpcpp.so")

include_directories(${Boost_INCLUDE_DIRS} ${PostgreSQL_INCLUDE_DIRS} ${AMQP_CPP_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
link_directories(${Boost_LIBRARY_DIRS} ${PostgreSQL_LIBRARY_DIRS} /usr/local/lib)

# Добавление исполняемого файла
add_executable(credit_service
        main.cpp
        src/tools/json_tools.h
        src/tools/json_tools.cpp
        src/tools/utils.h
        src/tools/utils.cpp
        src/app.h
        src/app.cpp
        src/errors/http_errors.h
        src/errors/http_errors.cpp
        src/endpoints/endpoint.h
        src/endpoints/time_endpoint.h
        src/endpoints/time_endpoint.cpp
        src/db/database_manager.h
        src/db/database_manager.cpp
        src/endpoints/db_test_endpoint.h
        src/models/db_models/base_db_model.h
        src/models/db_models/tariff_model.h
        src/db/database_initializer.h
        src/endpoints/tariff_endpoint.h
        src/endpoints/tariff_endpoint.cpp
        src/models/db_models/credit_history_model.h
        src/endpoints/credit_limit_endpoint.h
        src/endpoints/credit_limit_endpoint.cpp
        src/rabbit_mq/rabbitmq_manager.h
        src/rabbit_mq/rabbitmq_manager.cpp
        src/endpoints/test_rabbit_endpoint.h
        src/endpoints/test_rabbit_endpoint.cpp
        src/models/db_models/credit_model.h
        src/endpoints/credit_endpoint.h
        src/endpoints/credit_endpoint.cpp
        src/rabbit_mq/rabbit_db_operations.cpp

)



target_include_directories(credit_service PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${PostgreSQL_INCLUDE_DIRS}
        ${AMQP_CPP_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Линковка библиотек
target_link_libraries(credit_service PRIVATE
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${AMQP_CPP_LIBRARY}
        pthread
        dl
        ssl
        crypto
)

# Исправление несоответствия типов
target_compile_definitions(credit_service PRIVATE
        BOOST_BIND_GLOBAL_PLACEHOLDERS  # Для совместимости с Boost.Bind
)