#include <iostream>
#include <vector>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif
using namespace std;

struct Vehicle {
    string plateNumber;
    string ownerName;
    string type;
    time_t entryTime;
    time_t exitTime;
    bool isParked;

    Vehicle(string plate, string owner, string t) {
        plateNumber = plate;
        ownerName = owner;
        type = t;
        entryTime = time(nullptr);
        exitTime = 0;
        isParked = true;
    }
};

class ParkingLot {
private:
    vector<Vehicle> slots;
    int capacity;
    ofstream logFile;

public:
    ParkingLot(int cap) {
        capacity = cap;
        logFile.open("parking_log.txt", ios::app);
        if (!logFile) {
            cout << "[ERROR] Unable to open file!" << endl;
        }
    }

    ~ParkingLot() {
        if (logFile.is_open()) logFile.close();
    }

    bool parkVehicle(string plate, string owner, string type) {
        if (slots.size() >= capacity) {
            return false;
        }
        Vehicle v(plate, owner, type);
        slots.push_back(v);

        string entryTime = asctime(localtime(&v.entryTime));
        entryTime.pop_back();

        cout << "[INFO] Parked " << type << " " << plate 
             << " at slot " << slots.size() 
             << " (Entry: " << put_time(localtime(&v.entryTime), "%H:%M:%S") << ")" << endl;

        logFile << "[PARK] " << type << " " << plate 
                << " | Owner: " << owner
                << " | Entry: " << entryTime << endl;
        logFile.flush();
        return true;
    }

    double calculateFee(string plate) {
        for (const auto &v : slots) {
            if (v.plateNumber == plate && v.isParked) {
                time_t exitTime = time(nullptr);
                double hours = difftime(exitTime, v.entryTime) / 3600.0;
                if (hours < 1) hours = 1;
                double fee = 0;
                if (v.type == "Car") fee = hours * 20;
                else if (v.type == "Bike") fee = hours * 10;
                else if (v.type == "Truck") fee = hours * 30;
                return fee;
            }
        }
        return 0;
    }

    bool exitVehicle(string plate, double* feeOut = nullptr) {
        for (auto &v : slots) {
            if (v.plateNumber == plate && v.isParked) {
                v.exitTime = time(nullptr);
                v.isParked = false;

                double hours = difftime(v.exitTime, v.entryTime) / 3600.0;
                if (hours < 1) hours = 1;
                double fee = 0;

                if (v.type == "Car") fee = hours * 20;
                else if (v.type == "Bike") fee = hours * 10;
                else if (v.type == "Truck") fee = hours * 30;

                if (feeOut) *feeOut = fee;

                cout << "[INFO] Vehicle " << v.plateNumber 
                     << " leaving slot. Fee = Rs " << fixed << setprecision(2) << fee 
                     << " (Entry: " << put_time(localtime(&v.entryTime), "%H:%M:%S")
                     << " Exit: " << put_time(localtime(&v.exitTime), "%H:%M:%S") << ")" 
                     << endl;

                string entryTime = asctime(localtime(&v.entryTime));
                entryTime.pop_back();
                string exitTime = asctime(localtime(&v.exitTime));
                exitTime.pop_back();

                logFile << "[EXIT] " << v.type << " " << v.plateNumber 
                        << " | Owner: " << v.ownerName
                        << " | Entry: " << entryTime 
                        << " | Exit: " << exitTime
                        << " | Fee: Rs " << fixed << setprecision(2) << fee << endl;
                logFile.flush();
                return true;
            }
        }
        return false;
    }

    string getJSONData() {
        ostringstream json;
        json << "{\"capacity\":" << capacity << ",\"occupied\":" << slots.size() 
             << ",\"available\":" << (capacity - (int)slots.size()) << ",\"vehicles\":[";
        
        int numCars = 0, numBikes = 0, numTrucks = 0, numActive = 0;
        for (size_t i = 0; i < slots.size(); i++) {
            const auto &v = slots[i];
            if (v.type == "Car") numCars++;
            else if (v.type == "Bike") numBikes++;
            else if (v.type == "Truck") numTrucks++;
            if (v.isParked) numActive++;

            if (i > 0) json << ",";
            char entryBuf[32], exitBuf[32];
            strftime(entryBuf, sizeof(entryBuf), "%Y-%m-%d %H:%M:%S", localtime(&v.entryTime));
            if (v.exitTime != 0) {
                strftime(exitBuf, sizeof(exitBuf), "%Y-%m-%d %H:%M:%S", localtime(&v.exitTime));
            } else {
                strcpy(exitBuf, "-");
            }
            
            json << "{\"slot\":" << (i+1) << ",\"type\":\"" << v.type 
                 << "\",\"plate\":\"" << v.plateNumber << "\",\"owner\":\"" << v.ownerName
                 << "\",\"entry\":\"" << entryBuf << "\",\"exit\":\"" << exitBuf
                 << "\",\"parked\":" << (v.isParked ? "true" : "false") << "}";
        }
        
        json << "],\"stats\":{\"cars\":" << numCars << ",\"bikes\":" << numBikes 
             << ",\"trucks\":" << numTrucks << ",\"active\":" << numActive << "}}";
        return json.str();
    }

    string getDashboardHTML() {
        ostringstream html;
        int occupied = (int)slots.size();
        int available = capacity - occupied;
        int numCars = 0, numBikes = 0, numTrucks = 0, numActive = 0;
        for (const auto &v : slots) {
            if (v.type == "Car") numCars++;
            else if (v.type == "Bike") numBikes++;
            else if (v.type == "Truck") numTrucks++;
            if (v.isParked) numActive++;
        }
        double usage = capacity > 0 ? (100.0 * occupied / capacity) : 0.0;

        html << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Parking Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',Arial,sans-serif;background:#f6f8fb;color:#222;padding:20px}
.header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:24px;border-radius:12px;margin-bottom:24px;box-shadow:0 4px 15px rgba(0,0,0,.1)}
h1{font-size:32px;margin-bottom:8px}
.subtitle{opacity:.9;font-size:14px}
.container{max-width:1400px;margin:0 auto}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}
.card{background:#fff;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.08);padding:20px;transition:transform .2s}
.card:hover{transform:translateY(-2px);box-shadow:0 4px 15px rgba(0,0,0,.12)}
.card-label{color:#666;font-size:13px;text-transform:uppercase;letter-spacing:.5px;margin-bottom:8px}
.card-value{font-size:36px;font-weight:700;color:#333}
.usage-card{background:#fff;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.08);padding:20px;margin-bottom:24px}
.bar{height:16px;border-radius:8px;background:#e9eef7;overflow:hidden;margin-top:12px}
.fill{height:100%;background:linear-gradient(90deg,#667eea,#764ba2);transition:width .3s}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:20px;margin-bottom:24px}
.action-panel{background:#fff;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.08);padding:24px}
.action-panel h2{font-size:20px;margin-bottom:16px;color:#333}
.form-group{margin-bottom:16px}
label{display:block;margin-bottom:6px;color:#555;font-weight:500;font-size:14px}
input,select{width:100%;padding:10px 12px;border:2px solid #e0e0e0;border-radius:8px;font-size:14px;transition:border .2s}
input:focus,select:focus{outline:none;border-color:#667eea}
button{width:100%;padding:12px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;border:none;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;transition:opacity .2s}
button:hover{opacity:.9}
button:active{opacity:.8}
.vehicles-table{background:#fff;border-radius:12px;box-shadow:0 2px 10px rgba(0,0,0,.08);overflow:hidden}
table{width:100%;border-collapse:collapse}
th,td{padding:14px 16px;text-align:left;border-bottom:1px solid #f0f0f0}
th{background:#f8f9fa;font-weight:600;color:#555;font-size:13px;text-transform:uppercase;letter-spacing:.5px}
tr:hover{background:#f8f9fa}
.status{font-size:12px;padding:4px 12px;border-radius:20px;display:inline-block;font-weight:500}
.status-parked{background:#e7f7ee;color:#0f7b3f}
.status-exited{background:#fdecec;color:#b42318}
.message{padding:12px 16px;border-radius:8px;margin-bottom:16px;font-size:14px;display:none}
.message-success{background:#e7f7ee;color:#0f7b3f;border:1px solid #a8e6c7}
.message-error{background:#fdecec;color:#b42318;border:1px solid #f5a5a5}
.refresh-btn{position:fixed;bottom:24px;right:24px;width:60px;height:60px;border-radius:50%;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;border:none;font-size:24px;cursor:pointer;box-shadow:0 4px 15px rgba(0,0,0,.2);transition:transform .2s}
.refresh-btn:hover{transform:rotate(180deg) scale(1.1)}
</style>
</head>
<body>
<div class="container">
<div class="header">
<h1>🚗 Smart Parking System</h1>
<p class="subtitle">Real-time Parking Management Dashboard</p>
</div>

<div id="message" class="message"></div>

<div class="cards">
<div class="card">
<div class="card-label">Total Capacity</div>
<div class="card-value" id="capacity">)" << capacity << R"(</div>
</div>
<div class="card">
<div class="card-label">Occupied</div>
<div class="card-value" id="occupied">)" << occupied << R"(</div>
</div>
<div class="card">
<div class="card-label">Available</div>
<div class="card-value" id="available">)" << available << R"(</div>
</div>
<div class="card">
<div class="card-label">Cars</div>
<div class="card-value" id="cars">)" << numCars << R"(</div>
</div>
<div class="card">
<div class="card-label">Bikes</div>
<div class="card-value" id="bikes">)" << numBikes << R"(</div>
</div>
<div class="card">
<div class="card-label">Trucks</div>
<div class="card-value" id="trucks">)" << numTrucks << R"(</div>
</div>
<div class="card">
<div class="card-label">Active</div>
<div class="card-value" id="active">)" << numActive << R"(</div>
</div>
</div>

<div class="usage-card">
<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
<span class="card-label">Parking Usage</span>
<span style="font-weight:600;color:#333" id="usage-percent">)" << fixed << setprecision(0) << usage << R"(%</span>
</div>
<div class="bar"><div class="fill" id="usage-bar" style="width:)" << usage << R"(%"></div></div>
</div>

<div class="actions">
<div class="action-panel">
<h2>🚙 Park Vehicle</h2>
<form id="parkForm" onsubmit="parkVehicle(event)">
<div class="form-group">
<label>Vehicle Type</label>
<select name="type" required>
<option value="">Select Type</option>
<option value="Car">Car (Rs 20/hr)</option>
<option value="Bike">Bike (Rs 10/hr)</option>
<option value="Truck">Truck (Rs 30/hr)</option>
</select>
</div>
<div class="form-group">
<label>Plate Number</label>
<input type="text" name="plate" placeholder="e.g., MH12AB1234" required>
</div>
<div class="form-group">
<label>Owner Name</label>
<input type="text" name="owner" placeholder="Enter owner name" required>
</div>
<button type="submit">Park Vehicle</button>
</form>
</div>

<div class="action-panel">
<h2>🚪 Exit Vehicle</h2>
<form id="exitForm" onsubmit="exitVehicle(event)">
<div class="form-group">
<label>Plate Number</label>
<input type="text" name="plate" placeholder="Enter plate number" required>
</div>
<button type="submit">Exit Vehicle</button>
</form>
</div>
</div>

<div class="vehicles-table">
<h2 style="padding:20px 20px 0;font-size:20px;color:#333">📋 Vehicle List</h2>
<table>
<thead>
<tr>
<th>#</th>
<th>Type</th>
<th>Plate Number</th>
<th>Owner</th>
<th>Entry Time</th>
<th>Status</th>
<th>Exit Time</th>
</tr>
</thead>
<tbody id="vehicles-table-body">
)HTML";

        for (size_t i = 0; i < slots.size(); ++i) {
            const auto &v = slots[i];
            char entryBuf[32];
            strftime(entryBuf, sizeof(entryBuf), "%Y-%m-%d %H:%M:%S", localtime(&v.entryTime));
            string exitStr = "-";
            if (v.exitTime != 0) {
                char exitBuf[32];
                strftime(exitBuf, sizeof(exitBuf), "%Y-%m-%d %H:%M:%S", localtime(&v.exitTime));
                exitStr = exitBuf;
            }
            html << "<tr><td>" << (i+1) << "</td><td>" << v.type << "</td><td>" << v.plateNumber 
                 << "</td><td>" << v.ownerName << "</td><td>" << entryBuf << "</td><td>"
                 << (v.isParked ? "<span class=\"status status-parked\">Parked</span>" 
                     : "<span class=\"status status-exited\">Exited</span>")
                 << "</td><td>" << exitStr << "</td></tr>\n";
        }

        html << R"HTML(
</tbody>
</table>
</div>
</div>

<button class="refresh-btn" onclick="refreshData()" title="Refresh">🔄</button>

<script>
function showMessage(text, isError) {
    const msg = document.getElementById('message');
    msg.textContent = text;
    msg.className = 'message ' + (isError ? 'message-error' : 'message-success');
    msg.style.display = 'block';
    setTimeout(() => msg.style.display = 'none', 3000);
}

function parkVehicle(e) {
    e.preventDefault();
    const form = e.target;
    const formData = new FormData(form);
    const data = Object.fromEntries(formData);
    
    fetch('/park', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: new URLSearchParams(data)
    })
    .then(r => r.json())
    .then(result => {
        if (result.success) {
            showMessage('Vehicle parked successfully!', false);
            form.reset();
            refreshData();
        } else {
            showMessage(result.message || 'Failed to park vehicle', true);
        }
    })
    .catch(err => showMessage('Error: ' + err, true));
}

function exitVehicle(e) {
    e.preventDefault();
    const form = e.target;
    const formData = new FormData(form);
    const plate = formData.get('plate');
    
    fetch('/exit', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'plate=' + encodeURIComponent(plate)
    })
    .then(r => r.json())
    .then(result => {
        if (result.success) {
            showMessage('Vehicle exited successfully! Fee: Rs ' + result.fee, false);
            form.reset();
            refreshData();
            } else {
            showMessage(result.message || 'Vehicle not found', true);
        }
    })
    .catch(err => showMessage('Error: ' + err, true));
}

function refreshData() {
    fetch('/data')
    .then(r => r.json())
    .then(data => {
        document.getElementById('capacity').textContent = data.capacity;
        document.getElementById('occupied').textContent = data.occupied;
        document.getElementById('available').textContent = data.available;
        document.getElementById('cars').textContent = data.stats.cars;
        document.getElementById('bikes').textContent = data.stats.bikes;
        document.getElementById('trucks').textContent = data.stats.trucks;
        document.getElementById('active').textContent = data.stats.active;
        
        const usage = data.capacity > 0 ? (100 * data.occupied / data.capacity) : 0;
        document.getElementById('usage-percent').textContent = usage.toFixed(0) + '%';
        document.getElementById('usage-bar').style.width = usage + '%';
        
        const tbody = document.getElementById('vehicles-table-body');
        tbody.innerHTML = data.vehicles.map(v => {
            const status = v.parked ? 
                '<span class=\"status status-parked\">Parked</span>' : 
                '<span class=\"status status-exited\">Exited</span>';
            return '<tr>' +
                '<td>' + v.slot + '</td>' +
                '<td>' + v.type + '</td>' +
                '<td>' + v.plate + '</td>' +
                '<td>' + v.owner + '</td>' +
                '<td>' + v.entry + '</td>' +
                '<td>' + status + '</td>' +
                '<td>' + v.exit + '</td>' +
            '</tr>';
        }).join('');
    })
    .catch(err => console.error('Refresh error:', err));
}

setInterval(refreshData, 2000);
</script>
</body>
</html>)HTML";
        return html.str();
    }
};

class WebServer {
private:
    ParkingLot* parkingLot;
    int port;
#ifdef _WIN32
    SOCKET serverSocket;
#else
    int serverSocket;
#endif

    string urlDecode(const string& str) {
        string result;
        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value;
                sscanf(str.substr(i + 1, 2).c_str(), "%x", &value);
                result += (char)value;
                i += 2;
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }

    map<string, string> parseFormData(const string& data) {
        map<string, string> result;
        istringstream stream(data);
        string pair;
        while (getline(stream, pair, '&')) {
            size_t pos = pair.find('=');
            if (pos != string::npos) {
                string key = urlDecode(pair.substr(0, pos));
                string value = urlDecode(pair.substr(pos + 1));
                result[key] = value;
            }
        }
        return result;
    }

    void sendResponse(
#ifdef _WIN32
        SOCKET clientSocket
#else
        int clientSocket
#endif
        , const string& content, const string& contentType = "text/html") {
        ostringstream response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: " << contentType << "; charset=utf-8\r\n"
                 << "Content-Length: " << content.length() << "\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Connection: close\r\n\r\n"
                 << content;
        
        string responseStr = response.str();
#ifdef _WIN32
        send(clientSocket, responseStr.c_str(), (int)responseStr.length(), 0);
        closesocket(clientSocket);
#else
        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
        close(clientSocket);
#endif
    }

    void handleRequest(
#ifdef _WIN32
        SOCKET clientSocket
#else
        int clientSocket
#endif
        , const string& request) {
        if (request.find("GET / ") != string::npos || request.find("GET / HTTP") != string::npos) {
            sendResponse(clientSocket, parkingLot->getDashboardHTML());
        }
        else if (request.find("GET /data") != string::npos) {
            sendResponse(clientSocket, parkingLot->getJSONData(), "application/json");
        }
        else if (request.find("POST /park") != string::npos) {
            size_t bodyPos = request.find("\r\n\r\n");
            if (bodyPos != string::npos) {
                string body = request.substr(bodyPos + 4);
                auto formData = parseFormData(body);
                
                bool success = parkingLot->parkVehicle(
                    formData["plate"], 
                    formData["owner"], 
                    formData["type"]
                );
                
                ostringstream json;
                json << "{\"success\":" << (success ? "true" : "false") 
                     << ",\"message\":\"" << (success ? "Vehicle parked successfully" : "Parking lot full") << "\"}";
                sendResponse(clientSocket, json.str(), "application/json");
            } else {
                ostringstream json;
                json << "{\"success\":false,\"message\":\"Invalid request body\"}";
                sendResponse(clientSocket, json.str(), "application/json");
            }
        }
        else if (request.find("POST /exit") != string::npos) {
            size_t bodyPos = request.find("\r\n\r\n");
            if (bodyPos != string::npos) {
                string body = request.substr(bodyPos + 4);
                auto formData = parseFormData(body);
                
                // Calculate fee before exit
                double fee = parkingLot->calculateFee(formData["plate"]);
                double actualFee = 0;
                bool success = parkingLot->exitVehicle(formData["plate"], &actualFee);
                
                ostringstream json;
                json << "{\"success\":" << (success ? "true" : "false") 
                     << ",\"message\":\"" << (success ? "Vehicle exited successfully" : "Vehicle not found") 
                     << "\",\"fee\":" << fixed << setprecision(2) << (success ? actualFee : 0) << "}";
                sendResponse(clientSocket, json.str(), "application/json");
            } else {
                ostringstream json;
                json << "{\"success\":false,\"message\":\"Invalid request body\"}";
                sendResponse(clientSocket, json.str(), "application/json");
            }
        }
        else {
            sendResponse(clientSocket, "404 Not Found", "text/plain");
        }
    }

public:
    WebServer(ParkingLot* lot, int p) : parkingLot(lot), port(p) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
#else
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
#endif
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        int opt = 1;
#ifdef _WIN32
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        listen(serverSocket, 5);
#else
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        listen(serverSocket, 5);
#endif
    }

    void run() {
        cout << "[INFO] Web server started on http://localhost:" << port << endl;
        cout << "[INFO] Dashboard will open automatically in your browser..." << endl;
        
        // Open browser
#ifdef _WIN32
        string url = "http://localhost:" + to_string(port);
        system(("start \"\" \"" + url + "\"").c_str());
#elif __APPLE__
        system(("open http://localhost:" + to_string(port)).c_str());
#else
        system(("xdg-open http://localhost:" + to_string(port)).c_str());
#endif
        
        while (true) {
            sockaddr_in clientAddr;
#ifdef _WIN32
            int clientLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
            if (clientSocket == INVALID_SOCKET) continue;
#else
            socklen_t clientLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0) continue;
#endif
            
            char buffer[4096] = {0};
#ifdef _WIN32
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
#else
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
#endif
            
            if (bytesRead > 0) {
                string request(buffer, bytesRead);
                handleRequest(clientSocket, request);
            }
        }
    }
};

// Need to expose slots for fee calculation - add a getter
// Actually, let me fix this by making exitVehicle return the fee
// Wait, I need to refactor this better. Let me add a method to get slots.
// Actually, for simplicity, let me calculate fee in the web server after calling exitVehicle
// But exitVehicle modifies the vehicle, so I need to calculate before or return fee
// Let me modify exitVehicle to return fee, or add a getFee method

// Actually, the simplest is to calculate fee in handleRequest before calling exitVehicle
// But I can't access the vehicle before it's exited. Let me add a method to calculate fee for a plate.

// For now, let me just calculate it approximately or add a helper method
// Actually, let me modify the ParkingLot class to have a helper method

int main() {
    ParkingLot lot(10);
    
    cout << "\n========================================" << endl;
    cout << "   Smart Parking System - Web Server" << endl;
    cout << "========================================" << endl;
    
    WebServer server(&lot, 8080);
    server.run();
    
    return 0;
}
