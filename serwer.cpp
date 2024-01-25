#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>
#include <dirent.h>
#include <functional>


using namespace boost::asio;
using ip::tcp;
using std::cout;
using std::endl;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, std::unordered_map<std::string, std::string>& files, std::unordered_map<std::string, std::shared_ptr<Session>>& clients)
        : socket_(std::move(socket)), files_(files), clients_(clients) {
        client_address_ = socket_.remote_endpoint().address().to_string();
    }

    void Start() {
        cout << "Client connected: " << client_address_ << endl;
        DoRead();
    }

    const std::string& GetClientAddress() const {
        return client_address_;
    }

    std::vector<std::string> UpdateFileList() {
        std::vector<std::string> files;

        DIR* dir = opendir(".");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_REG) {
                    // Dodajemy do listy tylko pliki tekstowe (możesz dostosować warunki)
                    std::string filename(entry->d_name);
                    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
                        files.push_back(filename);
                    }
                }
            }
            closedir(dir);
        }

        return files;
    }


private:
    void DoRead() {
        auto self(shared_from_this());
        async_read_until(socket_, buffer_, '\n',
            [this, self](const boost::system::error_code& ec, std::size_t length) {
                if (!ec) {
                    std::istream is(&buffer_);
                    std::string request;
                    std::getline(is, request);
                    cout<<"WAITING FOR REQUEST"<<endl;
                    if (request.find("CREATE") == 0) {
                        HandleCreate(request.substr(7));
                    } else if (request.find("EDIT") == 0) {
                        HandleEdit(request.substr(5));
                    } else if (request.find("LIST") == 0) {
                        HandleList();
                    } else {
                        DoWrite("Invalid command\n");
                        DoRead();
                    }
                }
            });
    }

    void HandleCreate(const std::string& filename) {
        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << "Hello, this is a new fileeeeeeee! >\n" << std::endl;
            file.close();
            files_[filename] = "Hello, this is a new fileeeeeeee! >\n";
            auto it = files_.find(filename);
            DoWrite("File '" + filename + "' created successfully: "+
            it->second);
        } else {
            DoWrite("Failed to create file '" + filename + ">");
        }
        DoRead();
    }

    void HandleEdit(const std::string& filename) {
    // Zaktualizuj listę plików w folderze serwera (domyślnie zakładam, że masz funkcję UpdateFileList())
    std::vector<std::string> availableFiles = UpdateFileList();

    // Sprawdź, czy wymagany plik znajduje się w dostępnych plikach
    auto fileIt = std::find(availableFiles.begin(), availableFiles.end(), filename);

    if (fileIt != availableFiles.end()) {
        // Pobierz aktualną zawartość pliku
        auto it = files_.find(filename);
        if (it != files_.end()) {
            // Przesyłamy zawartość pliku do klienta
            DoWrite(it->second);
            cout << "Sent file content:" << endl;
            cout << it->second << endl;

            // Funkcja callback wywoływana po zakończeniu operacji asynchronicznej DoReadContent
            std::function<void(const std::string&)> readCallback;

            std::shared_ptr<std::string> newContent = std::make_shared<std::string>();
            cout << "filename przed callbackiem: " << filename << endl;
            // Funkcja callback wywoływana po zakończeniu operacji asynchronicznej DoReadContent
            readCallback = [this, filename, &newContent, &readCallback](const std::string& content) {
                cout << "Reading from client: " << content << endl;

                if (content == "ENDED_EDITING\n") {
                    cout << "Client ended editing" << endl;
                } else if (content != files_[filename] && content != "") {
                    cout << "Updating file content on server" << endl;
                    files_[filename] = content;

                    // Aktualizuj zawartość pliku
                    std::ofstream file(filename, std::ios::out | std::ios::trunc);
                    cout << "before if.is_open()" << endl;
                    cout << "filename: " << filename << endl;
                    if (file.is_open()) {
                        cout << "Updating file in server file" << endl;
                        file << content;
                        cout << "Content written to file: " << content << endl;
                        file.close();
                    }

                    // Tutaj możesz dodać kod do informowania klientów o aktualizacji zawartości pliku
                    BroadcastFileUpdate(filename);
                }

                // Ponownie uruchom odczyt asynchroniczny
                // DoReadContent(readCallback);
            };

            // Rozpocznij pierwszy odczyt asynchroniczny
            DoReadContent(readCallback);

            cout << "After while" << endl;
        } else {
            cout << "Błąd: nieznaleziono pliku '" << filename << "' w kontenerze serwera" << endl;
            // DoWrite("File '" + filename + "' not found\n");
            DoRead();
        }
    } else {
        cout << "Błąd: plik '" << filename << "' nie jest dostępny w folderze serwera" << endl;
        // DoWrite("File '" + filename + "' is not available on the server\n");
        DoRead();
    }
}


void DoReadContent(std::function<void(const std::string&)> callback) {
    auto self(shared_from_this());
    async_read_until(socket_, buffer_, '\n',
        [this, self, callback](const boost::system::error_code& ec, std::size_t length) {
            if (!ec) {
                std::istream is(&buffer_);
                std::string content;
                std::getline(is, content);
                std::cout << "Successfully read: " << content << std::endl;

                // Przekazujemy odczytaną zawartość do funkcji zwrotnej
                callback(content);
            } else {
                std::cout << "Error reading" << std::endl;
            }
        });
    std::cout << "Read initiated" << std::endl;
}

    void BroadcastFileUpdate(const std::string& filename) {
        // Pobierz aktualną zawartość pliku
        auto it = files_.find(filename);
        if (it != files_.end()) {
            std::string updatedContent = it->second;
            // Wyślij aktualizację do wszystkich klientów, oprócz aktualnego klienta
            for (auto& pair : clients_) {
                if (pair.second != shared_from_this()) {
                    pair.second->DoWrite("UPDATE " + filename + '\n' + updatedContent);
                }
            }
        }
        DoRead();
    }

    void HandleList() {
    std::vector<std::string> files = UpdateFileList();
    std::string fileNames;

    for (const auto& file : files) {
        fileNames += file + " ";
    }
    fileNames += "\n";

    DoWrite(fileNames);
    DoRead();
}

    void DoWrite(const std::string& response) {
        auto self(shared_from_this());
        async_write(socket_, buffer(response + '\n'),
            [this, self](const boost::system::error_code& ec, std::size_t /* length */) {
                if (ec) {
                    cout << "Error during write to " << client_address_ << ": " << ec.message() << endl;
                }
            });
    }

    

    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    std::unordered_map<std::string, std::string>& files_;
    std::unordered_map<std::string, std::shared_ptr<Session>>& clients_;
    std::string client_address_;
};

class Server {
public:
    Server(boost::asio::io_service& io_service, short port)
        : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
          socket_(io_service) {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code& ec) {
                if (!ec) {
                    auto session = std::make_shared<Session>(std::move(socket_), files_, clients_);
                    session->Start();
                    clients_[session->GetClientAddress()] = session;
                }
                DoAccept();
            });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::unordered_map<std::string, std::string> files_;
    std::unordered_map<std::string, std::shared_ptr<Session>> clients_;
};

int main() {
    try {
        boost::asio::io_service io_service;
        Server server(io_service, 12345);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}