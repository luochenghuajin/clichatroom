#ifndef SERVICES_H_
#define SERVICES_H_

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "common.h"
#include "network.h"
#include "file_io.h"

// Production-quality services for CLIChatRoom.
//
// This header declares the service modules translated from the golden
// pseudocode (Appendix A). Implementations are in services.cpp.
//
// Modules:
//  - UserManager
//  - CommandProcessor
//  - MessageRouter
//  - AnnouncementService
//  - LoggingService
//
// Thread-safety:
//  - UserManager is thread-safe via an internal mutex protecting the map.

namespace UserManager {

// Auth handshake: prompts for username up to N retries.
// Returns a constructed User on success; std::nullopt on failure/disconnect.
std::optional<User> Authenticate(Socket client_socket);

// Map ops
void AddUser(const User& user, Socket client_socket);
void RemoveUser(const std::string& username);
bool CheckUniqueness(const std::string& username);

// Returns the socket for username or -1 if not found (aligns with tests).
Socket GetSocket(const std::string& username);

// Snapshot of all usernames (no specific ordering guaranteed).
std::vector<std::string> GetAllUsernames();

// Iterate all sockets (snapshot at invocation time) and invoke callback(s).
void ForEachUserSocket(const std::function<void(Socket)>& callback);

} // namespace UserManager

namespace CommandProcessor {

// Process a received message from client_socket.
// Return values: "CONTINUE" or "DISCONNECT" (as per tests).
std::string Process(const Message& msg, Socket client_socket);

} // namespace CommandProcessor

namespace MessageRouter {

// Broadcast a public message to all connected users.
void BroadcastPublic(const Message& msg);

// Send a private message, or notify sender if user missing.
void SendPrivate(const Message& msg);

// Utility used internally and by tests via behavior: collect sockets snapshot.
std::vector<Socket> CollectAllSockets();

} // namespace MessageRouter

namespace AnnouncementService {

// Broadcast a server system announcement and log it.
void Broadcast(const std::string& text);

} // namespace AnnouncementService

namespace LoggingService {

// Configure output log file; ensures the file exists via OpenAppend.
void Initialize(const std::string& log_file_name);

// Log using data extracted from a Message.
void LogFromMessage(const Message& msg);

// Log an arbitrary system text entry from "Server".
void LogSystem(const std::string& text);

// Low-level write API used by Log* above.
void Write(const LogEntry& entry);

} // namespace LoggingService

#endif // SERVICES_H_
