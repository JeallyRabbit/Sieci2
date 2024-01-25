#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

using namespace boost::asio;
using ip::tcp;
using std::cin;
using std::cout;
using std::endl;

// Zmienna globalna do przechowywania bufora
//boost::asio::streambuf buffer;

void SendFileContent(tcp::socket& socket, const std::string& filename) {
    std::ifstream file(filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string fileContent = buffer.str();
    //boost::asio::write(socket, boost::asio::buffer(fileContent + '\n'));
    boost::asio::write(socket, boost::asio::buffer(fileContent + ">"));

    cout<<"sending: "<<fileContent<<endl;
}

/*void ReceiveFileContent(tcp::socket& socket, std::function<void(const std::string&)> callback) {
    auto buffer = std::make_shared<boost::asio::streambuf>();

    boost::asio::async_read_until(socket, *buffer, '\n',
        [buffer, callback](const boost::system::error_code& ec, std::size_t length) {
            if (!ec) {
                std::istream response_stream(buffer.get());
                std::string content;
                std::getline(response_stream, content);
                callback(content);
            } else {
                std::cerr << "Error receiving content: " << ec.message() << std::endl;
            }
        });
}*/


void ReceiveFileContent(tcp::socket& socket, std::string& content) {
    boost::asio::streambuf buffer;
    boost::asio::read_until(socket, buffer, ">\n");
    std::istream response_stream(&buffer);
    
    std::getline(response_stream, content);
}

void MonitorFileChanges(const std::string& filename, std::function<void(const std::string&)> callback) {
    boost::asio::io_service io_service;
    boost::asio::posix::stream_descriptor descriptor(io_service, ::open(filename.c_str(), O_RDONLY));

    // Zainicjalizuj bufor
    boost::asio::streambuf buffer;
    std::istream input_stream(&buffer);

    // Rozpocznij operację asynchronicznego czytania
    async_read_until(descriptor, buffer, '\n',
        [filename, &input_stream, &callback, &descriptor](const boost::system::error_code& ec, std::size_t length) {
            if (!ec) {
                // Odczytaj dane z bufora
                std::string content;
                std::getline(input_stream, content);
                cout << "klient odczytal: " << content << endl;

                // Wywołaj funkcję zwrotną z odczytaną zawartością
                callback(content);

                // Ponownie rozpocznij monitorowanie zmian w pliku
                MonitorFileChanges(filename, callback);
            } else {
                // Sprawdź, czy to nie jest koniec pliku
                if (ec != boost::asio::error::eof) {
                    std::cout << "Error monitoring file changes: " << ec.message() << std::endl;
                } else {
                    // Plik został zamknięty, zakończ monitorowanie
                    std::cout << "File closed. Stop monitoring." << std::endl;
                }
            }
        }
    );

    // Uruchom obsługę zdarzeń IO
    io_service.run();
}
int main() {
    try {
        boost::asio::io_service io_service;

        tcp::socket socket(io_service);
        tcp::endpoint endpoint(ip::address::from_string("127.0.0.1"), 12345);
        socket.connect(endpoint);

        cout << "Connected to the server" << endl;

         bool editingFinished = false; 

        while (true) {
            cout << "Menu:" << endl;
            cout << "1. Create text file" << endl;
            cout << "2. Edit file" << endl;
            cout << "3. List files" << endl;
            cout << "0. Exit" << endl;

            int choice;
            cout << "Enter your choice: ";
            cin >> choice;

            if (choice == 0) {
                break;
            } else if (choice == 1) {
                std::string filename;
                cout << "Enter the filename: ";
                cin >> filename;

                std::string createCommand = "CREATE " + filename + '\n';
                boost::asio::write(socket, boost::asio::buffer(createCommand));

                boost::asio::streambuf response;
                boost::asio::read_until(socket, response, '\n');

                std::istream is(&response);
                std::string serverResponse;
                std::getline(is, serverResponse);

                cout << "Server Response: " << serverResponse << endl;

            }  else if (choice == 2) {
                std::string filename;
                cout << "Enter the filename on server to edit: ";
                cin >> filename;

                std::string editCommand = "EDIT " + filename + '\n';
                boost::asio::write(socket, boost::asio::buffer(editCommand));

                std::string fileContent;
                ReceiveFileContent(socket, fileContent);

                std::string tempFilename = "/tmp/" + filename;
                std::ofstream tempFile(tempFilename);
                tempFile << fileContent;
                tempFile.close();

                boost::thread editorThread([tempFilename]() {
                    boost::process::system("xdg-open " + tempFilename);
                });

                while (true) {
                    std::ifstream tempFileCheck(tempFilename);
                    std::stringstream bufferCheck;
                    bufferCheck << tempFileCheck.rdbuf();
                    std::string localContent = bufferCheck.str();
                    tempFileCheck.close();

                    if (localContent != fileContent) {
                        SendFileContent(socket, tempFilename);
                        fileContent = localContent;
                        cout << "File has been updated on the server.\n";
                        break;
                    }

                    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                }

                cout << "Editing ended" << endl;
                boost::asio::write(socket, boost::asio::buffer("ENDED_EDITING")); 
                
                
                editorThread.join();

            } else if (choice == 3){
                std::string listCommand = "LIST\n";
                boost::asio::write(socket, boost::asio::buffer(listCommand));

                boost::asio::streambuf response;
                boost::asio::read_until(socket, response, '\n');

                std::istream is(&response);
                std::string serverResponse;
                std::getline(is, serverResponse);

                cout << "Server Response (File List):\n" << serverResponse << endl;
            }else {
                cout << "Invalid choice. Try again." << endl;
            }
        }

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
