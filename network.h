#ifndef NETWORK_H_
#define NETWORK_H_

#include <string>
#include <vector>
#include <optional>
#include "common.h"

namespace NetworkLayer {

// === Serialization API ===
std::vector<char> Serialize(const Message &msg);
Message Deserialize(const std::vector<char> &data);

// === Socket API ===
Socket StartServer(int listen_port);
Socket Accept(Socket server_socket);
Socket Connect(const std::string &server_host, int server_port);
bool SendMessage(Socket sock, const Message &msg);
// [修正] 返回一个optional对象，而不是原始指针
std::optional<Message> ReceiveMessage(Socket sock);  // returns nullptr on disconnect/empty
void Close(Socket sock);

} // namespace NetworkLayer

#endif // NETWORK_H_
