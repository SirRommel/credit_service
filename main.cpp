// #include "core/routers/headers/http_router.h"
//
// #include "core/routers/rabbit_mq_handler.h"
//
// #include "rabbit_mq/rabbit_mq_worker.h"
// #include "config/env_config.h"
// #include <boost/asio.hpp>
// #include <iostream>
// #include <vector>
// #include <memory>
// #include <amqpcpp/libboostasio.h>
// #include <thread>
//
// int main() {
//     try {
//         // Чтение конфигурации из .env файла
//         std::map<std::string, std::string> env = read_env_file(".env");
//         if (env.empty()) {
//             std::cerr << "Error: Failed to load configuration from .env file." << std::endl;
//             return 1;
//         }
//         std::cout << env.at("HOST");
//
//         // Извлечение параметров
//         const std::string host = env.at("HOST");
//         const uint16_t port = std::stoi(env.at("PORT"));
//
//         // Создание экземпляра RabbitMQWorker
//         auto rabbitmq_worker = std::make_shared<RabbitMQWorker>(env);
//         rabbitmq_worker->start();
//
//         // Создание io_context для основного приложения
//         boost::asio::io_context app_io_context;
//
//         // Создаем HTTP-роутер
//         HttpRouter router(app_io_context, host, port, rabbitmq_worker);
//         router.start();
//
//         // Ожидание завершения работы
//         app_io_context.run();
//
//         // Остановка рабочего потока RabbitMQ
//         rabbitmq_worker->stop();
//     } catch (const std::exception &e) {
//         std::cerr << "Exception: " << e.what() << "\n";
//     }
//
//     return 0;
// }

// #include <iostream>
// #include <fstream>
// #include <sstream>
// #include <map>
// #include <boost/asio.hpp>
// #include <boost/beast/core/detail/base64.hpp>
// #include <boost/thread.hpp>
// #include <boost/property_tree/ptree.hpp>
// #include <boost/property_tree/json_parser.hpp>
// #include <libpq-fe.h> // Подключаем PostgreSQL C API
// #include <amqpcpp.h>
// #include <amqpcpp/libboostasio.h>
// #include <thread>
// #include <queue>
// #include <mutex>
// #include <condition_variable>
// #include "src/tools/utils.h"
// using boost::asio::ip::tcp;
// std::queue<std::string> message_queue;
// std::mutex queue_mutex;
// std::condition_variable cv;
//
// // Флаг завершения работы
// bool stop_worker = false;
// std::string clean_message(const std::string &message) {
//     std::string result;
//     for (char c : message) {
//         if (isprint(c) || isspace(c)) {  // Отбрасываем все непечатаемые символы, кроме пробела
//             result.push_back(c);
//         }
//     }
//     return result;
// }
// std::string encode_base64(const std::string &input) {
//     std::string encoded;
//     encoded.resize(boost::beast::detail::base64::encoded_size(input.size()));
//     boost::beast::detail::base64::encode(encoded.data(), input.data(), input.size());
//     return encoded;
// }
//
// void print_characters(const std::string &message) {
//     for (size_t i = 0; i < message.size(); ++i) {
//         char c = message[i];
//
//         // Специальные символы
//         if (c == ' ') {
//             std::cout << "Character: [SPACE]  Code: " << static_cast<int>(c) << std::endl;
//         } else if (c == '\n') {
//             std::cout << "Character: [NEWLINE]  Code: " << static_cast<int>(c) << std::endl;
//         } else if (c == '\t') {
//             std::cout << "Character: [TAB]  Code: " << static_cast<int>(c) << std::endl;
//         } else if (c == '\r') {
//             std::cout << "Character: [CARRIAGE RETURN]  Code: " << static_cast<int>(c) << std::endl;
//         } else {
//             std::cout << "Character: " << c << "  Code: " << static_cast<int>(c) << std::endl;
//         }
//     }
// }
//
// void send_message(const std::string &message, AMQP::TcpChannel &channel) {
//     std::cout << "Message to send: " << message << std::endl;
//
//     try {
//         // Создаём shared_ptr для сохранения объекта envelope
//         auto envelope = std::make_shared<AMQP::Envelope>(message.c_str(), message.size());
//
//         // Создаем объект AMQP::Table для заголовков (если нужно)
//         AMQP::Table headers;
//         headers["Content-Type"] = "application/json";  // Тип содержимого JSON
//         envelope->setHeaders(headers);
//         std::cout << "envelope " << envelope << std::endl;
//         print_characters(clean_message(message));
//         std::string new_message = clean_message(message);
//         // Ожидаем, когда очередь будет готова
//         channel.declareExchange("my_exchange", AMQP::direct, AMQP::durable).onSuccess([new_message, &channel]() {
//             try {
//                 // Привязываем очередь к обменнику с ключом маршрутизации "my_routing_key"
//                 channel.declareQueue("my_queue", AMQP::durable).onSuccess([new_message, &channel]() {
//                     // Привязываем очередь к обменнику
//                     channel.bindQueue("my_exchange", "my_queue", "my_routing_key").onSuccess([new_message, &channel]() {
//                         // Публикуем сообщение в обменник
//                         channel.publish("my_exchange", "my_routing_key", new_message);
//                         std::cout << "Message published successfully" << std::endl;
//                     });
//                 });
//             } catch (const std::exception &e) {
//                 std::cerr << "Error sending message: " << e.what() << std::endl;
//             }
//         }).onError([](const char* message) {
//             std::cerr << "Exchange declaration failed: " << message << std::endl;
//         });
//     } catch (const std::bad_alloc &e) {
//         std::cerr << "Memory allocation failed: " << e.what() << std::endl;
//     }
// }
//
// void rabbitmq_worker(std::map<std::string, std::string> env) {
//     try {
//         boost::asio::io_context rabbitmq_io_context;
//
//         // Создаем work_guard для поддержания работы io_context
//         boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(
//             rabbitmq_io_context.get_executor());
//
//         // Настройки подключения к RabbitMQ
//         std::string host = env.at("RABBITMQ_HOST");
//         int port = std::stoi(env.at("RABBITMQ_PORT"));
//         std::string user = env.at("RABBITMQ_USER");
//         std::string password = env.at("RABBITMQ_PASSWORD");
//         std::string vhost = env.at("RABBITMQ_VHOST");
//
//         boost::asio::ip::tcp::resolver resolver(rabbitmq_io_context);
//         boost::asio::ip::tcp::socket socket(rabbitmq_io_context);
//         boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
//
//         boost::asio::connect(socket, endpoints);
//         AMQP::LibBoostAsioHandler handler(rabbitmq_io_context);
//         AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + vhost));
//         AMQP::TcpChannel channel(&connection);
//
//         // Запускаем io_context в отдельном потоке
//         std::thread io_thread([&]() {
//             rabbitmq_io_context.run();
//         });
//
//         while (!stop_worker) {
//             std::unique_lock<std::mutex> lock(queue_mutex);
//             cv.wait(lock, [] { return !message_queue.empty() || stop_worker; });
//
//             if (stop_worker && message_queue.empty()) {
//                 break;
//             }
//
//             std::string message = message_queue.front();
//             message_queue.pop();
//             lock.unlock();
//
//             if (!message.empty()) {
//                 send_message(message, channel);
//             }
//         }
//
//         // Остановка io_context и закрытие соединения
//         rabbitmq_io_context.stop();
//         connection.close();
//         io_thread.join();
//     } catch (const std::exception &e) {
//         std::cerr << "RabbitMQ worker error: " << e.what() << std::endl;
//     }
// }
// void worker(std::map<std::string, std::string> env, boost::asio::io_context &io_context) {
//     try {
//         // Настройки подключения к RabbitMQ
//         std::string host = env.at("RABBITMQ_HOST");
//         int port = std::stoi(env.at("RABBITMQ_PORT"));
//         std::string user = env.at("RABBITMQ_USER");
//         std::string password = env.at("RABBITMQ_PASSWORD");
//         std::string vhost = env.at("RABBITMQ_VHOST");
//
//         boost::asio::ip::tcp::resolver resolver(io_context);
//         boost::asio::ip::tcp::socket socket(io_context);
//         boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
//
//         // Создание соединения
//         boost::asio::connect(socket, endpoints);
//         AMQP::LibBoostAsioHandler handler(io_context);
//         AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + vhost));
//         AMQP::TcpChannel channel(&connection);
//
//
//         while (!stop_worker) {
//             std::unique_lock<std::mutex> lock(queue_mutex);
//             cv.wait(lock, [] { return !message_queue.empty() || stop_worker; });
//
//             if (stop_worker && message_queue.empty()) {
//                 break;
//             }
//
//             std::string message = message_queue.front();
//             message_queue.pop();
//
//             lock.unlock();
//
//             if (!message.empty()) {
//                 send_message(message, channel);
//             }
//         }
//         io_context.run();
//         // Закрытие соединения
//         connection.close();
//     } catch (const std::exception &e) {
//         std::cerr << "Worker error: " << e.what() << std::endl;
//     }
// }
//
// // Функция для отправки сообщений в очередь
// void enqueue_message(const std::string &message) {
//     std::cout << message << std::endl;
//     std::lock_guard<std::mutex> lock(queue_mutex);
//     message_queue.push(message);
//     cv.notify_one();
// }
//
// // Функция для остановки рабочего потока
// void stop_worker_thread() {
//     {
//         std::lock_guard<std::mutex> lock(queue_mutex);
//         stop_worker = true;
//     }
//     cv.notify_all();
// }
// // Функция для отправки сообщения в RabbitMQ
// void send_to_rabbit(std::map<std::string, std::string> env, const std::string &message) {
//     boost::asio::io_context io_context;
//     // Настройки подключения к RabbitMQ
//     std::string host = env.at("RABBITMQ_HOST");
//     int port = std::stoi(env.at("RABBITMQ_PORT"));
//     std::string user = env.at("RABBITMQ_USER");
//     std::string password = env.at("RABBITMQ_PASSWORD");
//     std::string vhost = env.at("RABBITMQ_VHOST");
//
//     try {
//         // Создание соединения
//         boost::asio::ip::tcp::resolver resolver(io_context);
//         boost::asio::ip::tcp::socket socket(io_context);
//         boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
//
//         // Подключение к RabbitMQ
//         boost::asio::connect(socket, endpoints);
//         AMQP::LibBoostAsioHandler handler(io_context);
//         AMQP::TcpConnection connection(&handler, AMQP::Address("amqp://" + user + ":" + password + "@" + host + ":" + std::to_string(port) + "/" + vhost));
//         AMQP::TcpChannel channel(&connection);
//         // Отправка сообщения в очередь
//         send_message(message, channel);
//
//         // Запуск обработчика событий
//         std::thread io_thread([&]() {
//             io_context.run();
//         });
//
//         // Дожидаемся завершения потока
//         io_thread.join(); // Ожидание завершения потока
//     } catch (const std::exception &e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }
// }
//
//
// // Функция для получения текущего времени
// std::string get_current_time() {
//     std::time_t now = std::time(nullptr);
//     char buffer[80];
//     std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
//     return std::string(buffer);
// }
//
// // Функция для создания JSON-ответа
// std::string create_json_response(const std::string& key, const std::string& value) {
//     boost::property_tree::ptree root;
//     root.put(key, value);
//
//     std::ostringstream oss;
//     boost::property_tree::write_json(oss, root);
//     return oss.str();
// }
//
// // Функция для чтения .env файла
// std::map<std::string, std::string> read_env_file(const std::string& filename) {
//     std::map<std::string, std::string> env_map;
//     std::ifstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "Error: Could not open .env file." << std::endl;
//         return env_map;
//     }
//
//     std::string line;
//     while (std::getline(file, line)) {
//         size_t pos = line.find('=');
//         if (pos != std::string::npos) {
//             std::string key = line.substr(0, pos);
//             std::string value = line.substr(pos + 1);
//             env_map[key] = value;
//         }
//     }
//     file.close();
//     return env_map;
// }
//
// // Функция для подключения к PostgreSQL
// PGconn* connect_to_database(const std::map<std::string, std::string>& env) {
//     std::ostringstream conninfo;
//     conninfo << "host=" << env.at("POSTGRES_HOST") << " "
//              << "user=" << env.at("POSTGRES_USER") << " "
//              << "password=" << env.at("POSTGRES_PASSWORD") << " "
//              << "port=" << env.at("POSTGRES_PORT") << " "
//              << "dbname=" << env.at("POSTGRES_DB");
//
//     PGconn* conn = PQconnectdb(conninfo.str().c_str());
//
//     if (PQstatus(conn) != CONNECTION_OK) {
//         std::cerr << "Connection to database failed: " << PQerrorMessage(conn) << std::endl;
//         PQfinish(conn);
//         return nullptr;
//     }
//
//     return conn;
// }
//
// // Функция для создания базы данных, если она не существует
// bool create_database_if_not_exists(PGconn* conn, const std::string& dbname) {
//     PGresult* res = PQexec(conn, ("SELECT 1 FROM pg_database WHERE datname = '" + dbname + "'").c_str());
//     if (PQresultStatus(res) != PGRES_TUPLES_OK) {
//         std::cerr << "Failed to check database existence: " << PQerrorMessage(conn) << std::endl;
//         PQclear(res);
//         return false;
//     }
//
//     int rows = PQntuples(res);
//     if (rows == 0) {
//         PQclear(res);
//         res = PQexec(conn, ("CREATE DATABASE " + dbname).c_str());
//         if (PQresultStatus(res) != PGRES_COMMAND_OK) {
//             std::cerr << "Failed to create database: " << PQerrorMessage(conn) << std::endl;
//             PQclear(res);
//             return false;
//         }
//         std::cout << "Database '" << dbname << "' created successfully.\n";
//     } else {
//         std::cout << "Database '" << dbname << "' already exists.\n";
//     }
//
//     PQclear(res);
//     return true;
// }
//
// // Функция для создания таблицы, если она не существует
// bool create_table_if_not_exists(PGconn* conn) {
//     PGresult* res = PQexec(conn, "CREATE TABLE IF NOT EXISTS credit (user_id SERIAL PRIMARY KEY, summ NUMERIC NOT NULL)");
//     if (PQresultStatus(res) != PGRES_COMMAND_OK) {
//         std::cerr << "Failed to create table: " << PQerrorMessage(conn) << std::endl;
//         PQclear(res);
//         return false;
//     }
//
//     std::cout << "Table 'credit' is ready.\n";
//     PQclear(res);
//     return true;
// }
//
// // Функция для вставки данных в таблицу
// bool insert_into_credit(PGconn* conn, double summ) {
//     std::ostringstream query;
//     query << "INSERT INTO credit (summ) VALUES (" << summ << ")";
//
//     PGresult* res = PQexec(conn, query.str().c_str());
//     if (PQresultStatus(res) != PGRES_COMMAND_OK) {
//         std::cerr << "Failed to insert data: " << PQerrorMessage(conn) << std::endl;
//         PQclear(res);
//         return false;
//     }
//
//     std::cout << "Data inserted successfully.\n";
//     PQclear(res);
//     return true;
// }
//
// // Функция для получения данных из таблицы по ID
// std::string query_database(PGconn* conn, const std::string& id) {
//     std::ostringstream query;
//     query << "SELECT summ FROM credit WHERE user_id = " << id;
//
//     PGresult* res = PQexec(conn, query.str().c_str());
//     if (PQresultStatus(res) != PGRES_TUPLES_OK) {
//         std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
//         PQclear(res);
//         return "{}"; // Возвращаем пустой JSON в случае ошибки
//     }
//
//     int rows = PQntuples(res);
//     if (rows > 0) {
//         std::string result = PQgetvalue(res, 0, 0); // Получаем значение первого столбца первой строки
//         PQclear(res);
//         return result; // Возвращаем значение из базы данных
//     } else {
//         PQclear(res);
//         return "{}"; // Возвращаем пустой JSON, если запись не найдена
//     }
// }
//
// int main() {
//     try {
//         // Чтение .env файла
//         std::map<std::string, std::string> env = read_env_file(".env");
//         if (env.empty()) {
//             std::cerr << "Error: Failed to load configuration from .env file." << std::endl;
//             return 1;
//         }
//
//         // Извлечение параметров из .env
//         std::string host = env.at("HOST");
//         uint16_t port = std::stoi(env.at("PORT"));
//
//         // Создание первого io_service для RabbitMQ
//
//
//         // Запуск рабочего потока для RabbitMQ
//         std::thread rabbitmq_thread(rabbitmq_worker, std::ref(env));
//
//         // Создание второго io_service для основного приложения
//         boost::asio::io_context app_io_context;
//
//         // Создаем TCP-акцептор для прослушивания входящих подключений
//         tcp::acceptor acceptor(app_io_context, tcp::endpoint(boost::asio::ip::address::from_string(host), port));
//         std::cout << "Server started. Listening on " << host << ":" << port << "...\n";
//
//         while (true) {
//             tcp::socket socket(app_io_context);
//             acceptor.accept(socket);
//             std::cout << "Connection received from: " << socket.remote_endpoint() << "\n";
//
//             try {
//                 boost::asio::streambuf request_buffer;
//
//                 // Читаем запрос клиента
//                 try {
//                     boost::asio::read_until(socket, request_buffer, "\r\n\r\n");
//                 } catch (const boost::system::system_error &e) {
//                     if (e.code() == boost::asio::error::eof) {
//                         std::cout << "Client disconnected unexpectedly.\n";
//                         continue;
//                     } else {
//                         throw;
//                     }
//                 }
//
//                 std::istream request_stream(&request_buffer);
//                 std::string http_request;
//                 std::getline(request_stream, http_request);
//
//                 if (http_request.empty()) {
//                     std::cout << "Empty or invalid request received. Skipping...\n";
//                     continue;
//                 }
//
//                 std::cout << "Request: " << http_request << "\n";
//
//                 // Обработка маршрута /test/{id}
//                 size_t pos = http_request.find("/test/");
//                 if (pos != std::string::npos) {
//                     size_t start = pos + 6; // Длина "/test/" = 6
//                     size_t end = http_request.find(' ', start);
//                     if (end != std::string::npos) {
//                         std::string id = http_request.substr(start, end - start);
//
//                         // Подключение к базе данных
//                         PGconn *conn = connect_to_database(env);
//                         if (!conn) {
//                             std::string response =
//                                 "HTTP/1.1 500 Internal Server Error\r\n"
//                                 "Content-Type: application/json\r\n"
//                                 "Content-Length: " +
//                                 std::to_string(create_json_response("error", "Database connection failed").length()) +
//                                 "\r\n"
//                                 "\r\n" + create_json_response("error", "Database connection failed");
//
//                             boost::asio::write(socket, boost::asio::buffer(response));
//                             continue;
//                         }
//
//                         // Получаем данные из базы данных
//                         std::string db_result = query_database(conn, id);
//
//                         // Создаем JSON-ответ
//                         std::string json_response = create_json_response("data", db_result);
//
//                         // Формируем ответ
//                         std::string response =
//                             "HTTP/1.1 200 OK\r\n"
//                             "Content-Type: application/json\r\n"
//                             "Content-Length: " + std::to_string(json_response.length()) + "\r\n"
//                                                                                          "\r\n" + json_response;
//
//                         // Отправляем ответ клиенту
//                         boost::asio::write(socket, boost::asio::buffer(response));
//                         PQfinish(conn);
//                         std::cout << "Response sent with data from the database.\n";
//                         continue;
//                     }
//                 }
//
//                 // Обработка POST-запроса для отправки сообщения в RabbitMQ
//                 size_t pos2 = http_request.find("POST /loans/");
//                 if (pos2 != std::string::npos) {
//                     // Чтение тела POST-запроса
//                     std::string body;
//                     std::string line;
//                     while (std::getline(request_stream, line) && line != "\r") {}
//
//                     // Чтение тела запроса
//                     if (request_stream.peek() != EOF) {
//                         std::ostringstream body_stream;
//                         body_stream << request_stream.rdbuf();
//                         body = body_stream.str();
//                     }
//                     if (body.empty()) {
//                         send_response(socket, "400 Bad Request", create_json_response("error", "Empty message"));
//                         continue;
//                     }
//                     std::map<std::string, std::string> result;
//
//                     boost::property_tree::ptree root;
//                     std::istringstream iss(body);
//                     boost::property_tree::read_json(iss, root);
//
//                     // Извлекаем id и summ
//                     if (root.get_optional<std::string>("id")) {
//                         result["id"] = root.get<std::string>("id");
//                     }
//
//                     if (root.get_optional<double>("summ")) {
//                         std::ostringstream stream;
//                         stream << std::fixed << std::setprecision(2) << root.get<double>("summ");
//                         result["summ"] = stream.str();
//                     }
//
//                     // Создание JSON-сообщения
//                     std::string id = result["id"];
//                     double summ = std::stod(result["summ"]);
//
//                     boost::property_tree::ptree json_message;
//                     json_message.put("id", id);
//                     json_message.put("summ", summ);
//
//                     std::ostringstream oss;
//                     boost::property_tree::write_json(oss, json_message);
//
//                     std::string message = oss.str();
//                     std::cout << "DDDDDDDDDDDDDDDDDDDDDDDDDDDDD    : " << message << std::endl;
//                     enqueue_message(message);
//
//                     // Формируем ответ
//                     std::string json_response = create_json_response("message", "Loan request sent to RabbitMQ");
//                     std::string response =
//                         "HTTP/1.1 200 OK\r\n"
//                         "Content-Type: application/json\r\n"
//                         "Content-Length: " + std::to_string(json_response.length()) + "\r\n"
//                         "\r\n" + json_response;
//
//                     boost::asio::write(socket, boost::asio::buffer(response));
//                     std::cout << "Loan request processed successfully." << std::endl;
//                     continue;
//                 }
//
//                 // Если маршрут не распознан, отправляем текущее время
//                 std::string current_time = get_current_time();
//                 std::string json_response = create_json_response("current_time", current_time);
//
//                 std::string response =
//                     "HTTP/1.1 200 OK\r\n"
//                     "Content-Type: application/json\r\n"
//                     "Content-Length: " + std::to_string(json_response.length()) + "\r\n"
//                                                                                  "\r\n" + json_response;
//
//                 boost::asio::write(socket, boost::asio::buffer(response));
//                 std::cout << "Response sent with current time in JSON format.\n";
//             } catch (const std::exception &e) {
//                 std::cerr << "Exception during request processing: " << e.what() << "\n";
//             }
//         }
//
//         // Остановка рабочего потока RabbitMQ
//         stop_worker = true;
//         cv.notify_all();
//         rabbitmq_thread.join();
//
//     } catch (std::exception &e) {
//         std::cerr << "Exception: " << e.what() << "\n";
//     }
//
//     return 0;
// }

#include "app.h"
#include "src/db/database_manager.h"
#include "src/tools/utils.h"
#include <iostream>

#include "db/database_initializer.h"
#include "models/db_models/tariff_model.h"
#include "models/db_models/credit_history_model.h"
#include "models/db_models/credit_model.h"

int main() {
    try {
        auto config = read_env_file(".env");

        // Регистрация моделей
        auto tariff_model = std::make_shared<TariffModel>();
        auto credit_history_model = std::make_shared<CreditHistoryModel>();
        auto credit_model = std::make_shared<CreditModel>();
        DatabaseInitializer::instance().register_model(tariff_model);
        DatabaseInitializer::instance().register_model(credit_history_model);
        DatabaseInitializer::instance().register_model(credit_model);

        db::DatabaseManager db(config);
        RabbitMQManager rabbitmq(config);
        db.start();


        App app(config, db, rabbitmq);
        app.run();

        std::cin.get();

        app.stop();
        db.stop();
        rabbitmq.stop();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
