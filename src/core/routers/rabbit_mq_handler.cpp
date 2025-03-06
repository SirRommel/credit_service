#include "rabbit_mq_handler.h"
#include "../../rabbit_mq/rabbit_mq_worker.h"
#include "../../tools/utils.h"
#include "../../tools/json_tools.h"

RabbitMQHandler::RabbitMQHandler(std::shared_ptr<RabbitMQWorker> worker)
    : worker_(worker) {}

// void RabbitMQHandler::handleSendToRabbit(boost::asio::ip::tcp::socket &socket, std::istream &request_stream) {
//     try {
//         std::string body;
//         std::string line;
//
//         // Чтение тела POST-запроса
//         while (std::getline(request_stream, line) && line != "\r") {}
//         while (std::getline(request_stream, line)) {
//             body += line;
//         }
//
//         if (body.empty()) {
//             send_response(socket, "400 Bad Request", create_json_response("error", "Empty message"));
//             return;
//         }
//
//         // Добавляем сообщение в очередь RabbitMQ
//         worker_->enqueueMessage(body);
//
//         // Формируем ответ
//         std::string json_response = create_json_response("message", "Message sent to RabbitMQ");
//         std::string response =
//             "HTTP/1.1 200 OK\r\n"
//             "Content-Type: application/json\r\n"
//             "Content-Length: " + std::to_string(json_response.length()) + "\r\n"
//                                                                          "\r\n" + json_response;
//
//         boost::asio::write(socket, boost::asio::buffer(response));
//     } catch (const std::exception &e) {
//         std::cerr << "RabbitMQHandler error: " << e.what() << "\n";
//         send_response(socket, "500 Internal Server Error", create_json_response("error", "Internal server error"));
//     }
// }