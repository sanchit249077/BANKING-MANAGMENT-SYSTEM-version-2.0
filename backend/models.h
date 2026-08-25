
#pragma once
#include <string>
#include <vector>
#include <ctime>

struct Transaction {
    std::string id;
    std::string type;
    std::string fromAccount;
    std::string toAccount;
    double amount = 0.0;
    double balanceAfter = 0.0;
    std::string time;
    std::string remark;
};

struct Card {
    std::string number;
    std::string pin;
    std::string cvv;
    bool blocked = false;

    // Modern card controls
    bool onlineEnabled = true;
    bool contactlessEnabled = true;
    bool atmEnabled = true;
    bool internationalEnabled = false;
    double dailyLimit = 25000.0;

    // Credit-card only
    double creditLimit = 100000.0;
    double outstanding = 0.0;
};

struct Loan {
    std::string id;
    std::string type;
    double amount = 0.0;
    double interestRate = 0.0;
    int tenureMonths = 0;
    double emi = 0.0;
    std::string status = "APPLIED";
};

struct Investment {
    std::string id;
    std::string symbol;
    std::string name;
    double units = 0.0;
    double buyPrice = 0.0;
    double currentPrice = 0.0;
    std::string purchasedAt;
    bool active = true;
};

struct FixedDeposit {
    std::string id;
    double principal = 0.0;
    double rate = 0.0;
    int tenureMonths = 0;
    std::string startDate;
    bool active = true;
};

struct QRTransaction {
    std::string qrId;
    std::string accountNo;
    double amount = 0.0;
    std::string otp;
    std::time_t createdAt = 0;
    bool used = false;
};

struct Account {
    std::string accountNo;
    std::string name;
    std::string mobile;
    std::string email;
    std::string address;
    std::string username;
    std::string password;
    std::string accountType = "Savings";
    std::string createdAt;
    double balance = 0.0;
    bool frozen = false;
    bool hasDebitCard = false;
    bool hasCreditCard = false;
    Card debitCard;
    Card creditCard;
    std::vector<Transaction> transactions;
    std::vector<Loan> loans;
    std::vector<FixedDeposit> fds;
    std::vector<Investment> investments;
};
