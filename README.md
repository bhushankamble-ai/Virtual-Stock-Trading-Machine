# Real-Time Stock Trading System

A multi-threaded client-server based stock trading simulation built using **C++**, **Winsock Socket Programming**, and **TCP communication**.

---

# Features

- Client-Server Architecture
- Real-time stock market simulation
- BUY and SELL operations
- User Login & Registration
- Portfolio Management
- Live Market Updates
- Encrypted Communication
- Request Queue Handling
- Traffic Analysis Monitor
- Multi-threaded Server
- Transaction Logging
- Persistent User Data Storage

---

# Project Structure

```text
├── server.cpp
├── client.cpp
├── users.txt
├── server_logs.txt
└── README.md
```

---

# Available Stocks

- AAPL
- TSLA
- GOOG
- MSFT

---

# Encryption

```cpp
vector<int> cipherPattern = {2,5,5,14,23,23};
```

Messages are encrypted before sending and decrypted after receiving.

---

# Functionalities

## Authentication
- Register new users
- Login existing users

## Market Operations
- View live stock prices
- View available stock quantities

## Portfolio Operations
- Check balance
- View owned stocks

## Trading Operations
- Buy stocks
- Sell stocks

---

# How to Run

## Compile Server

```bash
g++ server.cpp -o server -lws2_32
```

## Compile Client

```bash
g++ client.cpp -o client -lws2_32
```

## Run Server

```bash
server.exe
```

## Run Client

```bash
client.exe
```

---

# Server Flowchart

```text
Start Server
      |
Load User Database
      |
Start Threads
      |
Accept Client Connections
      |
Authenticate User
      |
Process Requests
      |
Update Market & Portfolio
      |
Save Logs & Data
      |
Send Response
```

---

# Client Flowchart

```text
Start Client
      |
Connect to Server
      |
Login / Register
      |
Display Market & Portfolio
      |
Send BUY / SELL Requests
      |
Receive Responses
      |
Update Display
      |
Exit
```

---

# Advantages

- Supports multiple clients
- Real-time communication
- Secure message transfer
- Demonstrates networking concepts


---

# Conclusion

The Real-Time Stock Trading System demonstrates practical implementation of socket programming, multithreading, synchronization, and real-time communication using C++.
