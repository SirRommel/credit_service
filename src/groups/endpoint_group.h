// #ifndef ENDPOINT_GROUP_H
// #define ENDPOINT_GROUP_H
//
// #include <boost/asio.hpp>
// #include <memory>
// #include <unordered_map>
// #include "../handlers/endpoint_handler.h"
//
// class EndpointGroup {
// public:
//     EndpointGroup(boost::asio::io_context& io_ctx)
//         : io_context_(io_ctx), thread_([this]() { io_context_.run(); }) {}
//
//     virtual ~EndpointGroup() {
//         io_context_.stop();
//         if (thread_.joinable()) thread_.join();
//     }
//
//     void register_endpoint(const std::string& path, std::shared_ptr<EndpointHandler> handler) {
//         handlers_[path] = handler;
//     }
//
//     // В EndpointGroup::handle_request
//     void EndpointGroup::handle_request(boost::beast::tcp_stream&& stream, const std::string& target) {
//         std::string normalized_path = target;
//         if (!normalized_path.empty() && normalized_path.back() == '/') {
//             normalized_path.pop_back();
//         }
//         auto it = handlers_.find(normalized_path);
//         if (it != handlers_.end()) {
//             boost::asio::post(io_context_, [handler = it->second, stream = std::move(stream)]() mutable {
//                 handler->handle_get(std::move(stream));
//             });
//         }
//     }
//
// protected:
//     boost::asio::io_context& io_context_;
//     std::thread thread_;
//     std::unordered_map<std::string, std::shared_ptr<EndpointHandler>> handlers_;
// };
//
// #endif // ENDPOINT_GROUP_H