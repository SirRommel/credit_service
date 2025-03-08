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
#include <iostream>

enum class PaymentType { ANNUITY, DIFFERENTIATED };

double calculate_annuity_payment(double remaining_debt, double annual_rate) {
    double monthly_rate = (annual_rate / 100.0) / 12.0;
    return remaining_debt * monthly_rate;  // Упрощённая версия без срока
}

double calculate_differentiated_payment(double remaining_debt, double annual_rate) {
    double monthly_rate = (annual_rate / 100.0) / 12.0;
    return remaining_debt * monthly_rate;  // Только проценты за месяц
}

int main() {
    double initial_amount, remaining_debt, annual_rate;
    int payment_choice;

    std::cout << "Введите начальную сумму кредита: ";
    std::cin >> initial_amount;
    std::cout << "Введите оставшийся долг: ";
    std::cin >> remaining_debt;
    std::cout << "Введите годовую процентную ставку (%): ";
    std::cin >> annual_rate;

    std::cout << "Выберите тип платежа (1 - аннуитетный, 2 - дифференцированный): ";
    std::cin >> payment_choice;

    if (payment_choice == 1) {
        double annuity_payment = calculate_annuity_payment(remaining_debt, annual_rate);
        std::cout << "Аннуитетный платеж (без учёта срока): " << annuity_payment << std::endl;
    } else if (payment_choice == 2) {
        double diff_payment = calculate_differentiated_payment(remaining_debt, annual_rate);
        std::cout << "Дифференцированный платеж (проценты за месяц): " << diff_payment << std::endl;
    } else {
        std::cout << "Ошибка: неверный тип платежа!" << std::endl;
    }

    return 0;
}
