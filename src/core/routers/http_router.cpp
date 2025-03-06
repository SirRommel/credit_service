#include "headers/http_router.h"

#include "rabbit_mq_handler.h"
#include "../../rabbit_mq/rabbit_mq_worker.h"

#include "../../tools/utils.h"
#include "../../tools/json_tools.h"

HttpRouter::HttpRouter(boost::asio::io_context &io_context, const std::string &host, uint16_t port,
                       std::shared_ptr<RabbitMQWorker> rabbitmq_worker)
    : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port)),
      rabbitmq_worker_(rabbitmq_worker) {}

void HttpRouter::doAccept() {
    acceptor_.async_accept([this](std::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            std::cout << "Connection received from: " << socket.remote_endpoint() << "\n";
            handleRequest(std::move(socket));
        }
        doAccept(); // Продолжаем принимать новые соединения
    });
}

void HttpRouter::start() {
    std::cout << "Server started. Listening on " << acceptor_.local_endpoint() << "...\n";
    doAccept();
}

void HttpRouter::handleRequest(boost::asio::ip::tcp::socket socket) {
    std::thread request_handler([this, s = std::move(socket)]() mutable {
        try {
            boost::asio::streambuf request_buffer;

            // Читаем запрос клиента
            try {
                boost::asio::read_until(s, request_buffer, "\r\n\r\n");
            } catch (const boost::system::system_error &e) {
                if (e.code() == boost::asio::error::eof) {
                    std::cout << "Client disconnected unexpectedly.\n";
                    return;
                } else {
                    throw;
                }
            }

            std::istream request_stream(&request_buffer);
            std::string http_request;
            std::getline(request_stream, http_request);

            if (http_request.empty()) {
                std::cout << "Empty or invalid request received. Skipping...\n";
                return;
            }

            std::cout << "Request: " << http_request << "\n";

            if (http_request.find("POST /send_to_rabbit") != std::string::npos) {
                handleSendToRabbit(s, request_stream);
            } else {
                send_response(s, "404 Not Found", create_json_response("error", "Route not found"));
            }
        } catch (const std::exception &e) {
            std::cerr << "Exception during request processing: " << e.what() << "\n";
        }
    });
    request_handler.detach();
}

void HttpRouter::handleSendToRabbit(boost::asio::ip::tcp::socket &socket, std::istream &request_stream) {
    try {
        std::string body;
        std::string line;

        // Пропускаем заголовки
        while (std::getline(request_stream, line) && line != "\r") {}
        while (std::getline(request_stream, line)) {
            body += line;
        }

        if (body.empty()) {
            send_response(socket, "400 Bad Request", create_json_response("error", "Empty message"));
            return;
        }

        // Добавляем сообщение в очередь RabbitMQ
        rabbitmq_worker_->enqueueMessage(body);

        // Формируем ответ
        std::string json_response = create_json_response("message", "Message sent to RabbitMQ");
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(json_response.length()) + "\r\n"
            "\r\n" + json_response;

        boost::asio::write(socket, boost::asio::buffer(response));
    } catch (const std::exception &e) {
        std::cerr << "HttpRouter error: " << e.what() << std::endl;
        send_response(socket, "500 Internal Server Error", create_json_response("error", "Internal server error"));
    }
}

void sendResponse(boost::asio::ip::tcp::socket &socket, const std::string &status, const std::string &json_response) {
    std::ostringstream response_stream;
    response_stream
        << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << json_response.length() << "\r\n"
        << "\r\n"
        << json_response;

    boost::asio::write(socket, boost::asio::buffer(response_stream.str()));
}