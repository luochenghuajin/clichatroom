#ifndef COMMON_H_
#define COMMON_H_

#include <string>
#include <chrono>
#include <cstdint>

/**
 * @file common.h
 * @brief Core data structures shared between client and server.
 *
 * This header defines the common entities used across both
 * client and server sides of the chat system, including:
 *  - User representation
 *  - Message types
 *  - Message payload
 *  - Logging entries
 *
 * No business logic is included here; only pure data structures.
 */

/**
 * @brief Represents a single connected user in the system.
 */
using Socket = int;

struct User {
    int id;                     ///< Unique identifier (e.g., socket descriptor)
    std::string username;       ///< Unique username
    bool connected;             ///< Connection status (true if online)
    long long joined_at;        ///< Time the user joined (epoch timestamp)
};

/**
 * @brief Enumeration of different types of messages exchanged in the system.
 */
enum class MessageType {
    PUBLIC_MESSAGE,             ///< Standard chat message visible to all users
    PRIVATE_MESSAGE,            ///< Direct message to a specific user
    SYSTEM_ANNOUNCEMENT,        ///< Server-initiated broadcast
    USER_JOINED,                ///< Notification when a user enters the chat
    USER_LEFT,                  ///< Notification when a user exits the chat
    USER_LIST_REQUEST,          ///< Client command to request online users
    USER_LIST_RESPONSE,         ///< Server response with current user list
    COMMAND_RESPONSE            ///< Generic response to commands (acknowledge, error, etc.)
};

/**
 * @brief A universal structure for all messages transmitted between client and server.
 */
struct Message {
    MessageType type;           ///< Defines how the message should be processed
    long long timestamp;        ///< Time the message was created (epoch timestamp)
    std::string sender_username;///< The user who sent the message (or "Server" for system)
    std::string target_username;///< For private messages, the intended recipient (empty if not applicable)
    std::string content;        ///< Main message text or command payload
                                ///< e.g. For USER_LIST_RESPONSE, contains usernames as "alice,bob,charlie"
};

/**
 * @brief Structure for logging events and chat history.
 */
struct LogEntry {
    long long timestamp;        ///< When the event occurred (epoch timestamp)
    MessageType event_type;     ///< Type of event (message, join, leave, etc.)
    std::string actor;          ///< User or "Server" responsible for the event
    std::string target;         ///< Recipient (only for private messages, empty otherwise)
    std::string content;        ///< Text content or event description
};

// Helper: current epoch ms
static long long NowEpochMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
#endif // COMMON_H_