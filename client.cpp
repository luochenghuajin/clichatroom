// client.cpp
// Implementation of the CLIChatRoom client application.
// Translates ClientCLI pseudocode into C++ with POSIX threads for concurrency.

#include <iostream>
#include <string>
#include <pthread.h>
#include <csignal>
#include <optional>
#include <cstdlib>

#include "common.h"
#include "network.h"
#include "console.h"


namespace ClientCLI {

// Forward declarations
void InputLoop(Socket sock);
void ReceiveLoop(Socket sock);
void RunClient(const std::string &server_host, int server_port);

// ========== InputLoop ==========
void InputLoop(Socket sock) {
    while (true) {
        std::string line = Console::ReadLine();
        if (line.empty()) {
            // treat empty input as no-op
            continue;
        }

        Message msg;
        msg.timestamp = NowEpochMs();
        msg.sender_username = ""; // filled by server

        if (line == "/bye") {
            msg.type = MessageType::COMMAND_RESPONSE;
            msg.content = "BYE";
            NetworkLayer::SendMessage(sock, msg);
            NetworkLayer::Close(sock);
            break;
        } else if (line == "/list") {
            msg.type = MessageType::USER_LIST_REQUEST;
            msg.content = "";
            NetworkLayer::SendMessage(sock, msg);
        } else if (!line.empty() && line[0] == '@') {
            // private message: format "@user message..."
            size_t spacePos = line.find(' ');
            if (spacePos != std::string::npos) {
                std::string target = line.substr(1, spacePos - 1);
                std::string content = line.substr(spacePos + 1);
                msg.type = MessageType::PRIVATE_MESSAGE;
                msg.target_username = target;
                msg.content = content;
                NetworkLayer::SendMessage(sock, msg);
            }
        } else {
            msg.type = MessageType::PUBLIC_MESSAGE;
            msg.content = line;
            NetworkLayer::SendMessage(sock, msg);
        }
    }
}

// ========== ReceiveLoop ==========
void ReceiveLoop(Socket sock) {
    while (true) {
        std::optional<Message> opt_msg = NetworkLayer::ReceiveMessage(sock);
        if (!opt_msg.has_value()) {
            Console::Print("Disconnected from server.");
            break;
        }
        Message msg = opt_msg.value();

        if (msg.type == MessageType::COMMAND_RESPONSE && msg.content == "GOODBYE") {
            // exit silently
            break;
        }
        else if (msg.content.rfind("USER_NOT_FOUND:", 0) == 0) 
        {
        Console::Print("User not found" + msg.content.substr(14));
        }

        switch (msg.type) {
            case MessageType::USER_LIST_RESPONSE:
                Console::Print("Online: " + msg.content);
                break;
            case MessageType::SYSTEM_ANNOUNCEMENT:
                Console::Print("[SERVER] " + msg.content);
                break;
            case MessageType::PRIVATE_MESSAGE:
                Console::Print("[PM from " + msg.sender_username + "] " + msg.content);
                break;
            case MessageType::PUBLIC_MESSAGE:
                Console::Print(msg.sender_username + ": " + msg.content);
                break;
            case MessageType::USER_JOINED:
                Console::Print("* " + msg.sender_username + " joined the chat *");
                break;
            case MessageType::USER_LEFT:
                Console::Print("* " + msg.sender_username + " left the chat *");
                break;
            default:
                // ignore other message types
                break;
        }
    }
}

// Thread entry for pthread to run ReceiveLoop
static void* ReceiveLoopEntry(void* arg) {
    Socket sock = *reinterpret_cast<Socket*>(arg);
    ReceiveLoop(sock);
    return nullptr;
}

// ========== RunClient ==========
void RunClient(const std::string &server_host, int server_port) {
    Socket sock = NetworkLayer::Connect(server_host, server_port);
    if (sock < 0) {
        Console::Print("Failed to connect to server.");
        return;
    }

    // Authentication handshake: loop until accepted
    while (true) {
        std::optional<Message> opt_msg = NetworkLayer::ReceiveMessage(sock);
        if (!opt_msg.has_value()) {
            Console::Print("Disconnected during authentication.");
            NetworkLayer::Close(sock);
            return;
        }
        Message msg = opt_msg.value();
        if (msg.type == MessageType::COMMAND_RESPONSE && msg.content == "ENTER_USERNAME") {
            Console::Print("Please enter your username:");
            std::string uname = Console::ReadLine();
            Message reply;
            reply.type = MessageType::COMMAND_RESPONSE;
            reply.timestamp = NowEpochMs();
            reply.content = uname;
            NetworkLayer::SendMessage(sock, reply);
        } else if (msg.type == MessageType::COMMAND_RESPONSE && msg.content == "USERNAME_ACCEPTED") {
            break;
        } else if (msg.type == MessageType::COMMAND_RESPONSE && msg.content == "USERNAME_TAKEN") {
            Console::Print("Username already taken, try another:");
        }
    }

    // Spawn background thread for ReceiveLoop
    pthread_t recv_thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    Socket* arg = new Socket(sock);
    if (pthread_create(&recv_thread, &attr, ReceiveLoopEntry, arg) != 0) {
        Console::Print("Failed to start receive thread.");
        delete arg;
        pthread_attr_destroy(&attr);
        NetworkLayer::Close(sock);
        return;
    }
    pthread_attr_destroy(&attr);

    // Input loop runs in main thread
    InputLoop(sock);
}

} // namespace ClientCLI
#ifndef TEST_BUILD
// Console abstraction to allow tests to stub
namespace Console {
    std::string ReadLine() {
        std::string line;
        if (!std::getline(std::cin, line)) {
            return "";
        }
        return line;
    }
    void Print(const std::string &s) {
        std::cout << s << std::endl;
    }
}
// ========== Main ==========
int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 12345;
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid port argument, using default 12345\n";
        }
    }

    ClientCLI::RunClient(host, port);
    return 0;
}
#endif