
#include "BankSystem.h"
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <algorithm>

using namespace std;

static string nowText() {
    time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);
    stringstream ss;
    ss << put_time(&local, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static string field(string s) {
    replace(s.begin(), s.end(), '|', '/');
    return s;
}


static string jsonEscapeLocal(const string& s) {
    string o;
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else o += c;
    }
    return o;
}

// ---------------- BST ----------------
AccountBST::Node* AccountBST::insert(Node* n, long long key, Account* a) {
    if (!n) return new Node(key, a);
    if (key < n->key) n->left = insert(n->left, key, a);
    else if (key > n->key) n->right = insert(n->right, key, a);
    return n;
}
Account* AccountBST::search(Node* n, long long key) const {
    if (!n) return nullptr;
    if (key == n->key) return n->account;
    return key < n->key ? search(n->left, key) : search(n->right, key);
}
void AccountBST::destroy(Node* n) {
    if (!n) return;
    destroy(n->left);
    destroy(n->right);
    delete n;
}
AccountBST::~AccountBST() { destroy(root); }
void AccountBST::insert(long long k, Account* a) { root = insert(root, k, a); }
Account* AccountBST::search(long long k) const { return search(root, k); }

// ---------------- Bank ----------------
BankSystem::BankSystem() {
    loadAccounts();
    loadTransactions();
    loadLoans();
    loadFDs();
    loadInvestments();
    for (auto& [id, a] : accounts) {
        try { accountTree.insert(stoll(id), &a); } catch (...) {}
    }
}

string BankSystem::randomDigits(int n) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<int> d(0, 9);
    string out;
    for (int i = 0; i < n; ++i) out += char('0' + d(gen));
    return out;
}
string BankSystem::id(const string& prefix) { return prefix + randomDigits(8); }
string BankSystem::accountNumber() {
    string a;
    do { a = "10" + randomDigits(8); } while (accounts.count(a));
    return a;
}

void BankSystem::addTransaction(Account& a, const string& type,
                                 const string& from, const string& to,
                                 double amount, const string& remark) {
    Transaction t;
    t.id = id("TXN");
    t.type = type;
    t.fromAccount = from;
    t.toAccount = to;
    t.amount = amount;
    t.balanceAfter = a.balance;
    t.time = nowText();
    t.remark = remark;
    a.transactions.push_back(t);
    recentTransactions.push(t.id);
}

Account* BankSystem::getAccount(const string& a) {
    auto it = accounts.find(a);
    return it == accounts.end() ? nullptr : &it->second;
}
const unordered_map<string, Account>& BankSystem::allAccounts() const { return accounts; }

string BankSystem::createAccount(const Account& input, string& error) {
    if (input.name.empty() || input.mobile.empty() ||
        input.username.empty() || input.password.size() < 4) {
        error = "Please fill the required fields. Password must be at least 4 characters.";
        return "";
    }

    for (const auto& [id, a] : accounts) {
        if (a.username == input.username) {
            error = "Username already exists.";
            return "";
        }
    }

    Account a = input;
    a.accountNo = accountNumber();
    a.createdAt = input.createdAt.empty() ? nowText() : input.createdAt;
    a.balance = max(0.0, input.balance);
    accounts[a.accountNo] = a;
    accountTree.insert(stoll(a.accountNo), &accounts[a.accountNo]);

    if (a.balance > 0)
        addTransaction(accounts[a.accountNo], "ACCOUNT OPENING",
                       "-", a.accountNo, a.balance, "Opening deposit");

    saveAll();
    return a.accountNo;
}

bool BankSystem::login(const string& acc, const string& user,
                       const string& pass, string& error) {
    Account* a = getAccount(acc);
    if (!a) { error = "Account not found."; return false; }
    if (a->frozen) { error = "Account is frozen."; return false; }
    if (a->username != user || a->password != pass) {
        error = "Invalid username or password.";
        return false;
    }
    return true;
}

bool BankSystem::deposit(const string& acc, double amount, string& msg) {
    Account* a = getAccount(acc);
    if (!a || a->frozen || amount <= 0) { msg = "Invalid account or amount."; return false; }
    a->balance += amount;
    addTransaction(*a, "DEPOSIT", "-", acc, amount, "Deposit");
    saveAll();
    msg = "Deposit successful.";
    return true;
}

bool BankSystem::withdraw(const string& acc, double amount, string& msg) {
    Account* a = getAccount(acc);
    if (!a || a->frozen || amount <= 0 || amount > a->balance) {
        msg = "Invalid amount or insufficient balance.";
        return false;
    }
    a->balance -= amount;
    addTransaction(*a, "WITHDRAWAL", acc, "-", amount, "Cash withdrawal");
    saveAll();
    msg = "Withdrawal successful.";
    return true;
}

bool BankSystem::transfer(const string& from, const string& to,
                          double amount, string& msg) {
    Account* s = getAccount(from);
    Account* r = getAccount(to);
    if (!s || !r || s->frozen || r->frozen || from == to ||
        amount <= 0 || amount > s->balance) {
        msg = "Transfer failed. Check account and balance.";
        return false;
    }

    s->balance -= amount;
    r->balance += amount;

    addTransaction(*s, "TRANSFER SENT", from, to, amount, "Fund transfer");
    addTransaction(*r, "TRANSFER RECEIVED", from, to, amount, "Funds received");

    serviceQueue.push({"SRV" + randomDigits(6), "Transfer " + from + " -> " + to});
    saveAll();
    msg = "Transfer successful.";
    return true;
}

bool BankSystem::createQR(const string& acc, double amount,
                          QRTransaction& result, string& msg) {
    Account* a = getAccount(acc);
    if (!a || a->frozen || amount <= 0 || amount > a->balance) {
        msg = "Invalid amount or insufficient balance.";
        return false;
    }

    QRTransaction q;
    q.qrId = id("QR");
    q.accountNo = acc;
    q.amount = amount;
    q.otp = randomDigits(4);
    q.createdAt = time(nullptr);
    qrTransactions[q.qrId] = q;

    serviceQueue.push({"SRV" + randomDigits(6), "QR withdrawal " + q.qrId});
    result = q;
    msg = "QR + OTP generated. Valid for 5 minutes.";
    return true;
}

bool BankSystem::completeQR(const string& qrId, const string& acc,
                             const string& otp, string& msg) {
    auto it = qrTransactions.find(qrId);
    if (it == qrTransactions.end()) { msg = "QR transaction not found."; return false; }

    QRTransaction& q = it->second;
    if (q.used || difftime(time(nullptr), q.createdAt) > 300) {
        msg = "QR transaction expired or already used.";
        return false;
    }
    if (q.accountNo != acc || q.otp != otp) {
        msg = "Invalid QR ID, account or OTP.";
        return false;
    }

    Account* a = getAccount(acc);
    if (!a || a->balance < q.amount) {
        msg = "Insufficient balance.";
        return false;
    }

    a->balance -= q.amount;
    addTransaction(*a, "QR CARDLESS WITHDRAWAL",
                   acc, "-", q.amount, "QR + OTP ATM");
    q.used = true;
    saveAll();

    msg = "Cash dispensed successfully.";
    return true;
}

bool BankSystem::createDebitCard(const string& acc, string& msg) {
    Account* a = getAccount(acc);
    if (!a) { msg = "Account not found."; return false; }
    if (a->hasDebitCard) { msg = "Debit card already exists."; return false; }

    static random_device rd;
    a->debitCard.number = "4321" + randomDigits(12);
    a->debitCard.pin = randomDigits(4);
    a->debitCard.cvv = randomDigits(3);
    a->debitCard.blocked = false;
    a->debitCard.onlineEnabled = true;
    a->debitCard.contactlessEnabled = true;
    a->debitCard.atmEnabled = true;
    a->debitCard.internationalEnabled = false;
    a->debitCard.dailyLimit = 25000.0;
    a->hasDebitCard = true;
    saveAll();

    msg = "Debit card generated.";
    return true;
}

bool BankSystem::createCreditCard(const string& acc, string& msg) {
    Account* a = getAccount(acc);
    if (!a) { msg = "Account not found."; return false; }
    if (a->hasCreditCard) { msg = "Credit card already exists."; return false; }

    a->creditCard.number = "5312" + randomDigits(12);
    a->creditCard.pin = randomDigits(4);
    a->creditCard.cvv = randomDigits(3);
    a->creditCard.blocked = false;
    a->creditCard.onlineEnabled = true;
    a->creditCard.contactlessEnabled = true;
    a->creditCard.atmEnabled = true;
    a->creditCard.internationalEnabled = false;
    a->creditCard.dailyLimit = 50000.0;
    a->creditCard.creditLimit = 100000.0;
    a->creditCard.outstanding = 0.0;
    a->hasCreditCard = true;
    saveAll();

    msg = "Credit card generated.";
    return true;
}


static Card* selectCard(Account* a, const string& cardType) {
    if (!a) return nullptr;
    if (cardType == "debit" && a->hasDebitCard) return &a->debitCard;
    if (cardType == "credit" && a->hasCreditCard) return &a->creditCard;
    return nullptr;
}

bool BankSystem::setCardControl(const string& accountNo,
                                const string& cardType,
                                const string& control,
                                bool enabled,
                                string& message) {
    Account* a = getAccount(accountNo);
    Card* c = selectCard(a, cardType);
    if (!c) { message = "Card not found."; return false; }

    if (control == "online") c->onlineEnabled = enabled;
    else if (control == "contactless") c->contactlessEnabled = enabled;
    else if (control == "atm") c->atmEnabled = enabled;
    else if (control == "international") c->internationalEnabled = enabled;
    else { message = "Unknown card control."; return false; }

    saveAll();
    message = control + string(enabled ? " enabled." : " disabled.");
    return true;
}

bool BankSystem::setCardLimit(const string& accountNo,
                              const string& cardType,
                              double limit,
                              string& message) {
    Account* a = getAccount(accountNo);
    Card* c = selectCard(a, cardType);
    if (!c || limit <= 0) { message = "Invalid card or limit."; return false; }

    c->dailyLimit = limit;
    saveAll();
    message = "Daily card limit updated.";
    return true;
}

bool BankSystem::setCardBlocked(const string& accountNo,
                                const string& cardType,
                                bool blocked,
                                string& message) {
    Account* a = getAccount(accountNo);
    Card* c = selectCard(a, cardType);
    if (!c) { message = "Card not found."; return false; }

    c->blocked = blocked;
    saveAll();
    message = blocked ? "Card blocked." : "Card unblocked.";
    return true;
}

bool BankSystem::payCreditCardBill(const string& accountNo,
                                   double amount,
                                   string& message) {
    Account* a = getAccount(accountNo);
    if (!a || !a->hasCreditCard || amount <= 0 ||
        amount > a->balance || amount > a->creditCard.outstanding) {
        message = "Invalid amount, balance, or outstanding credit-card bill.";
        return false;
    }

    a->balance -= amount;
    a->creditCard.outstanding -= amount;

    addTransaction(*a, "CREDIT CARD BILL PAYMENT",
                   accountNo, "CREDIT CARD", amount, "Credit-card bill payment");
    saveAll();

    message = "Credit-card bill payment successful.";
    return true;
}

double BankSystem::emi(double p, double annualRate, int months) const {
    if (p <= 0 || months <= 0) return 0;
    double r = annualRate / 1200.0;
    if (r == 0) return p / months;
    double f = pow(1 + r, months);
    return p * r * f / (f - 1);
}

bool BankSystem::createFD(const string& acc, double amount,
                          double rate, int months, string& msg) {
    Account* a = getAccount(acc);
    if (!a || a->frozen || amount <= 0 || amount > a->balance || months <= 0) {
        msg = "Invalid FD details or insufficient balance.";
        return false;
    }

    a->balance -= amount;

    FixedDeposit fd;
    fd.id = id("FD");
    fd.principal = amount;
    fd.rate = rate;
    fd.tenureMonths = months;
    fd.startDate = nowText();
    a->fds.push_back(fd);

    addTransaction(*a, "FD CREATION", acc, "BANK", amount, "Fixed deposit");
    saveAll();

    msg = "Fixed deposit created.";
    return true;
}


static double recentMonthlySpend(const Account& a) {
    time_t now = time(nullptr);
    double spend = 0.0;
    int transactions = 0;

    for (const auto& t : a.transactions) {
        try {
            tm tmv{};
            strptime(t.time.c_str(), "%Y-%m-%d %H:%M:%S", &tmv);
            time_t tt = mktime(&tmv);
            if (difftime(now, tt) <= 30.0 * 86400.0 &&
                (t.type == "WITHDRAWAL" ||
                 t.type == "TRANSFER SENT" ||
                 t.type == "INVESTMENT BUY" ||
                 t.type == "FD CREATION" ||
                 t.type == "CREDIT CARD BILL PAYMENT")) {
                spend += t.amount;
                transactions++;
            }
        } catch (...) {}
    }

    // Small demo fallback to keep the simulator useful for new accounts.
    if (transactions == 0) spend = a.balance * 0.05;
    return spend;
}

string BankSystem::digitalTwin(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    const Account& a = it->second;
    double monthlySpend = recentMonthlySpend(a);
    double monthlyIncome = 0.0;

    for (const auto& t : a.transactions) {
        if (t.type == "DEPOSIT" || t.type == "ACCOUNT OPENING")
            monthlyIncome += t.amount;
    }
    if (monthlyIncome <= 0) monthlyIncome = monthlySpend * 1.35;

    double netMonthly = monthlyIncome - monthlySpend;
    auto projected = [&](int months) {
        return a.balance + netMonthly * months;
    };

    stringstream ss;
    ss << "{\"ok\":true,"
       << "\"balance\":" << a.balance << ","
       << "\"monthlyIncome\":" << monthlyIncome << ","
       << "\"monthlySpend\":" << monthlySpend << ","
       << "\"projected7\":" << projected(0.25) << ","
       << "\"projected30\":" << projected(1) << ","
       << "\"projected90\":" << projected(3) << "}";
    return ss.str();
}

string BankSystem::whatIf(const string& accountNo, double extraSpend, double extraIncome) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    const Account& a = it->second;
    double base = a.balance;
    double after = base + extraIncome - extraSpend;
    double monthlySpend = recentMonthlySpend(a);
    double safeBuffer = max(1000.0, monthlySpend * 0.25);
    string risk = after < safeBuffer ? "HIGH" : (after < monthlySpend ? "MEDIUM" : "LOW");

    stringstream ss;
    ss << "{\"ok\":true,\"current\":" << base
       << ",\"extraSpend\":" << extraSpend
       << ",\"extraIncome\":" << extraIncome
       << ",\"after\":" << after
       << ",\"risk\":\"" << risk << "\"}";
    return ss.str();
}

string BankSystem::transactionExplanation(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    const Account& a = it->second;
    stringstream ss;
    ss << "{\"ok\":true,\"items\":[";
    bool first = true;
    for (auto rit = a.transactions.rbegin(); rit != a.transactions.rend(); ++rit) {
        const auto& t = *rit;
        if (!first) ss << ",";
        first = false;

        string explanation;
        if (t.type == "DEPOSIT") explanation = "Money was added to the account.";
        else if (t.type == "WITHDRAWAL") explanation = "Cash was removed from the account.";
        else if (t.type == "TRANSFER SENT") explanation = "Funds moved from this account to another account.";
        else if (t.type == "TRANSFER RECEIVED") explanation = "Funds were received from another account.";
        else if (t.type == "QR CARDLESS WITHDRAWAL") explanation = "Cardless ATM flow debited the account after QR + OTP verification.";
        else if (t.type == "INVESTMENT BUY") explanation = "Cash balance decreased because money was invested.";
        else if (t.type == "INVESTMENT SELL") explanation = "Investment proceeds were credited back to the bank account.";
        else if (t.type == "FD CREATION") explanation = "Money was moved from the savings balance into a fixed deposit.";
        else if (t.type == "CREDIT CARD BILL PAYMENT") explanation = "Cash balance decreased to pay the credit-card outstanding amount.";
        else explanation = "Balance changed because of a banking operation.";

        ss << "{\"id\":\"" << jsonEscapeLocal(t.id) << "\","
           << "\"type\":\"" << jsonEscapeLocal(t.type) << "\","
           << "\"amount\":" << t.amount << ","
           << "\"time\":\"" << jsonEscapeLocal(t.time) << "\","
           << "\"explanation\":\"" << jsonEscapeLocal(explanation) << "\"}";
    }
    ss << "]}";
    return ss.str();
}

bool BankSystem::setEmergencyMode(const string& accountNo, bool enabled, string& message) {
    if (!getAccount(accountNo)) { message = "Account not found."; return false; }
    emergencyMode[accountNo] = enabled;
    message = enabled
        ? "Emergency Banking Mode enabled. Treat this as a simulation control."
        : "Emergency Banking Mode disabled.";
    return true;
}

bool BankSystem::addBeneficiary(const string& accountNo, const string& beneficiary,
                                int coolingMinutes, string& message) {
    if (!getAccount(accountNo) || beneficiary.empty() || coolingMinutes < 0) {
        message = "Invalid beneficiary request.";
        return false;
    }
    beneficiaryUnlockAt[accountNo + ":" + beneficiary] =
        time(nullptr) + coolingMinutes * 60;
    message = "Beneficiary added with a cooling-off period.";
    return true;
}

string BankSystem::beneficiaryStatus(const string& accountNo, const string& beneficiary) const {
    string key = accountNo + ":" + beneficiary;
    auto it = beneficiaryUnlockAt.find(key);
    if (it == beneficiaryUnlockAt.end())
        return R"({"ok":false,"message":"Beneficiary not found"})";

    double remaining = difftime(it->second, time(nullptr));
    if (remaining <= 0)
        return R"({"ok":true,"status":"UNLOCKED","remainingSeconds":0})";

    stringstream ss;
    ss << "{\"ok\":true,\"status\":\"PENDING\",\"remainingSeconds\":"
       << static_cast<long long>(remaining) << "}";
    return ss.str();
}

double BankSystem::financialStressScore(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return -1;

    const Account& a = it->second;
    double spend = recentMonthlySpend(a);
    double ratio = spend <= 0 ? 0 : spend / max(1.0, a.balance + spend);
    double score = min(100.0, ratio * 100.0);

    if (a.balance < 1000) score = min(100.0, score + 35);
    else if (a.balance < spend * 0.5) score = min(100.0, score + 20);

    return score;
}

string BankSystem::subscriptionAnalysis(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    const Account& a = it->second;
    unordered_map<string, int> hits;
    for (const auto& t : a.transactions) {
        string r = t.remark;
        if (r.find("subscription") != string::npos ||
            r.find("SUBSCRIPTION") != string::npos ||
            r.find("stream") != string::npos ||
            r.find("STREAM") != string::npos ||
            r.find("membership") != string::npos ||
            r.find("MEMBERSHIP") != string::npos) {
            hits[r]++;
        }
    }

    stringstream ss;
    ss << "{\"ok\":true,\"patterns\":[";
    bool first = true;
    for (const auto& [pattern, count] : hits) {
        if (!first) ss << ",";
        first = false;
        ss << "{\"pattern\":\"" << jsonEscapeLocal(pattern) << "\",\"count\":" << count << "}";
    }
    ss << "],\"note\":\"Demo pattern detection based on transaction descriptions.\"}";
    return ss.str();
}

bool BankSystem::requestFamilyApproval(const string& accountNo, double amount,
                                        const string& purpose, string& message) {
    Account* a = getAccount(accountNo);
    if (!a || amount <= 0 || amount > a->balance) {
        message = "Invalid approval request.";
        return false;
    }

    string request = "FAM|" + accountNo + "|" + to_string(amount) + "|" + purpose;
    familyApprovalQueue.push(request);
    message = "Family approval requested and placed in the approval queue.";
    return true;
}

bool BankSystem::approveFamilyRequest(const string& accountNo, string& message) {
    if (familyApprovalQueue.empty()) {
        message = "No pending family approvals.";
        return false;
    }

    string request = familyApprovalQueue.front();
    stringstream ss(request);
    vector<string> parts;
    string x;
    while (getline(ss, x, '|')) parts.push_back(x);

    if (parts.size() < 4 || parts[1] != accountNo) {
        message = "No pending approval for this account at the front of the queue.";
        return false;
    }

    familyApprovalQueue.pop();
    message = "Family approval granted for ₹" + parts[2] + " — " + parts[3];
    return true;
}

string BankSystem::fraudGraph(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    unordered_map<string, vector<string>> graph;
    for (const auto& [id, a] : accounts) {
        for (const auto& t : a.transactions) {
            if (t.type == "TRANSFER SENT") {
                if (t.fromAccount != "-" && t.toAccount != "-") {
                    graph[t.fromAccount].push_back(t.toAccount);
                }
            }
        }
    }

    // BFS from the selected account.
    queue<string> q;
    unordered_map<string, bool> seen;
    vector<string> bfs;
    q.push(accountNo);
    seen[accountNo] = true;

    while (!q.empty()) {
        string cur = q.front(); q.pop();
        bfs.push_back(cur);
        for (const string& nxt : graph[cur]) {
            if (!seen[nxt]) {
                seen[nxt] = true;
                q.push(nxt);
            }
        }
    }

    stringstream ss;
    ss << "{\"ok\":true,\"start\":\"" << jsonEscapeLocal(accountNo)
       << "\",\"bfs\":[";
    for (size_t i = 0; i < bfs.size(); ++i) {
        if (i) ss << ",";
        ss << "\"" << jsonEscapeLocal(bfs[i]) << "\"";
    }
    ss << "],\"note\":\"BFS traversal of the simulated transfer network.\"}";
    return ss.str();
}

string BankSystem::replayMonth(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    const Account& a = it->second;
    stringstream ss;
    ss << "{\"ok\":true,\"events\":[";
    for (size_t i = 0; i < a.transactions.size(); ++i) {
        const auto& t = a.transactions[i];
        if (i) ss << ",";
        ss << "{\"step\":" << (i + 1)
           << ",\"time\":\"" << jsonEscapeLocal(t.time)
           << "\",\"type\":\"" << jsonEscapeLocal(t.type)
           << "\",\"amount\":" << t.amount
           << ",\"balanceAfter\":" << t.balanceAfter
           << ",\"remark\":\"" << jsonEscapeLocal(t.remark) << "\"}";
    }
    ss << "]}";
    return ss.str();
}

bool BankSystem::setProgrammableCardRule(const string& accountNo, const string& rule,
                                          string& message) {
    if (!getAccount(accountNo) || rule.empty()) {
        message = "Invalid programmable card rule.";
        return false;
    }
    programmableCardRules[accountNo] = rule;
    message = "Programmable card rule saved for this demo session.";
    return true;
}

string BankSystem::programmableCardRule(const string& accountNo) const {
    auto it = programmableCardRules.find(accountNo);
    if (it == programmableCardRules.end())
        return R"({"ok":true,"rule":""})";
    return "{\"ok\":true,\"rule\":\"" + jsonEscapeLocal(it->second) + "\"}";
}

string BankSystem::inactivityStatus(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return R"({"ok":false,"message":"Account not found"})";

    time_t last = 0;
    for (const auto& t : it->second.transactions) {
        tm tmv{};
        if (strptime(t.time.c_str(), "%Y-%m-%d %H:%M:%S", &tmv) == nullptr) continue;
        time_t tt = mktime(&tmv);
        last = max(last, tt);
    }
    if (last == 0) last = time(nullptr);

    long long days = static_cast<long long>(difftime(time(nullptr), last) / 86400.0);
    string status = days >= 30 ? "INACTIVE" : (days >= 7 ? "WATCH" : "ACTIVE");

    stringstream ss;
    ss << "{\"ok\":true,\"daysInactive\":" << days
       << ",\"status\":\"" << status
       << "\",\"note\":\"Demo inactivity workflow.\"}";
    return ss.str();
}


double BankSystem::totalBalance() const {
    double t = 0;
    for (const auto& [id, a] : accounts) t += a.balance;
    return t;
}
size_t BankSystem::accountCount() const { return accounts.size(); }
size_t BankSystem::pendingRequests() const { return serviceQueue.size(); }

void BankSystem::loadAccounts() {
    ifstream in(accountFile);
    string line;
    while (getline(in, line)) {
        stringstream ss(line);
        vector<string> p;
        string x;
        while (getline(ss, x, '|')) p.push_back(x);
        if (p.size() < 8) continue;

        try {
            Account a;
            a.accountNo = p[0];
            a.name = p[1];
            a.mobile = p[2];
            a.email = p[3];
            a.address = p[4];
            a.username = p[5];
            a.password = p[6];
            a.balance = stod(p[7]);
            a.createdAt = p.size() >= 9 ? p[8] : "";
            if (a.createdAt.empty()) a.createdAt = "Legacy account";

            // Optional modern card fields. Old account records still load.
            size_t i = 9;
            auto get = [&](size_t idx, const string& fallback) -> string {
                return idx < p.size() ? p[idx] : fallback;
            };
            auto getBool = [&](size_t idx, bool fallback) -> bool {
                if (idx >= p.size()) return fallback;
                return p[idx] == "1";
            };
            auto getDouble = [&](size_t idx, double fallback) -> double {
                if (idx >= p.size()) return fallback;
                try { return stod(p[idx]); } catch (...) { return fallback; }
            };

            a.hasDebitCard = getBool(i++, false);
            a.debitCard.number = get(i++, "");
            a.debitCard.pin = get(i++, "");
            a.debitCard.cvv = get(i++, "");
            a.debitCard.blocked = getBool(i++, false);
            a.debitCard.onlineEnabled = getBool(i++, true);
            a.debitCard.contactlessEnabled = getBool(i++, true);
            a.debitCard.atmEnabled = getBool(i++, true);
            a.debitCard.internationalEnabled = getBool(i++, false);
            a.debitCard.dailyLimit = getDouble(i++, 25000.0);

            a.hasCreditCard = getBool(i++, false);
            a.creditCard.number = get(i++, "");
            a.creditCard.pin = get(i++, "");
            a.creditCard.cvv = get(i++, "");
            a.creditCard.blocked = getBool(i++, false);
            a.creditCard.onlineEnabled = getBool(i++, true);
            a.creditCard.contactlessEnabled = getBool(i++, true);
            a.creditCard.atmEnabled = getBool(i++, true);
            a.creditCard.internationalEnabled = getBool(i++, false);
            a.creditCard.dailyLimit = getDouble(i++, 50000.0);
            a.creditCard.creditLimit = getDouble(i++, 100000.0);
            a.creditCard.outstanding = getDouble(i++, 0.0);

            accounts[a.accountNo] = a;
        } catch (...) {}
    }
}
void BankSystem::loadTransactions() {
    ifstream in(transactionFile);
    string line;
    while (getline(in, line)) {
        stringstream ss(line);
        vector<string> p;
        string x;
        while (getline(ss, x, '|')) p.push_back(x);
        if (p.size() != 9) continue;

        Account* a = getAccount(p[0]);
        if (!a) continue;

        try {
            Transaction t;
            t.id = p[1]; t.type = p[2]; t.time = p[3];
            t.fromAccount = p[4]; t.toAccount = p[5];
            t.amount = stod(p[6]); t.balanceAfter = stod(p[7]);
            t.remark = p[8];
            a->transactions.push_back(t);
        } catch (...) {}
    }
}

void BankSystem::loadLoans() {}
void BankSystem::loadFDs() {}

void BankSystem::loadInvestments() {
    ifstream in(investmentFile);
    string line;
    while (getline(in, line)) {
        stringstream ss(line);
        vector<string> p;
        string x;
        while (getline(ss, x, '|')) p.push_back(x);
        if (p.size() != 8) continue;
        Account* a = getAccount(p[0]);
        if (!a) continue;
        try {
            Investment inv;
            inv.id = p[1];
            inv.symbol = p[2];
            inv.name = p[3];
            inv.units = stod(p[4]);
            inv.buyPrice = stod(p[5]);
            inv.currentPrice = stod(p[6]);
            inv.purchasedAt = p[7];
            a->investments.push_back(inv);
        } catch (...) {}
    }
}

void BankSystem::saveAccounts() {
    ofstream out(accountFile);
    for (const auto& [id, a] : accounts) {
        out << field(a.accountNo) << '|'
            << field(a.name) << '|'
            << field(a.mobile) << '|'
            << field(a.email) << '|'
            << field(a.address) << '|'
            << field(a.username) << '|'
            << field(a.password) << '|'
            << fixed << setprecision(2) << a.balance << '|'
            << field(a.createdAt) << '|';

        // Debit card
        out << (a.hasDebitCard ? 1 : 0) << '|'
            << field(a.debitCard.number) << '|'
            << field(a.debitCard.pin) << '|'
            << field(a.debitCard.cvv) << '|'
            << (a.debitCard.blocked ? 1 : 0) << '|'
            << (a.debitCard.onlineEnabled ? 1 : 0) << '|'
            << (a.debitCard.contactlessEnabled ? 1 : 0) << '|'
            << (a.debitCard.atmEnabled ? 1 : 0) << '|'
            << (a.debitCard.internationalEnabled ? 1 : 0) << '|'
            << a.debitCard.dailyLimit << '|';

        // Credit card
        out << (a.hasCreditCard ? 1 : 0) << '|'
            << field(a.creditCard.number) << '|'
            << field(a.creditCard.pin) << '|'
            << field(a.creditCard.cvv) << '|'
            << (a.creditCard.blocked ? 1 : 0) << '|'
            << (a.creditCard.onlineEnabled ? 1 : 0) << '|'
            << (a.creditCard.contactlessEnabled ? 1 : 0) << '|'
            << (a.creditCard.atmEnabled ? 1 : 0) << '|'
            << (a.creditCard.internationalEnabled ? 1 : 0) << '|'
            << a.creditCard.dailyLimit << '|'
            << a.creditCard.creditLimit << '|'
            << a.creditCard.outstanding << '\n';
    }
}
void BankSystem::saveTransactions() {
    ofstream out(transactionFile);
    for (const auto& [id, a] : accounts) {
        for (const auto& t : a.transactions) {
            out << a.accountNo << '|'
                << field(t.id) << '|'
                << field(t.type) << '|'
                << field(t.time) << '|'
                << field(t.fromAccount) << '|'
                << field(t.toAccount) << '|'
                << t.amount << '|'
                << t.balanceAfter << '|'
                << field(t.remark) << '\n';
        }
    }
}

void BankSystem::saveLoans() {}
void BankSystem::saveFDs() {}
void BankSystem::saveInvestments() {
    ofstream out(investmentFile);
    for (const auto& [id, a] : accounts) {
        for (const auto& inv : a.investments) {
            if (!inv.active) continue;
            out << a.accountNo << '|' << field(inv.id) << '|'
                << field(inv.symbol) << '|' << field(inv.name) << '|'
                << inv.units << '|' << inv.buyPrice << '|'
                << inv.currentPrice << '|' << field(inv.purchasedAt) << '\n';
        }
    }
}

std::vector<Investment> BankSystem::getInvestments(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return {};
    return it->second.investments;
}

bool BankSystem::buyInvestment(const string& accountNo, const string& symbol,
                               const string& name, double units, double price, string& message) {
    Account* a = getAccount(accountNo);
    if (!a || a->frozen || units <= 0 || price <= 0) {
        message = "Invalid investment details.";
        return false;
    }
    double cost = units * price;
    if (cost > a->balance) {
        message = "Insufficient balance for investment.";
        return false;
    }
    a->balance -= cost;
    Investment inv;
    inv.id = id("INV");
    inv.symbol = symbol;
    inv.name = name;
    inv.units = units;
    inv.buyPrice = price;
    inv.currentPrice = price;
    inv.purchasedAt = nowText();
    a->investments.push_back(inv);
    addTransaction(*a, "INVESTMENT BUY", accountNo, symbol, cost, "Simulated investment purchase");
    saveAll();
    message = "Investment purchased successfully.";
    return true;
}

bool BankSystem::sellInvestment(const string& accountNo, const string& investmentId,
                                double sellPrice, string& message) {
    Account* a = getAccount(accountNo);
    if (!a || a->frozen || sellPrice <= 0) {
        message = "Invalid investment sale.";
        return false;
    }
    for (auto& inv : a->investments) {
        if (inv.id == investmentId && inv.active) {
            double proceeds = inv.units * sellPrice;
            inv.currentPrice = sellPrice;
            inv.active = false;
            a->balance += proceeds;
            addTransaction(*a, "INVESTMENT SELL", inv.symbol, accountNo, proceeds, "Simulated investment sale");
            saveAll();
            message = "Investment sold successfully.";
            return true;
        }
    }
    message = "Investment not found.";
    return false;
}

bool BankSystem::updateProfile(const string& accountNo, const string& mobile,
                               const string& email, const string& address, string& message) {
    Account* a = getAccount(accountNo);
    if (!a) { message = "Account not found."; return false; }
    if (!mobile.empty()) a->mobile = mobile;
    if (!email.empty()) a->email = email;
    if (!address.empty()) a->address = address;
    saveAll();
    message = "Account details updated.";
    return true;
}

double BankSystem::investmentValue(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return 0;
    double total = 0;
    for (const auto& inv : it->second.investments)
        if (inv.active) total += inv.units * inv.currentPrice;
    return total;
}

double BankSystem::investmentCost(const string& accountNo) const {
    auto it = accounts.find(accountNo);
    if (it == accounts.end()) return 0;
    double total = 0;
    for (const auto& inv : it->second.investments)
        if (inv.active) total += inv.units * inv.buyPrice;
    return total;
}

void BankSystem::saveAll() {
    saveAccounts();
    saveTransactions();
    saveLoans();
    saveFDs();
    saveInvestments();
}
