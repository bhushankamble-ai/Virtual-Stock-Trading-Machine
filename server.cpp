// server.cpp

#include <iostream>
#include <winsock2.h>
#include <thread>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <ctime>
#include <queue>
#include <chrono>
#include <vector>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

// ================= ENCRYPTION =================

vector<int> cipherPattern = {2,5,5,14,23,23};

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

// ================= USER STRUCT =================

struct User {
    string username;
    string password;
    double balance = 10000;
    map<string,int> stocks;
};

// ================= STOCK STRUCT =================

struct Stock {
    double price;
    int available;
};

// ================= GLOBAL VARIABLES =================

map<string, User> usersDB;
map<SOCKET, string> loggedClients;

map<string, Stock> market = {
    {"AAPL", {150, 10}},
    {"TSLA", {220, 10}},
    {"GOOG", {2800, 10}},
    {"MSFT", {320, 10}}
};

mutex mtx;
queue<pair<SOCKET,string>> requestQueue;

// ================= TRAFFIC STATS =================

int totalPackets = 0;
int totalBuyRequests = 0;
int totalSellRequests = 0;
int totalFailedRequests = 0;

// ================= SEND =================

void sendMsg(SOCKET s, string msg) {
    msg += "<END>";
    string encrypted = encryptMessage(msg);
    send(s, encrypted.c_str(), encrypted.size(), 0);
}

// ================= LOGGING =================

void logRequest(string username, string request, string status) {
    ofstream file("server_logs.txt", ios::app);
    time_t now = time(0);
    string dt = ctime(&now);
    dt.pop_back();
    file << "[" << dt << "] " << username << " -> " << request << " -> " << status << "\n";
}

// ================= SAVE USERS =================

void saveUsers() {
    ofstream file("users.txt");
    for(auto &u : usersDB) {
        file << u.second.username << "," << u.second.password << "," << u.second.balance << ",";
        bool first = true;
        for(auto &s : u.second.stocks) {
            if(!first) file << "|";
            file << s.first << ":" << s.second;
            first = false;
        }
        file << "\n";
    }
}

// ================= LOAD USERS =================

void loadUsers() {
    ifstream file("users.txt");
    string line;
    while(getline(file, line)) {
        stringstream ss(line);
        string user, pass, bal, hold;
        getline(ss, user, ',');
        getline(ss, pass, ',');
        getline(ss, bal, ',');
        getline(ss, hold);

        User u;
        u.username = user;
        u.password = pass;
        u.balance = stod(bal);

        stringstream hs(hold);
        string token;
        while(getline(hs, token, '|')) {
            if(token == "") continue;
            int pos = token.find(':');
            string stock = token.substr(0, pos);
            int qty = stoi(token.substr(pos+1));
            u.stocks[stock] = qty;
        }
        usersDB[user] = u;
    }
}

// ================= SAVE MARKET =================

void saveMarket() {
    ofstream file("market.txt");
    for(auto &m : market) {
        file << m.first << "," << m.second.price << "," << m.second.available << "\n";
    }
}

// ================= LOAD MARKET =================

void loadMarket() {
    ifstream file("market.txt");
    if(!file.is_open()) return; // If no file, keep defaults

    string line;
    while(getline(file, line)) {
        stringstream ss(line);
        string symbol, priceStr, availStr;
        getline(ss, symbol, ',');
        getline(ss, priceStr, ',');
        getline(ss, availStr, ',');

        double price = stod(priceStr);
        int available = stoi(availStr);

        market[symbol] = {price, available};
    }
}

// ================= MARKET VIEW =================

string marketView() {
    stringstream ss;
    ss << "===== MARKET =====\n";
    for(auto &x : market) {
        ss << x.first
           << " : $" << x.second.price
           << " | Available: " << x.second.available
           << "\n";
    }
    return ss.str();
}

// ================= PORTFOLIO =================

string portfolio(User &u) {
    stringstream ss;
    ss << "===== PORTFOLIO =====\n";
    ss << "User: " << u.username << "\n";
    ss << "Balance: $" << u.balance << "\n";
    for(auto &x : u.stocks) {
        ss << x.first << " : " << x.second << "\n";
    }
    return ss.str();
}

// ================= UPDATE MARKET =================

void updatePrices() {
    while(true) {
        this_thread::sleep_for(chrono::seconds(15));
        mtx.lock();
        for(auto &x : market) {
            int change = rand()%5 - 2;
            x.second.price += change;
            if(x.second.price < 10) x.second.price = 10;
        }
        saveMarket(); // persist price changes
        mtx.unlock();
    }
}

// ================= LOGIN / REGISTER =================

bool loginUser(string user, string pass) {
    if(usersDB.count(user) == 0) return false;
    return usersDB[user].password == pass;
}

bool registerUser(string user, string pass) {
    if(usersDB.count(user)) return false;
    User u;
    u.username = user;
    u.password = pass;
    usersDB[user] = u;
    saveUsers();
    return true;
}

// ================= PROCESS QUEUE =================

void processQueue() {
    while(true) {
        if(!requestQueue.empty()) {
            mtx.lock();
            auto req = requestQueue.front();
            requestQueue.pop();
            mtx.unlock();

            SOCKET client = req.first;
            string msg = req.second;
            totalPackets++;

            string username = loggedClients[client];
            User &u = usersDB[username];

            stringstream ss(msg);
            string cmd;
            ss >> cmd;

            // Random failure simulation
            int fail = rand()%10;
            if(fail == 0) {
                totalFailedRequests++;
                sendMsg(client, "[FAIL]\nREQUEST FAILED");
                logRequest(username, msg, "FAILED");
                continue;
            }

            if(cmd == "MARKET") {
                sendMsg(client, "[MARKET]\n" + marketView());
                logRequest(username, msg, "SUCCESS");
            }
            else if(cmd == "PORT") {
                sendMsg(client, "[PORT]\n" + portfolio(u));
                logRequest(username, msg, "SUCCESS");
            }
            else if(cmd == "BUY") {
                totalBuyRequests++;
                sendMsg(client, "[BUY]\nBUY request queued...");
                this_thread::sleep_for(chrono::seconds(3));

                string stock; int qty;
                ss >> stock >> qty;

                if(market.find(stock) == market.end()) {
                    sendMsg(client, "[BUY]\nINVALID STOCK SYMBOL");
                    logRequest(username, msg, "FAILED");
                    continue;
                }

                double cost = market[stock].price * qty;

                if(market[stock].available < qty) {
                    sendMsg(client, "[BUY]\nNOT ENOUGH STOCKS AVAILABLE");
                    logRequest(username, msg, "FAILED");
                }
                else if(u.balance >= cost) {
                    u.balance -= cost;
                    u.stocks[stock] += qty;
                    market[stock].available -= qty;
                    saveUsers();
                    saveMarket();
                    sendMsg(client, "[BUY]\nBUY SUCCESS");
                    sendMsg(client, "[PORT]\n" + portfolio(u));
                    sendMsg(client, "[MARKET]\n" + marketView());
                    logRequest(username, msg, "SUCCESS");
                }
                else {
                    sendMsg(client, "[BUY]\nINSUFFICIENT BALANCE");
                    logRequest(username, msg, "FAILED");
                }
            }
            else if(cmd == "SELL") {
                totalSellRequests++;
                sendMsg(client, "[SELL]\nSELL request queued...");
                this_thread::sleep_for(chrono::seconds(3));

                string stock; int qty;
                ss >> stock >> qty;

                if(market.find(stock) == market.end()) {
                    sendMsg(client, "[SELL]\nINVALID STOCK SYMBOL");
                    logRequest(username, msg, "FAILED");
                    continue;
                }

                if(u.stocks[stock] >= qty) {
                                        u.stocks[stock] -= qty;
                    u.balance += market[stock].price * qty;
                    market[stock].available += qty;
                    saveUsers();
                    saveMarket();   // <-- persist changes
                    sendMsg(client, "[SELL]\nSELL SUCCESS");
                    sendMsg(client, "[PORT]\n" + portfolio(u));
                    sendMsg(client, "[MARKET]\n" + marketView());
                    logRequest(username, msg, "SUCCESS");
                }
                else {
                    sendMsg(client, "[SELL]\nNOT ENOUGH SHARES");
                    logRequest(username, msg, "FAILED");
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

// ================= CLIENT HANDLER =================

void clientHandler(SOCKET client) {
    char buffer[4096];

    // LOGIN / REGISTER
    while(true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client, buffer, sizeof(buffer), 0);
        if(bytes <= 0) {
            closesocket(client);
            return;
        }

        string encrypted(buffer, bytes);
        string msg = decryptMessage(encrypted);

        stringstream ss(msg);
        string cmd, user, pass;
        ss >> cmd >> user >> pass;

        mtx.lock();
        if(cmd == "LOGIN") {
            if(loginUser(user, pass)) {
                loggedClients[client] = user;
                sendMsg(client, "[LOGIN]\nLogin Success");
                mtx.unlock();
                break;
            } else {
                sendMsg(client, "[LOGIN]\nInvalid Login");
            }
        }
        else if(cmd == "REGISTER") {
            if(registerUser(user, pass)) {
                sendMsg(client, "[REGISTER]\nRegister Success");
            } else {
                sendMsg(client, "[REGISTER]\nUser Exists");
            }
        }
        mtx.unlock();
    }

    // ================= REQUEST LOOP =================
    while(true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client, buffer, sizeof(buffer), 0);
        if(bytes <= 0) break;

        string encrypted(buffer, bytes);
        string msg = decryptMessage(encrypted);

        mtx.lock();
        requestQueue.push({client, msg});
        mtx.unlock();
    }

    mtx.lock();
    loggedClients.erase(client);
    mtx.unlock();

    closesocket(client);
}

// ================= TRAFFIC ANALYSIS =================

void trafficAnalysisMonitor() {
    while(true) {
        system("cls");
        cout << "\n========== SERVER TRAFFIC ANALYSIS ==========\n";
        cout << "Active Clients: " << loggedClients.size() << "\n";
        cout << "Total Packets: " << totalPackets << "\n";
        cout << "BUY Requests: " << totalBuyRequests << "\n";
        cout << "SELL Requests: " << totalSellRequests << "\n";
        cout << "Failed Requests: " << totalFailedRequests << "\n";
        cout << "Pending Queue Requests: " << requestQueue.size() << "\n";

        cout << "\n========== MARKET ==========\n";
        for(auto &x : market) {
            cout << x.first
                 << " : $" << x.second.price
                 << " | Available: " << x.second.available
                 << "\n";
        }

        cout << "\nServer Running...\n";
        this_thread::sleep_for(chrono::seconds(2));
    }
}

// ================= MAIN =================

int main() {
    srand(time(0));
    loadUsers();
    loadMarket();   // <-- load saved market state

    WSADATA ws;
    WSAStartup(MAKEWORD(2,2), &ws);

    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    bind(serverSock, (sockaddr*)&server, sizeof(server));
    listen(serverSock, 5);

    cout << "Server Running on Port 8080...\n";

    thread(updatePrices).detach();
    thread(processQueue).detach();
    thread(trafficAnalysisMonitor).detach();

    while(true) {
        SOCKET client;
        sockaddr_in c;
        int sz = sizeof(c);

        client = accept(serverSock, (sockaddr*)&c, &sz);
        thread(clientHandler, client).detach();
    }

    saveMarket();   // <-- save before shutdown
    closesocket(serverSock);
    WSACleanup();
    return 0;
}
