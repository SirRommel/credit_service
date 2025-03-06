// #ifndef TIME_ENDPOINT_H
// #define TIME_ENDPOINT_H
//
// #include "endpoint_handler.h"
// #include <chrono>
// #include <iomanip>
// #include <ctime>
//
// class TimeEndpoint : public EndpointHandler {
// public:
//     void handle_get(boost::beast::tcp_stream&& stream) override {
//         auto session = std::make_shared<Session>(std::move(stream));
//         session->start();
//     }
//
// private:
//     class Session : public std::enable_shared_from_this<Session> {
//     public:
//         void process_request() {
//             if (req_.method() != boost::beast::http::verb::get) {
//                 return send_error(boost::beast::http::status::method_not_allowed);
//             }
//             if (req_.target() == "/time") {
//                 send_time_response();
//             } else {
//                 send_error(boost::beast::http::status::not_found);
//             }
//         }
//         Session(boost::beast::tcp_stream&& stream)
//             : stream_(std::move(stream)) {}
//
//         void start() {
//             do_read();
//         }
//
//     private:
//         void do_read() {
//             req_ = {};
//             boost::beast::http::async_read(
//                 stream_, buffer_, req_,
//                 [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
//                     if (!ec) {
//                         self->process_request(); // Убрана проверка метода здесь
//                     } else {
//                         self->send_error(boost::beast::http::status::bad_request);
//                     }
//                 });
//         }
//
//
//
//
//         void send_time_response() {
//             auto now = std::chrono::system_clock::now();
//             std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//             std::stringstream ss;
//             ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %X");
//
//             boost::beast::http::response<boost::beast::http::string_body> res{
//                 boost::beast::http::status::ok, req_.version()};
//             res.set(boost::beast::http::field::server, "TimeEndpoint");
//             res.set(boost::beast::http::field::content_type, "text/plain");
//             res.body() = "Current time: " + ss.str();
//             res.prepare_payload();
//
//             res.set(boost::beast::http::field::connection, "close");
//             boost::beast::http::async_write(
//                 stream_, res,
//                 [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
//                     if (!ec) {
//                         self->stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
//                     }
//                 });
//         }
//
//         void send_error(boost::beast::http::status status) {
//             boost::beast::http::response<boost::beast::http::string_body> res{status, req_.version()};
//             res.set(boost::beast::http::field::server, "TimeEndpoint");
//             res.body() = "Error: " + std::to_string(static_cast<int>(status));
//             res.prepare_payload();
//             boost::beast::http::async_write(
//                 stream_, res,
//                 [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
//                     self->stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send);
//                 });
//         }
//
//         boost::beast::tcp_stream stream_;
//         boost::beast::flat_buffer buffer_;
//         boost::beast::http::request<boost::beast::http::string_body> req_;
//     };
// };
//
// #endif // TIME_ENDPOINT_H