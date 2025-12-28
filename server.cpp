// server.cpp
// Implementation of ConnectionManager and ClientHandler per pseudocode (Appendix A).
// Uses POSIX threads for concurrency (pthread).

#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <memory>
#include <atomic>

#include "common.h"
#include "network.h"
#include "services.h"

// Module-scope server socket for graceful shutdown
static Socket g_server_socket = -1;

// Forward declaration of graceful shutdown handler used by signal()
extern "C" void GracefulShutdownHandler(int signo);

namespace ClientHandler {

    // ServeClient implements the complete lifecycle for a single client connection
    // as described in Appendix A pseudocode.
    void ServeClient(Socket client_socket) {
        // Authenticate user
        std::optional<User> opt_user = UserManager::Authenticate(client_socket);
        if (!opt_user.has_value()) {
            // Authentication failed or client disconnected during handshake
            NetworkLayer::Close(client_socket);
            return;
        }

        User user = opt_user.value();

        // Build and broadcast join message
        Message joinMsg;
        joinMsg.type = MessageType::USER_JOINED;
        joinMsg.timestamp = NowEpochMs();
        joinMsg.sender_username = user.username;
        joinMsg.target_username = "";
        joinMsg.content = user.username + " joined";

        MessageRouter::BroadcastPublic(joinMsg);
        LoggingService::LogFromMessage(joinMsg);
       
        // Main receive loop
        while (user.connected) {
           
            std::optional<Message> incoming_opt = NetworkLayer::ReceiveMessage(client_socket);
            
            if (!incoming_opt.has_value()) {
                // Client disconnected or nothing to read
                break;
            }
            
            Message incoming = incoming_opt.value();
            // Populate sender username
            incoming.sender_username = user.username;
            // Fill timestamp if empty (tests set 0 for "empty")
            if (incoming.timestamp == 0) {
                incoming.timestamp = NowEpochMs();
            }

            // Process command / message
            std::string result = CommandProcessor::Process(incoming, client_socket);
            if (result == "DISCONNECT") {
                break;
            }
            // Otherwise continue loop
        }
        
        // Remove user from user manager
        UserManager::RemoveUser(user.username);

        // Broadcast leave message
        Message leaveMsg;
        leaveMsg.type = MessageType::USER_LEFT;
        leaveMsg.timestamp = NowEpochMs();
        leaveMsg.sender_username = user.username;
        leaveMsg.target_username = "";
        leaveMsg.content = user.username + " left";

        MessageRouter::BroadcastPublic(leaveMsg);
        LoggingService::LogFromMessage(leaveMsg);

        // Close socket
        NetworkLayer::Close(client_socket);
    }

} // namespace ClientHandler

namespace ConnectionManager {

    // Internal thread entry that adapts pthread signature to ServeClient
    struct ThreadArg {
        Socket sock;
    };

    static void* ThreadEntry(void* varg) {
        ThreadArg* t = static_cast<ThreadArg*>(varg);
        if (t) {
            Socket s = t->sock;
            // Call actual client handler
            ClientHandler::ServeClient(s);
            // cleanup
            delete t;
        }
        return nullptr;
    }

    // Spawn a detached pthread to run ClientHandler::ServeClient for client_socket
    static bool SpawnThreadForClient(Socket client_socket) {
        pthread_t thread;
        pthread_attr_t attr;
        int rc = pthread_attr_init(&attr);
        if (rc != 0) {
            return false;
        }
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        ThreadArg* arg = new ThreadArg();
        arg->sock = client_socket;

        rc = pthread_create(&thread, &attr, ThreadEntry, static_cast<void*>(arg));
        pthread_attr_destroy(&attr);
        if (rc != 0) {
            // failed to create thread; cleanup
            delete arg;
            return false;
        }
        // Detached thread; nothing more to do
        return true;
    }

    // Run: accept loop that spawns a detached thread for each accepted client
    void Run(Socket server_socket) {
        (void)server_socket; // parameter named to match pseudocode; not used to store globally here
        while (true) {
            Socket client_socket = NetworkLayer::Accept(server_socket);
            if (client_socket >= 0) {
                // spawn detached thread to serve client
                bool ok = SpawnThreadForClient(client_socket);
                if (!ok) {
                    LoggingService::LogSystem("Failed to spawn thread for client");
                    // close the client socket to avoid leak
                    NetworkLayer::Close(client_socket);
                }
            } else {
                LoggingService::LogSystem("Accept failed");
            }
            // Keep looping forever as per pseudocode
        }
    }

    // ShutdownAll broadcasts shutdown message, closes all user sockets, and logs.
    void ShutdownAll() {
        AnnouncementService::Broadcast("Server is shutting down");
        // Close every user socket using UserManager.ForEachUserSocket
        UserManager::ForEachUserSocket([](Socket s) {
            NetworkLayer::Close(s);
        });
        LoggingService::LogSystem("Server shutdown broadcasted");
    }

} // namespace ConnectionManager

// Graceful shutdown handler for signals (SIGINT)
extern "C" void GracefulShutdownHandler(int /*signo*/) {
    // Notify clients and close their sockets
    ConnectionManager::ShutdownAll();

    // Close server main socket if open
    if (g_server_socket >= 0) {
        NetworkLayer::Close(g_server_socket);
        g_server_socket = -1;
    }

    // Exit program
    std::_Exit(0);
}
#ifndef TEST_BUILD
// Server bootstrap functions
static void StartServerMain(int port) {
    // Initialize logging system
    LoggingService::Initialize("chat_history.log");

    // Start server listening socket
    g_server_socket = NetworkLayer::StartServer(port);

    // Welcome announcement
    AnnouncementService::Broadcast("Welcome to the chat room!");

    // Register graceful shutdown handler for SIGINT
    std::signal(SIGINT, GracefulShutdownHandler);

    // Enter main connection loop
    ConnectionManager::Run(g_server_socket);
}

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    int port = 12345; // default
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port argument, using default 12345\n";
        }
    }

    StartServerMain(port);
    return 0;
}
#endif // TEST_BUILD