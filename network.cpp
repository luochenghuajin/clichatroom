#include "network.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <optional>

#include <cstring>
#include <stdexcept>
#include <vector>

namespace NetworkLayer {

// ------------------ Helpers ------------------

static bool send_all(Socket sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(sock, buf + sent, len - sent, MSG_NOSIGNAL); 
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

static bool recv_all(Socket sock, char *buf, size_t len) {
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = ::recv(sock, buf + recvd, len - recvd, 0);
        if (n <= 0) return false;
        recvd += n;
    }
    return true;
}

// ------------------ Serialization ------------------

std::vector<char> Serialize(const Message &msg) {
    std::vector<char> buffer;

    auto write_int32 = [&](int32_t v) {
        int32_t netv = htonl(v);
        const char *p = reinterpret_cast<const char *>(&netv);
        buffer.insert(buffer.end(), p, p + sizeof(netv));
    };
    auto write_int64 = [&](int64_t v) {
        // convert to network order manually (big-endian)
        uint64_t uv = static_cast<uint64_t>(v);
        for (int i = 7; i >= 0; --i) {
            buffer.push_back(static_cast<char>((uv >> (i * 8)) & 0xFF));
        }
    };
    auto write_string = [&](const std::string &s) {
        write_int32(static_cast<int32_t>(s.size()));
        buffer.insert(buffer.end(), s.begin(), s.end());
    };

    write_int32(static_cast<int32_t>(msg.type));
    write_int64(msg.timestamp);
    write_string(msg.sender_username);
    write_string(msg.target_username);
    write_string(msg.content);

    return buffer;
}

// network.cpp

Message Deserialize(const std::vector<char> &data) {
    size_t pos = 0;
    auto read_int32 = [&](int32_t &out) {
        if (pos + 4 > data.size()) throw std::runtime_error("Deserialize: truncated int32");
        int32_t netv;
        std::memcpy(&netv, &data[pos], 4);
        pos += 4;
        out = ntohl(netv);
    };

    // [修正] 将参数类型从 int64_t& 改为 long long&
    auto read_int64 = [&](long long &out) {
        if (pos + 8 > data.size()) throw std::runtime_error("Deserialize: truncated int64");
        uint64_t uv = 0;
        for (int i = 0; i < 8; ++i) {
            uv = (uv << 8) | (static_cast<unsigned char>(data[pos + i]));
        }
        pos += 8;
        out = static_cast<long long>(uv);
    };

    auto read_string = [&](std::string &out) {
        int32_t len;
        read_int32(len);
        if (len < 0 || pos + static_cast<size_t>(len) > data.size())
            throw std::runtime_error("Deserialize: invalid string length");
        out.assign(&data[pos], &data[pos + len]);
        pos += len;
    };

    Message msg;
    int32_t type_i32;
    read_int32(type_i32);
    msg.type = static_cast<MessageType>(type_i32);
    read_int64(msg.timestamp); // 现在类型完全匹配了
    read_string(msg.sender_username);
    read_string(msg.target_username);
    read_string(msg.content);

    return msg;
}

// ------------------ Network API ------------------

Socket StartServer(int listen_port) {
    Socket sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) throw std::runtime_error("socket() failed");

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listen_port);

    if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        ::close(sock);
        throw std::runtime_error("bind() failed");
    }

    if (::listen(sock, SOMAXCONN) < 0) {
        ::close(sock);
        throw std::runtime_error("listen() failed");
    }

    return sock;
}

Socket Accept(Socket server_socket) {
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    Socket cs = ::accept(server_socket, reinterpret_cast<sockaddr *>(&client_addr), &len);
    if (cs < 0) throw std::runtime_error("accept() failed");
    return cs;
}

Socket Connect(const std::string &server_host, int server_port) {
    Socket sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);

    if (::inet_pton(AF_INET, server_host.c_str(), &addr.sin_addr) <= 0) {
        ::close(sock);
        throw std::runtime_error("inet_pton() failed");
    }

    if (::connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        ::close(sock);
        throw std::runtime_error("connect() failed");
    }

    return sock;
}

bool SendMessage(Socket sock, const Message &msg) {
    std::vector<char> serialized = Serialize(msg);

    int32_t total_len = static_cast<int32_t>(serialized.size());
    int32_t net_len = htonl(total_len);

    if (!send_all(sock, reinterpret_cast<const char *>(&net_len), sizeof(net_len)))
        return false;
    if (!send_all(sock, serialized.data(), serialized.size()))
        return false;

    return true;
}

std::optional<Message> ReceiveMessage(Socket sock) {
    int32_t net_len;
    if (!recv_all(sock, reinterpret_cast<char*>(&net_len), sizeof(net_len))) {
        return std::nullopt; // [修正] 用 std::nullopt 替代 nullptr
    }
    int32_t total_len = ntohl(net_len);
    if (total_len <= 0) {
        return std::nullopt; // [修正]
    }

    std::vector<char> buf(total_len);
    if (!recv_all(sock, buf.data(), buf.size())) {
        return std::nullopt; // [修正]
    }

    try {
        Message msg = Deserialize(buf);
        return msg; // [修正] 直接返回值，optional会自动包装
    } catch (...) {
        return std::nullopt; // [修正]
    }
}

void Close(Socket sock) {
    if (sock >= 0) ::close(sock);
}

} // namespace NetworkLayer
