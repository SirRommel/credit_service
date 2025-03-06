// //
// // Created by rommel on 3/4/25.
// //
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
//         tcp::acceptor acceptor(app_io_context, tcp::endpoint(boost::asio::ip::address::from_string(host), port));
//         acceptor.accept(socket);
//         boost::asio::streambuf request_buffer;
//
//         // Читаем запрос клиента
//         try {
//             boost::asio::read_until(socket, request_buffer, "\r\n\r\n");
//         } catch (const boost::system::system_error &e) {
//             if (e.code() == boost::asio::error::eof) {
//                 std::cout << "Client disconnected unexpectedly.\n";
//                 continue;
//             } else {
//                 throw;
//             }
//         }
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