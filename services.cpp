#include "services.h"

#include <sstream>
#include <algorithm>

// ===============================
// helpers (internal linkage)
// ===============================
namespace {

constexpr int kAuthMaxRetries = 3;

// Simple string join utility.
static std::string Join(const std::vector<std::string>& parts, const std::string& sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) oss << sep;
        oss << parts[i];
    }
    return oss.str();
}

// Lightweight "format" for a LogEntry line. Tests only check substring presence,
// so we keep a readable, stable delimiter-based encoding.
static std::string FormatLogLine(const LogEntry& e) {
    std::ostringstream oss;
    oss << e.timestamp
        << " | " << static_cast<int>(e.event_type)
        << " | " << e.actor
        << " | " << e.target
        << " | " << e.content;
    return oss.str();
}

} // namespace

// ===============================
// UserManager (thread-safe map)
// ===============================
namespace UserManager {

using Pair = std::pair<User, Socket>;

static std::unordered_map<std::string, Pair> g_users;
static std::mutex g_mutex;

static Message MakeServerCommand(const std::string& content) {
    Message m;
    m.type = MessageType::COMMAND_RESPONSE;
    m.timestamp = NowEpochMs();
    m.sender_username = "Server";
    m.target_username = "";
    m.content = content;
    return m;
}

std::optional<User> Authenticate(Socket client_socket) {
    int retries = 0;

    while (retries < kAuthMaxRetries) {
        // Prompt for username
        Message prompt = MakeServerCommand("ENTER_USERNAME");
        NetworkLayer::SendMessage(client_socket, prompt);

        // Wait for reply
        auto replyOpt = NetworkLayer::ReceiveMessage(client_socket);
        if (!replyOpt.has_value()) {
            return std::nullopt;
        }
        const Message& reply = *replyOpt;
        std::string username = reply.content;

        bool unique = CheckUniqueness(username);
        if (unique) {
            User user;
            // In this project, Socket serves as the id surrogate.
            user.id = client_socket;
            user.username = username;
            user.connected = true;
            user.joined_at = NowEpochMs();

            AddUser(user, client_socket);

            Message ok = MakeServerCommand("USERNAME_ACCEPTED");
            NetworkLayer::SendMessage(client_socket, ok);

            return user;
        } else {
            Message taken = MakeServerCommand("USERNAME_TAKEN");
            NetworkLayer::SendMessage(client_socket, taken);
            ++retries;
        }
    }

    // Too many attempts
    Message fail = MakeServerCommand("AUTH_FAILED");
    NetworkLayer::SendMessage(client_socket, fail);
    return std::nullopt;
}

void AddUser(const User& user, Socket client_socket) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_users[user.username] = std::make_pair(user, client_socket);
}

void RemoveUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_users.erase(username);
}

bool CheckUniqueness(const std::string& username) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_users.find(username) == g_users.end();
}

Socket GetSocket(const std::string& username) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_users.find(username);
    if (it == g_users.end()) return static_cast<Socket>(-1);
    return it->second.second;
}

std::vector<std::string> GetAllUsernames() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<std::string> names;
    names.reserve(g_users.size());
    for (const auto& kv : g_users) {
        names.push_back(kv.first);
    }
    return names;
}

void ForEachUserSocket(const std::function<void(Socket)>& callback) {
    // Snapshot sockets under lock, then invoke callbacks without holding lock.
    std::vector<Socket> sockets;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        sockets.reserve(g_users.size());
        for (const auto& kv : g_users) {
            sockets.push_back(kv.second.second);
        }
    }
    for (Socket s : sockets) {
        callback(s);
    }
}

} // namespace UserManager

// ===============================
// CommandProcessor
// ===============================
namespace CommandProcessor {

std::string Process(const Message& msg, Socket client_socket) {
    if (msg.type == MessageType::USER_LIST_REQUEST) {
        auto names = UserManager::GetAllUsernames();
        std::string content = Join(names, ",");

        Message resp;
        resp.type = MessageType::USER_LIST_RESPONSE;
        resp.timestamp = NowEpochMs();
        resp.sender_username = "Server";
        resp.target_username = "";
        resp.content = content;

        NetworkLayer::SendMessage(client_socket, resp);
        LoggingService::LogFromMessage(resp);
        return "CONTINUE";
    } else if (msg.type == MessageType::PRIVATE_MESSAGE) {
        MessageRouter::SendPrivate(msg);
        LoggingService::LogFromMessage(msg);
        return "CONTINUE";
    } else if (msg.type == MessageType::PUBLIC_MESSAGE) {
        MessageRouter::BroadcastPublic(msg);
        LoggingService::LogFromMessage(msg);
        return "CONTINUE";
    } else if (msg.type == MessageType::COMMAND_RESPONSE && msg.content == "BYE") {
        Message ack;
        ack.type = MessageType::COMMAND_RESPONSE;
        ack.timestamp = NowEpochMs();
        ack.sender_username = "Server";
        ack.target_username = "";
        ack.content = "GOODBYE";
        NetworkLayer::SendMessage(client_socket, ack);
        return "DISCONNECT";
    } else {
        Message err;
        err.type = MessageType::COMMAND_RESPONSE;
        err.timestamp = NowEpochMs();
        err.sender_username = "Server";
        err.target_username = "";
        err.content = "UNKNOWN_COMMAND";
        NetworkLayer::SendMessage(client_socket, err);
        return "CONTINUE";
    }
}

} // namespace CommandProcessor

// ===============================
// MessageRouter
// ===============================
namespace MessageRouter {

std::vector<Socket> CollectAllSockets() {
    std::vector<Socket> sockets;
    // Build vector via ForEachUserSocket callback
    UserManager::ForEachUserSocket([&](Socket s) {
        sockets.push_back(s);
    });
    return sockets;
}

void BroadcastPublic(const Message& msg) {
    auto sockets = CollectAllSockets();
    for (Socket s : sockets) {
        NetworkLayer::SendMessage(s, msg);
    }
}

void SendPrivate(const Message& msg) {
    Socket target_socket = UserManager::GetSocket(msg.target_username);
    if (target_socket != static_cast<Socket>(-1)) {
        NetworkLayer::SendMessage(target_socket, msg);
        return;
    }

    // Notify sender that user not found
    Message notify;
    notify.type = MessageType::COMMAND_RESPONSE;
    notify.timestamp = NowEpochMs();
    notify.sender_username = "Server";
    notify.target_username = "";
    notify.content = std::string("USER_NOT_FOUND:") + msg.target_username;

    Socket sender_socket = UserManager::GetSocket(msg.sender_username);
    if (sender_socket != static_cast<Socket>(-1)) {
        NetworkLayer::SendMessage(sender_socket, notify);
    }
}

} // namespace MessageRouter

// ===============================
// AnnouncementService
// ===============================
namespace AnnouncementService {

void Broadcast(const std::string& text) {
    Message m;
    m.type = MessageType::SYSTEM_ANNOUNCEMENT;
    m.timestamp = NowEpochMs();
    m.sender_username = "Server";
    m.target_username = "";
    m.content = text;

    MessageRouter::BroadcastPublic(m);
    LoggingService::LogFromMessage(m);
}

} // namespace AnnouncementService

// ===============================
// LoggingService
// ===============================
namespace LoggingService {

static std::string g_current_log_file = "chat_history.log";

void Initialize(const std::string& log_file_name) {
    g_current_log_file = log_file_name;
    File::OpenAppend(g_current_log_file);
}

void LogFromMessage(const Message& msg) {
    LogEntry e;
    e.timestamp = msg.timestamp;
    e.event_type = msg.type;
    e.actor = msg.sender_username;
    e.target = msg.target_username;
    e.content = msg.content;
    Write(e);
}

void LogSystem(const std::string& text) {
    LogEntry e;
    e.timestamp = NowEpochMs();
    e.event_type = MessageType::SYSTEM_ANNOUNCEMENT;
    e.actor = "Server";
    e.target = "";
    e.content = text;
    Write(e);
}

void Write(const LogEntry& entry) {
    const std::string line = FormatLogLine(entry);
    File::AppendLine(g_current_log_file, line);
}

} // namespace LoggingService
