// #ifndef TIME_GROUP_H
// #define TIME_GROUP_H
//
// #include "endpoint_group.h"
// #include "../handlers/time_endpoint.h"
//
// class TimeGroup : public EndpointGroup {
// public:
//     TimeGroup(boost::asio::io_context& io_ctx)
//         : EndpointGroup(io_ctx) {
//         auto time_ep = std::make_shared<TimeEndpoint>();
//         register_endpoint("/time", time_ep);
//     }
// };
//
// #endif // TIME_GROUP_H