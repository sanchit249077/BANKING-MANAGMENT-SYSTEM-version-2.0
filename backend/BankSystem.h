
#pragma once

#include "models.h"
#include <unordered_map>
#include <queue>
#include <stack>
#include <string>

class AccountBST {
    struct Node {
        long long key;
        Account* account;
        Node* left;
        Node* right;
        Node(long long k, Account* a) : key(k), account(a), left(nullptr), right(nullptr) {}
    };
    Node* root = nullptr;

    Node* insert(Node*, long long, Account*);
    Account* search(Node*, long long) const;
    void destroy(Node*);

public:
    ~AccountBST();
    void insert(long long, Account*);
    Account* search(long long) const;
};

struct ServiceRequest {
    std::string id;
    std::string description;
};

class BankSystem {
    std::unordered_map<std::string, Account> accounts;
    std::unordered_map<std::string, QRTransaction> qrTransactions;
    std::queue<ServiceRequest> serviceQueue;
    std::stack<std::string> recentTransactions;
    AccountBST accountTree;

    // Advanced banking simulations
    std::unordered_map<std::string, bool> emergencyMode;
    std::unordered_map<std::string, std::time_t> beneficiaryUnlockAt;
    std::unordered_map<std::string, std::string> programmableCardRules;
    std::unordered_map<std::string, std::time_t> lastActivity;
    std::queue<std::string> familyApprovalQueue;

    std::string accountFile = "data/accounts.db";
    std::string transactionFile = "data/transactions.db";
    std::string loanFile = "data/loans.db";
    std::string fdFile = "data/fds.db";
    std::string investmentFile = "data/investments.db";

    std::string randomDigits(int n);
    std::string id(const std::string& prefix);
    std::string accountNumber();
    void addTransaction(Account&, const std::string&, const std::string&,
                        const std::string&, double, const std::string&);

    void loadAccounts();
    void loadTransactions();
    void loadLoans();
    void loadFDs();
    void loadInvestments();

    void saveAccounts();
    void saveTransactions();
    void saveLoans();
    void saveFDs();
    void saveInvestments();

public:
    BankSystem();

    Account* getAccount(const std::string& accountNo);
    const std::unordered_map<std::string, Account>& allAccounts() const;

    std::string createAccount(const Account& input, std::string& error);
    bool login(const std::string&, const std::string&, const std::string&, std::string& error);

    bool deposit(const std::string&, double, std::string&);
    bool withdraw(const std::string&, double, std::string&);
    bool transfer(const std::string&, const std::string&, double, std::string&);

    bool createQR(const std::string&, double, QRTransaction&, std::string&);
    bool completeQR(const std::string&, const std::string&, const std::string&, std::string&);

    bool createDebitCard(const std::string&, std::string&);
    bool createCreditCard(const std::string&, std::string&);

    // Modern card access/control
    bool setCardControl(const std::string& accountNo,
                        const std::string& cardType,
                        const std::string& control,
                        bool enabled,
                        std::string& message);
    bool setCardLimit(const std::string& accountNo,
                      const std::string& cardType,
                      double limit,
                      std::string& message);
    bool setCardBlocked(const std::string& accountNo,
                        const std::string& cardType,
                        bool blocked,
                        std::string& message);
    bool payCreditCardBill(const std::string& accountNo,
                           double amount,
                           std::string& message);

    double emi(double, double, int) const;
    bool createFD(const std::string&, double, double, int, std::string&);

    // Advanced / uncommon project features
    std::string digitalTwin(const std::string& accountNo) const;
    std::string whatIf(const std::string& accountNo, double extraSpend, double extraIncome) const;
    std::string transactionExplanation(const std::string& accountNo) const;
    bool setEmergencyMode(const std::string& accountNo, bool enabled, std::string& message);
    bool addBeneficiary(const std::string& accountNo, const std::string& beneficiary,
                        int coolingMinutes, std::string& message);
    std::string beneficiaryStatus(const std::string& accountNo, const std::string& beneficiary) const;
    double financialStressScore(const std::string& accountNo) const;
    std::string subscriptionAnalysis(const std::string& accountNo) const;
    bool requestFamilyApproval(const std::string& accountNo, double amount,
                               const std::string& purpose, std::string& message);
    bool approveFamilyRequest(const std::string& accountNo, std::string& message);
    std::string fraudGraph(const std::string& accountNo) const;
    std::string replayMonth(const std::string& accountNo) const;
    bool setProgrammableCardRule(const std::string& accountNo, const std::string& rule,
                                 std::string& message);
    std::string programmableCardRule(const std::string& accountNo) const;
    std::string inactivityStatus(const std::string& accountNo) const;

    std::vector<Investment> getInvestments(const std::string& accountNo) const;
    bool buyInvestment(const std::string& accountNo, const std::string& symbol,
                      const std::string& name, double units, double price, std::string& message);
    bool sellInvestment(const std::string& accountNo, const std::string& investmentId,
                       double sellPrice, std::string& message);
    bool updateProfile(const std::string& accountNo, const std::string& mobile,
                       const std::string& email, const std::string& address, std::string& message);

    double totalBalance() const;
    size_t accountCount() const;
    size_t pendingRequests() const;
    double investmentValue(const std::string& accountNo) const;
    double investmentCost(const std::string& accountNo) const;

    void saveAll();
};
