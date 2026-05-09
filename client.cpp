// client.cpp

#include <iostream>
#include <winsock2.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

// ================= ENCRYPTION =================

vector<int> cipherPattern = {2,5,5,14,23,23};

// ================= GLOBALS =================

mutex screenMutex;
mutex queueMutex;

queue<string> buyQueue;
queue<string> sellQueue;
queue<string> failQueue;
queue<string> authQueue;

string latestMarket = "Loading Market...\n";
string latestPortfolio = "Loading Portfolio...\n";

// ================= ENCRYPT / DECRYPT =================

string encryptMessage(string msg) {
    string encrypted = msg;
    for(int i=0; i<msg.size(); i++) {
        int shift = cipherPattern[i % cipherPattern.size()];
        encrypted[i] = msg[i] + shift;
    }
    return encrypted;
}

string decryptMessage(string msg) {
    string decrypted = msg;
    for(int i=0; i<msg.size(); i++) {
        int shift = cipherPattern[i % cipherPattern.size()];
        decrypted[i] = msg[i] - shift;
    }
    return decrypted;
}

// ================= SEND =================

void sendEncrypted(SOCKET sock, string msg) {
    string encrypted = encryptMessage(msg);
    send(sock, encrypted.c_str(), encrypted.size(), 0);
}

// ================= DISPLAY =================

void displayScreen() {
    screenMutex.lock();
    system("cls");

    string cleanMarket = latestMarket;
    if(cleanMarket.find("[MARKET]") != string::npos) {
        cleanMarket.erase(cleanMarket.find("[MARKET]"), 9);
    }

    string cleanPort = latestPortfolio;
    if(cleanPort.find("[PORT]") != string::npos) {
        cleanPort.erase(cleanPort.find("[PORT]"), 7);
    }

    cout << "========== LIVE MARKET ==========\n\n";
    cout << cleanMarket << "\n";
    cout << "========== PORTFOLIO ==========\n\n";
    cout << cleanPort << "\n";
    cout << "========== MENU ==========\n";
    cout << "1. Buy Stock\n";
    cout << "2. Sell Stock\n";
    cout << "3. Refresh Portfolio\n";
    cout << "4. Exit\n";

    screenMutex.unlock();
}

// ================= RECEIVER THREAD =================

void receiverThread(SOCKET sock) {
    string pendingData = "";
    while(true) {
        char buffer[4096];
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if(bytes <= 0) break;

        string encrypted(buffer, bytes);
        string data = decryptMessage(encrypted);
        pendingData += data;

        size_t pos;
        while((pos = pendingData.find("<END>")) != string::npos) {
            string msg = pendingData.substr(0, pos);
            pendingData.erase(0, pos + 5);

            queueMutex.lock();
            if(msg.find("[MARKET]") != string::npos) {
                latestMarket = msg;
            }
            else if(msg.find("[PORT]") != string::npos) {
                latestPortfolio = msg;
            }
            else if(msg.find("[BUY]") != string::npos) {
                buyQueue.push(msg);
            }
            else if(msg.find("[SELL]") != string::npos) {
                sellQueue.push(msg);
            }
            else if(msg.find("[FAIL]") != string::npos) {
                failQueue.push(msg);
            }
            else {
                authQueue.push(msg);
            }
            queueMutex.unlock();
        }
    }
}

// ================= WAIT FUNCTIONS =================

string waitForBuyMessage() {
    while(true) {
        queueMutex.lock();
        if(!buyQueue.empty()) {
            string msg = buyQueue.front();
            buyQueue.pop();
            queueMutex.unlock();
            return msg;
        }
        queueMutex.unlock();
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

string waitForSellMessage() {
    while(true) {
        queueMutex.lock();
        if(!sellQueue.empty()) {
            string msg = sellQueue.front();
            sellQueue.pop();
            queueMutex.unlock();
            return msg;
        }
        queueMutex.unlock();
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

string waitForAuthMessage() {
    while(true) {
        queueMutex.lock();
        if(!authQueue.empty()) {
            string msg = authQueue.front();
            authQueue.pop();
            queueMutex.unlock();
            return msg;
        }
        queueMutex.unlock();
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

// ================= MARKET REFRESH =================

void refreshMarket(SOCKET sock) {
    while(true) {
        sendEncrypted(sock, "MARKET");
        sendEncrypted(sock, "PORT");
        this_thread::sleep_for(chrono::seconds(5));
    }
}

// ================= CLEAN RESPONSE =================

string cleanResponse(string response) {
    if(response.find("[BUY]") != string::npos) {
        response.erase(response.find("[BUY]"), 5);
    }
    if(response.find("[SELL]") != string::npos) {
        response.erase(response.find("[SELL]"), 6);
    }
    if(response.find("[FAIL]") != string::npos) {
        response.erase(response.find("[FAIL]"), 6);
    }
    return response;
}

// ================= MAIN =================

int main() {
    WSADATA ws;
    WSAStartup(MAKEWORD(2,2), &ws);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        cout << "Connection Failed!\n";
        return 0;
    }

    cout << "Connected to Stock Trading Server\n";
    thread(receiverThread, sock).detach();

    // AUTH
    while(true) {
        cout << "\n===== AUTH MENU =====\n";
        cout << "1. Login\n";
        cout << "2. Register\n";
        cout << "Choice: ";
        int ch; cin >> ch;
        string username, password;
        cout << "Username: "; cin >> username;
        cout << "Password: "; cin >> password;

        string msg;
        if(ch == 1) msg = "LOGIN " + username + " " + password;
        else if(ch == 2) msg = "REGISTER " + username + " " + password;
        else continue;

        sendEncrypted(sock, msg);
        string reply = waitForAuthMessage();
        cout << "\n" << cleanResponse(reply) << endl;

        if(reply.find("Login Success") != string::npos) break;
    }

    sendEncrypted(sock, "MARKET");
    sendEncrypted(sock, "PORT");
    this_thread::sleep_for(chrono::milliseconds(500));
    thread(refreshMarket, sock).detach();

    // MAIN LOOP
    while(true) {
        displayScreen();
        cout << "\nEnter Choice: ";
        int ch; cin >> ch;

        if(ch == 1) { // BUY
            string stock; int qty;
            cout << "\nStock Name: "; cin >> stock;
            cout << "Quantity: "; cin >> qty;
            string request = "BUY " + stock + " " + to_string(qty);

            sendEncrypted(sock, request);
            string response = waitForBuyMessage();
            cout << "\n" << cleanResponse(response) << endl;

            if(response.find("queued") != string::npos) {
                response = waitForBuyMessage();
                cout << "\n" << cleanResponse(response) << endl;
            }

            sendEncrypted(sock, "PORT");
            sendEncrypted(sock, "MARKET");
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        else if(ch == 2) { // SELL
            string stock; int qty;
            cout << "\nStock Name: "; cin >> stock;
            cout << "Quantity: "; cin >> qty;
            string request = "SELL " + stock + " " + to_string(qty);

            sendEncrypted(sock, request);
            string response = waitForSellMessage();
            cout << "\n" << cleanResponse(response) << endl;

            if(response.find("queued") != string::npos) {
                response = waitForSellMessage();
                cout << "\n" << cleanResponse(response) << endl;
            }

            sendEncrypted(sock, "PORT");
            sendEncrypted(sock, "MARKET");
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        else if(ch == 3) { // REFRESH
            sendEncrypted(sock, "PORT");
            sendEncrypted(sock, "MARKET");
            this_thread::sleep_for(chrono::milliseconds(500));
        }
        else if(ch == 4) {
            cout << "\nDisconnected.\n";
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
