
#include "BankSystem.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <map>
#include <fstream>
#include <filesystem>

using namespace std;

static string urlDecode(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int v = 0;
            string hex = s.substr(i + 1, 2);
            sscanf(hex.c_str(), "%x", &v);
            out += char(v);
            i += 2;
        } else if (s[i] == '+') out += ' ';
        else out += s[i];
    }
    return out;
}

static map<string,string> formDecode(const string& body) {
    map<string,string> m;
    stringstream ss(body);
    string part;

    while (getline(ss, part, '&')) {
        auto pos = part.find('=');
        if (pos == string::npos) continue;
        m[urlDecode(part.substr(0, pos))] = urlDecode(part.substr(pos + 1));
    }
    return m;
}

static string jsonEscape(const string& s) {
    string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

static string cardJson(const Card& c, bool isCredit) {
    string masked = c.number;
    if (masked.size() > 4)
        masked = "•••• •••• •••• " + masked.substr(masked.size() - 4);

    stringstream ss;
    ss << "{"
       << "\"exists\":" << (!c.number.empty() ? "true" : "false") << ","
       << "\"maskedNumber\":\"" << jsonEscape(masked) << "\","
       << "\"blocked\":" << (c.blocked ? "true" : "false") << ","
       << "\"onlineEnabled\":" << (c.onlineEnabled ? "true" : "false") << ","
       << "\"contactlessEnabled\":" << (c.contactlessEnabled ? "true" : "false") << ","
       << "\"atmEnabled\":" << (c.atmEnabled ? "true" : "false") << ","
       << "\"internationalEnabled\":" << (c.internationalEnabled ? "true" : "false") << ","
       << "\"dailyLimit\":" << c.dailyLimit;

    if (isCredit) {
        ss << ",\"creditLimit\":" << c.creditLimit
           << ",\"outstanding\":" << c.outstanding;
    }

    ss << "}";
    return ss.str();
}

static string accountJson(const Account& a) {
    stringstream ss;
    ss << "{"
       << "\"accountNo\":\"" << jsonEscape(a.accountNo) << "\","
       << "\"name\":\"" << jsonEscape(a.name) << "\","
       << "\"mobile\":\"" << jsonEscape(a.mobile) << "\","
       << "\"email\":\"" << jsonEscape(a.email) << "\","
       << "\"address\":\"" << jsonEscape(a.address) << "\","
       << "\"type\":\"" << jsonEscape(a.accountType) << "\","
       << "\"createdAt\":\"" << jsonEscape(a.createdAt) << "\","
       << "\"balance\":" << a.balance << ","
       << "\"frozen\":" << (a.frozen ? "true" : "false") << ","
       << "\"debitCard\":" << cardJson(a.debitCard, false) << ","
       << "\"creditCard\":" << cardJson(a.creditCard, true)
       << "}";
    return ss.str();
}

static string transactionsJson(const Account& a) {
    stringstream ss;
    ss << "[";
    for (size_t i = 0; i < a.transactions.size(); ++i) {
        const auto& t = a.transactions[i];
        if (i) ss << ",";
        ss << "{"
           << "\"id\":\"" << jsonEscape(t.id) << "\","
           << "\"type\":\"" << jsonEscape(t.type) << "\","
           << "\"amount\":" << t.amount << ","
           << "\"balanceAfter\":" << t.balanceAfter << ","
           << "\"time\":\"" << jsonEscape(t.time) << "\","
           << "\"remark\":\"" << jsonEscape(t.remark) << "\""
           << "}";
    }
    ss << "]";
    return ss.str();
}

static string investmentsJson(const Account& a) {
    stringstream ss;
    ss << "[";
    bool first = true;
    for (const auto& inv : a.investments) {
        if (!inv.active) continue;
        if (!first) ss << ",";
        first = false;
        ss << "{"
           << "\"id\":\"" << jsonEscape(inv.id) << "\","
           << "\"symbol\":\"" << jsonEscape(inv.symbol) << "\","
           << "\"name\":\"" << jsonEscape(inv.name) << "\","
           << "\"units\":" << inv.units << ","
           << "\"buyPrice\":" << inv.buyPrice << ","
           << "\"currentPrice\":" << inv.currentPrice << ","
           << "\"purchasedAt\":\"" << jsonEscape(inv.purchasedAt) << "\","
           << "\"value\":" << (inv.units * inv.currentPrice) << ","
           << "\"gain\":" << (inv.units * (inv.currentPrice - inv.buyPrice))
           << "}";
    }
    ss << "]";
    return ss.str();
}

static string jsonMessage(bool ok, const string& message) {
    return string("{\"ok\":") + (ok ? "true" : "false") +
           ",\"message\":\"" + jsonEscape(message) + "\"}";
}

static bool hasSuffix(const string& s, const string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static string contentType(const string& path) {
    if (hasSuffix(path, ".html")) return "text/html; charset=UTF-8";
    if (hasSuffix(path, ".css")) return "text/css; charset=UTF-8";
    if (hasSuffix(path, ".js")) return "application/javascript; charset=UTF-8";
    if (hasSuffix(path, ".svg")) return "image/svg+xml";
    if (hasSuffix(path, ".png")) return "image/png";
    return "text/plain; charset=UTF-8";
}

static string staticFile(const string& path) {
    string clean = path;
    if (clean == "/") clean = "/index.html";
    if (clean.find("..") != string::npos) return "";

    string full = "frontend" + clean;
    ifstream in(full, ios::binary);
    if (!in) return "";

    return string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
}

static void sendResponse(int fd, int code, const string& type, const string& body) {
    string status = code == 200 ? "200 OK" : code == 404 ? "404 Not Found" : "400 Bad Request";
    string response =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + type + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: " + to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;

    send(fd, response.c_str(), response.size(), 0);
}

static bool splitRequest(const string& req, string& method, string& path, string& body) {
    auto lineEnd = req.find("\r\n");
    if (lineEnd == string::npos) return false;

    string first = req.substr(0, lineEnd);
    stringstream ls(first);
    ls >> method >> path;

    auto headerEnd = req.find("\r\n\r\n");
    if (headerEnd != string::npos)
        body = req.substr(headerEnd + 4);
    else
        body = "";

    return true;
}

int main() {
    filesystem::create_directories("data");

    BankSystem bank;

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        cerr << "socket() failed\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "bind() failed: " << strerror(errno) << "\n";
        return 1;
    }

    if (listen(serverFd, 20) < 0) {
        cerr << "listen() failed\n";
        return 1;
    }

    cout << "Smart Bank running on port 8080\n";

    while (true) {
        int client = accept(serverFd, nullptr, nullptr);
        if (client < 0) continue;

        string req;
        char buf[8192];

        ssize_t n;
        while ((n = recv(client, buf, sizeof(buf), 0)) > 0) {
            req.append(buf, n);
            if ((size_t)n < sizeof(buf)) break;
            if (req.size() > 1024 * 1024) break;
        }

        string method, path, body;
        if (!splitRequest(req, method, path, body)) {
            sendResponse(client, 400, "application/json", jsonMessage(false, "Bad request"));
            close(client);
            continue;
        }

        // ---------- API ----------
        if (method == "POST" && path == "/api/register") {
            auto f = formDecode(body);
            Account a;
            a.name = f["name"];
            a.mobile = f["mobile"];
            a.email = f["email"];
            a.address = f["address"];
            a.username = f["username"];
            a.password = f["password"];
            try { a.balance = stod(f["openingDeposit"]); } catch (...) { a.balance = 0; }

            string error;
            string accountNo = bank.createAccount(a, error);

            if (accountNo.empty())
                sendResponse(client, 400, "application/json", jsonMessage(false, error));
            else
                sendResponse(client, 200, "application/json",
                    "{\"ok\":true,\"accountNo\":\"" + accountNo + "\",\"message\":\"Account created successfully\"}");
        }
        else if (method == "POST" && path == "/api/login") {
            auto f = formDecode(body);
            string error;

            if (!bank.login(f["accountNo"], f["username"], f["password"], error)) {
                sendResponse(client, 400, "application/json", jsonMessage(false, error));
            } else {
                Account* a = bank.getAccount(f["accountNo"]);
                sendResponse(client, 200, "application/json",
                    string("{\"ok\":true,\"message\":\"Login successful\",\"account\":") +
                    accountJson(*a) + "}");
            }
        }
        else if (method == "GET" && path.rfind("/api/account?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            Account* a = bank.getAccount(acc);
            if (!a)
                sendResponse(client, 404, "application/json", jsonMessage(false, "Account not found"));
            else
                sendResponse(client, 200, "application/json",
                    "{\"ok\":true,\"account\":" + accountJson(*a) + "}");
        }
        else if (method == "GET" && path.rfind("/api/transactions?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            Account* a = bank.getAccount(acc);
            if (!a)
                sendResponse(client, 404, "application/json", jsonMessage(false, "Account not found"));
            else
                sendResponse(client, 200, "application/json",
                    "{\"ok\":true,\"transactions\":" + transactionsJson(*a) + "}");
        }
        else if (method == "POST" && path == "/api/deposit") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}
            string msg;
            bool ok = bank.deposit(f["accountNo"], amount, msg);
            if (!ok)
                sendResponse(client, 400, "application/json", jsonMessage(false, msg));
            else {
                Account* a = bank.getAccount(f["accountNo"]);
                sendResponse(client, 200, "application/json",
                    "{\"ok\":true,\"message\":\"" + jsonEscape(msg) +
                    "\",\"account\":" + accountJson(*a) + "}");
            }
        }
        else if (method == "POST" && path == "/api/withdraw") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}
            string msg;
            bool ok = bank.withdraw(f["accountNo"], amount, msg);
            if (!ok)
                sendResponse(client, 400, "application/json", jsonMessage(false, msg));
            else {
                Account* a = bank.getAccount(f["accountNo"]);
                sendResponse(client, 200, "application/json",
                    "{\"ok\":true,\"message\":\"" + jsonEscape(msg) +
                    "\",\"account\":" + accountJson(*a) + "}");
            }
        }
        else if (method == "POST" && path == "/api/transfer") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}
            string msg;
            bool ok = bank.transfer(f["from"], f["to"], amount, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/qr/create") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}

            QRTransaction q;
            string msg;
            bool ok = bank.createQR(f["accountNo"], amount, q, msg);

            if (!ok) {
                sendResponse(client, 400, "application/json", jsonMessage(false, msg));
            } else {
                stringstream js;
                js << "{\"ok\":true,\"message\":\"" << jsonEscape(msg)
                   << "\",\"qrId\":\"" << q.qrId
                   << "\",\"otp\":\"" << q.otp
                   << "\",\"amount\":" << q.amount
                   << ",\"accountNo\":\"" << q.accountNo << "\"}";
                sendResponse(client, 200, "application/json", js.str());
            }
        }
        else if (method == "POST" && path == "/api/qr/complete") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.completeQR(f["qrId"], f["accountNo"], f["otp"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "GET" && path.rfind("/api/investments?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            Account* a = bank.getAccount(acc);
            if (!a) sendResponse(client, 404, "application/json", jsonMessage(false, "Account not found"));
            else {
                stringstream js;
                js << "{\"ok\":true,\"investments\":" << investmentsJson(*a)
                   << ",\"cost\":" << bank.investmentCost(acc)
                   << ",\"value\":" << bank.investmentValue(acc) << "}";
                sendResponse(client, 200, "application/json", js.str());
            }
        }
        else if (method == "POST" && path == "/api/investment/buy") {
            auto f = formDecode(body);
            double units = 0, price = 0;
            try { units = stod(f["units"]); price = stod(f["price"]); } catch (...) {}
            string msg;
            bool ok = bank.buyInvestment(f["accountNo"], f["symbol"], f["name"], units, price, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/investment/sell") {
            auto f = formDecode(body);
            double price = 0;
            try { price = stod(f["sellPrice"]); } catch (...) {}
            string msg;
            bool ok = bank.sellInvestment(f["accountNo"], f["investmentId"], price, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/profile/update") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.updateProfile(f["accountNo"], f["mobile"], f["email"], f["address"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "GET" && path.rfind("/api/smart/digital-twin?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            string result = bank.digitalTwin(acc);
            sendResponse(client, 200, "application/json", result);
        }
        else if (method == "GET" && path.rfind("/api/smart/explain?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.transactionExplanation(acc));
        }
        else if (method == "GET" && path.rfind("/api/smart/stress?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            double score = bank.financialStressScore(acc);
            stringstream js;
            js << "{\"ok\":true,\"score\":" << score << "}";
            sendResponse(client, 200, "application/json", js.str());
        }
        else if (method == "GET" && path.rfind("/api/smart/subscriptions?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.subscriptionAnalysis(acc));
        }
        else if (method == "GET" && path.rfind("/api/smart/fraud?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.fraudGraph(acc));
        }
        else if (method == "GET" && path.rfind("/api/smart/replay?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.replayMonth(acc));
        }
        else if (method == "GET" && path.rfind("/api/smart/inactivity?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.inactivityStatus(acc));
        }
        else if (method == "GET" && path.rfind("/api/smart/card-rule?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            sendResponse(client, 200, "application/json", bank.programmableCardRule(acc));
        }
        else if (method == "POST" && path == "/api/smart/what-if") {
            auto f = formDecode(body);
            double spend = 0, income = 0;
            try { spend = stod(f["extraSpend"]); } catch (...) {}
            try { income = stod(f["extraIncome"]); } catch (...) {}
            sendResponse(client, 200, "application/json",
                         bank.whatIf(f["accountNo"], spend, income));
        }
        else if (method == "POST" && path == "/api/smart/emergency") {
            auto f = formDecode(body);
            bool enabled = f["enabled"] == "1" || f["enabled"] == "true";
            string msg;
            bool ok = bank.setEmergencyMode(f["accountNo"], enabled, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/smart/beneficiary/add") {
            auto f = formDecode(body);
            int mins = 0;
            try { mins = stoi(f["coolingMinutes"]); } catch (...) {}
            string msg;
            bool ok = bank.addBeneficiary(f["accountNo"], f["beneficiary"], mins, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "GET" && path.rfind("/api/smart/beneficiary/status?", 0) == 0) {
            auto q = path.substr(path.find('?') + 1);
            auto f = formDecode(q);
            sendResponse(client, 200, "application/json",
                         bank.beneficiaryStatus(f["accountNo"], f["beneficiary"]));
        }
        else if (method == "POST" && path == "/api/smart/family/request") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}
            string msg;
            bool ok = bank.requestFamilyApproval(f["accountNo"], amount, f["purpose"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/smart/family/approve") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.approveFamilyRequest(f["accountNo"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/smart/card-rule") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.setProgrammableCardRule(f["accountNo"], f["rule"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "GET" && path.rfind("/api/cards?accountNo=", 0) == 0) {
            string acc = urlDecode(path.substr(path.find('=') + 1));
            Account* a = bank.getAccount(acc);
            if (!a) {
                sendResponse(client, 404, "application/json", jsonMessage(false, "Account not found"));
            } else {
                stringstream js;
                js << "{\"ok\":true,\"debitCard\":" << cardJson(a->debitCard, false)
                   << ",\"creditCard\":" << cardJson(a->creditCard, true) << "}";
                sendResponse(client, 200, "application/json", js.str());
            }
        }
        else if (method == "POST" && path == "/api/card/control") {
            auto f = formDecode(body);
            bool enabled = f["enabled"] == "1" || f["enabled"] == "true";
            string msg;
            bool ok = bank.setCardControl(
                f["accountNo"], f["cardType"], f["control"], enabled, msg
            );
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/card/limit") {
            auto f = formDecode(body);
            double limit = 0;
            try { limit = stod(f["limit"]); } catch (...) {}
            string msg;
            bool ok = bank.setCardLimit(
                f["accountNo"], f["cardType"], limit, msg
            );
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/card/block") {
            auto f = formDecode(body);
            bool blocked = f["blocked"] == "1" || f["blocked"] == "true";
            string msg;
            bool ok = bank.setCardBlocked(
                f["accountNo"], f["cardType"], blocked, msg
            );
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/card/bill-payment") {
            auto f = formDecode(body);
            double amount = 0;
            try { amount = stod(f["amount"]); } catch (...) {}
            string msg;
            bool ok = bank.payCreditCardBill(f["accountNo"], amount, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/card/debit") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.createDebitCard(f["accountNo"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "POST" && path == "/api/card/credit") {
            auto f = formDecode(body);
            string msg;
            bool ok = bank.createCreditCard(f["accountNo"], msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok, msg));
        }
        else if (method == "GET" && path == "/api/admin/stats") {
            stringstream js;
            js << "{\"ok\":true,\"accounts\":" << bank.accountCount()
               << ",\"balance\":" << bank.totalBalance()
               << ",\"pendingRequests\":" << bank.pendingRequests() << "}";
            sendResponse(client, 200, "application/json", js.str());
        }
        else if (method == "POST" && path == "/api/emi") {
            auto f = formDecode(body);
            double p=0,r=0; int months=0;
            try {
                p = stod(f["principal"]);
                r = stod(f["rate"]);
                months = stoi(f["months"]);
            } catch (...) {}

            double e = bank.emi(p,r,months);
            stringstream js;
            js << "{\"ok\":true,\"emi\":" << e << "}";
            sendResponse(client, 200, "application/json", js.str());
        }
        else if (method == "POST" && path == "/api/fd") {
            auto f = formDecode(body);
            double amount=0, rate=0; int months=0;
            try {
                amount = stod(f["amount"]);
                rate = stod(f["rate"]);
                months = stoi(f["months"]);
            } catch (...) {}

            string msg;
            bool ok = bank.createFD(f["accountNo"], amount, rate, months, msg);
            sendResponse(client, ok ? 200 : 400, "application/json", jsonMessage(ok,msg));
        }
        else {
            // ---------- Static frontend ----------
            string filePath = (path == "/") ? "/index.html" : path;
            string data = staticFile(filePath);
            if (data.empty()) {
                sendResponse(client, 404, "text/plain; charset=UTF-8", "Not Found");
            } else {
                sendResponse(client, 200, contentType(filePath), data);
            }
        }

        close(client);
    }

    close(serverFd);
    return 0;
}
