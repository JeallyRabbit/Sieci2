// server.cpp

#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <map>
#include <set>
#include <iterator> // For std::istream_iterator

class Server {
public:
    Server(int port) : port(port) {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            std::cerr << "Error in opening socket" << std::endl;
            exit(1);
        }

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Error in binding to port" << std::endl;
            exit(2);
        }
    }

    void start() {
        if (listen(serverSocket, 10) < 0) {
            std::cerr << "Error in listening" << std::endl;
            exit(3);
        }
        std::cout << "Server listening on port " << port << std::endl;

        while (true) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);

            if (clientSocket < 0) {
                std::cerr << "Error in accepting client connection" << std::endl;
                continue;
            }

            pthread_t threadId;
            if (pthread_create(&threadId, nullptr, handleClient, new int(clientSocket)) != 0) {
                std::cerr << "Failed to create thread" << std::endl;
            }
        }
    }

private:
    int serverSocket;
    int port;
    static std::map<std::string, std::string> fileContents; // Filename to content mapping
    static std::map<int, std::string> currentEditingFile; // ClientID to currently editing file
    static pthread_mutex_t fileMutex;
    static pthread_mutex_t editMutex;

    static void* handleClient(void* arg) {
        int clientSocket = *(int*)arg;
        char buffer[1024] = {0};
        
        while (true) {
            ssize_t bytesRead = read(clientSocket, buffer, 1024);
            if (bytesRead <= 0) {
                break; // Client disconnected
            }
            std::string command(buffer);
            std::memset(buffer, 0, sizeof(buffer)); // Clear the buffer for the next message
            
            processCommand(command, clientSocket);
        }
        
        clearCurrentEditingFile(clientSocket);
        close(clientSocket);
        delete (int*)arg; // Cleanup
        return nullptr;
    }

    static void processCommand(const std::string& command, int clientSocket) {
        std::istringstream iss(command);
        std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                        std::istream_iterator<std::string>{}};

        if (tokens.empty()) return;

        if (tokens[0] == "create" && tokens.size() == 2) {
            createFile(tokens[1]);
        } else if (tokens[0] == "list" && tokens.size() == 1) {
            std::string fileList = listFiles();
            send(clientSocket, fileList.c_str(), fileList.length(), 0);
        } else if (tokens[0] == "edit" && tokens.size() >= 3) {
            //std::string content = combine(tokens.begin() + 2, tokens.end(), " ");
            std::string content =tokens[2];
            //std::cout<<content<<"\n";
            editFile(tokens[1], content);
            setCurrentEditingFile(clientSocket, tokens[1]);
            broadcastUpdate(tokens[1], clientSocket); // Pass clientSocket as originClientSocket
        }else if (tokens[0] == "get_content" && tokens.size() == 2) {
            sendFileContent(clientSocket, tokens[1]);
        }

    }

    static void sendFileContent(int clientSocket, const std::string& filename) {
        pthread_mutex_lock(&fileMutex);
        auto it = fileContents.find(filename);
        if (it != fileContents.end()) {
            std::string content = it->second;
            std::string message = "Content: " + filename + " " + content;
            send(clientSocket, message.c_str(), message.length(), 0);
        } else {
            std::string message = "Error: File not found";
            send(clientSocket, message.c_str(), message.length(), 0);
        }
        pthread_mutex_unlock(&fileMutex);
    }

    static void createFile(const std::string& filename) {
        pthread_mutex_lock(&fileMutex);
        std::ofstream file(filename);
        if (file.is_open()) {
            fileContents[filename] = ""; // Create an empty file
            file.close();
        }
        pthread_mutex_unlock(&fileMutex);
    }

    static std::string listFiles() {
        pthread_mutex_lock(&fileMutex);
        std::string fileList;
        for (const auto& pair : fileContents) {
            fileList += pair.first + "\n";
        }
        pthread_mutex_unlock(&fileMutex);
        return fileList;
    }

    static void editFile(const std::string& filename, const std::string& content) {
        std::cout<<"editing fileL "<<filename<<" Content:\n";
        std::cout<<content<<"\n"; 
        pthread_mutex_lock(&fileMutex);
        std::ofstream file(filename);
        if (file.is_open()) {
            fileContents[filename] = content;
            file << content;
            file.close();
        }
        pthread_mutex_unlock(&fileMutex);
    }

    static void broadcastUpdate(const std::string& filename, int originClientSocket) {
    std::string content;
    pthread_mutex_lock(&fileMutex);
    content = fileContents[filename];
    pthread_mutex_unlock(&fileMutex);

    pthread_mutex_lock(&editMutex);
    for (const auto& client : currentEditingFile) {
        // Check if the current client is not the one who sent the update
        if (client.second == filename && client.first != originClientSocket) {
            std::string message = "Update: " + filename + " " + content;
            send(client.first, message.c_str(), message.length(), 0);
        }
    }
    pthread_mutex_unlock(&editMutex);
    }

    static void setCurrentEditingFile(int clientSocket, const std::string& filename) {
        pthread_mutex_lock(&editMutex);
        currentEditingFile[clientSocket] = filename;
        pthread_mutex_unlock(&editMutex);
    }

    static void clearCurrentEditingFile(int clientSocket) {
        pthread_mutex_lock(&editMutex);
        currentEditingFile.erase(clientSocket);
        pthread_mutex_unlock(&editMutex);
    }

    template <typename Iter>
    static std::string combine(Iter begin, Iter end, const std::string& separator) {
        std::ostringstream result;
        if (begin != end) {
            result << *begin++;
        }
        while (begin != end) {
            result << separator << *begin++;
        }
        return result.str();
    }
};

std::map<std::string, std::string> Server::fileContents;
std::map<int, std::string> Server::currentEditingFile;
pthread_mutex_t Server::fileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Server::editMutex = PTHREAD_MUTEX_INITIALIZER;

int main() {
    int port = 12345; // Adjust as needed
    Server server(port);
    server.start();
    return 0;
}
