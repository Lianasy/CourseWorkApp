#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#define PORT 8080

void startClient() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error: WinSock initialization failed!" << std::endl;
        return;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Error: Connection to server failed!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    std::cout << "Connected to server!" << std::endl;

    std::string input;
    std::cout << "Enter keywords to search (separated by space): ";
    std::getline(std::cin, input);

    std::string request = "SEARCH " + input + "\n";
    send(clientSocket, request.c_str(), request.length(), 0);

    char buffer[4096];
    std::string response;
    int bytesReceived;

    // Чтение всех данных из сокета
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0'; // Завершаем строку
        response += buffer;          // Добавляем данные в полный ответ
    }

    if (!response.empty()) {
        std::cout << "Movies found:\n" << response;
    } else {
        std::cout << "No response from server." << std::endl;
    }

    closesocket(clientSocket);
    WSACleanup();
}

int main(){
    startClient();
    return 0;
}
