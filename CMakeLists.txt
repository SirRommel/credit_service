cmake_minimum_required(VERSION 3.30)
project(credit_service LANGUAGES CXX)

# Установка флагов компиляции
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# Нахождение необходимых библиотек
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(PostgreSQL REQUIRED) # PostgreSQL

if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR "PostgreSQL not found")
endif()

# Поиск заголовочных файлов AMQP-CPP
find_path(AMQP_CPP_INCLUDE_DIR NAMES amqpcpp.h PATHS /usr/local/include /usr/include)
if(NOT AMQP_CPP_INCLUDE_DIR)
    message(FATAL_ERROR "AMQP-CPP headers not found")
endif()

# Поиск библиотеки AMQP-CPP
find_library(AMQP_CPP_LIBRARY NAMES amqpcpp PATHS /usr/local/lib /usr/lib)
if(NOT AMQP_CPP_LIBRARY)
    message(FATAL_ERROR "AMQP-CPP library not found")
endif()


# Добавление исполняемого файла
add_executable(credit_service
        main.cpp
#        src/core/task_queue.h
#        src/core/task_queue.cpp
#        src/core/routers/headers/http_router.h
#        src/core/routers/http_router.cpp
#        src/config/env_config.h
#        src/config/env_config.cpp
        src/tools/json_tools.h
        src/tools/json_tools.cpp
        src/tools/utils.h
        src/tools/utils.cpp
        src/app.h
        src/app.cpp
        src/handlers/endpoint_handler.h
        src/handlers/time_endpoint.h
        src/groups/time_group.h
        src/groups/endpoint_group.h
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

)



# Указание путей к заголовочным файлам
target_include_directories(credit_service PRIVATE
        ${Boost_INCLUDE_DIRS}
        ${PostgreSQL_INCLUDE_DIRS}
        ${AMQP_CPP_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src  # Путь к модулям
)

# Связывание библиотек
target_link_libraries(credit_service PRIVATE
        ${Boost_LIBRARIES}
        ${PostgreSQL_LIBRARIES}
        ${AMQP_CPP_LIBRARY}
        pthread
        ssl
        crypto
)

# Вывод информации о PostgreSQL
message(STATUS "PostgreSQL include dir: ${PostgreSQL_INCLUDE_DIRS}")
message(STATUS "PostgreSQL libraries: ${PostgreSQL_LIBRARIES}")