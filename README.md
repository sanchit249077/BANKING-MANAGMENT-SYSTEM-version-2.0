
# Smart Banking Management System — GitHub Codespaces Edition

**Created by: SANCHIT NAYYAR**

This version is designed to run directly in **GitHub Codespaces**.

## Architecture

```text
SmartBank_Codespaces/
│
├── .devcontainer/
│   └── devcontainer.json
│
├── frontend/
│   ├── index.html
│   ├── style.css
│   └── app.js
│
├── backend/
│   ├── models.h
│   ├── BankSystem.h
│   ├── BankSystem.cpp
│   └── server.cpp
│
├── data/
│   ├── accounts.db
│   ├── transactions.db
│   ├── loans.db
│   └── fds.db
│
├── Makefile
├── run.sh
└── README.md
```

## Technologies

Frontend:
- HTML5
- CSS3
- Vanilla JavaScript

Backend:
- C++17
- Linux TCP socket HTTP server
- OOP
- File handling

DSA:
- `unordered_map` — fast account lookup
- `BST` — account-number indexing
- `vector` — transactions and records
- `queue` — service requests
- `stack` — recent transaction IDs
- searching/sorting concepts

## Features

- Account creation
- Customer login
- Dashboard
- Deposit
- Withdrawal
- Money transfer
- Transaction history and e-Passbook
- Downloadable/printable transaction statement
- Investment portfolio simulation with buy/sell
- Recent account number and account creation timestamp
- Account profile update
- Debit card generation
- Credit card generation
- Loan EMI calculator
- Fixed Deposit
- Admin statistics
- QR + OTP cardless cash withdrawal
- Investment dashboard with simulated market assets
- 5-minute QR expiry
- Persistent data files

## Run in GitHub Codespaces

### 1. Create the repository

Upload this project to a GitHub repository.

### 2. Open Codespaces

Open the repository and choose:

`Code → Codespaces → Create codespace on main`

The `.devcontainer/devcontainer.json` automatically forwards port **8080**.

### 3. Build

The devcontainer automatically compiles the backend. You can also run:

```bash
make
```

### 4. Start

```bash
./smartbank
```

or:

```bash
bash run.sh
```

### 5. Open the UI

Codespaces will detect port `8080`.

Open the forwarded **8080** port in the Ports panel.

You can also use:

```text
http://localhost:8080
```

when working in an environment that supports local forwarding.

## Important

The QR module is a **college-project simulation**, not a real UPI/payment gateway.

The system generates:
- QR ID
- OTP
- amount
- account association
- 5-minute validity

The backend verifies these before deducting the balance.

## Default Admin

```text
Username: admin
Password: admin123
```

## Suggested demo flow

1. Create account
2. Login
3. Deposit
4. Transfer to another account
5. Open transaction history
6. Generate QR cash withdrawal
7. Enter QR ID + OTP
8. Verify balance
9. Open Admin statistics

## Screenshots

The UI is designed as a professional dark banking dashboard with:
- Sidebar navigation
- Responsive cards
- Forms
- Tables
- QR transaction panel
- Admin statistics


## Card Centre — Modern Debit & Credit Access

The project now includes an expanded card-management module:

- Debit card generation
- Credit card generation
- Masked card display
- Instant block / unblock
- Online payment ON/OFF
- Contactless ON/OFF
- ATM withdrawal ON/OFF
- International usage ON/OFF
- Configurable daily card limits
- Credit-card limit and outstanding balance display
- Credit-card bill payment
- Card controls are persisted to the account data file

Sensitive card values are only generated for this academic simulation. A real
banking product must use secure tokenization, encryption, HSM-backed key
management and PCI-compliant payment infrastructure.

## Candidate Differentiating Features

No one can honestly guarantee that a feature has "never been added in any
other banking system." The following are intentionally uncommon / project-
differentiating ideas that can make this college project stand out:

1. Banking Digital Twin
   - Simulate the customer's future balance 7, 30 and 90 days ahead using
     income, recurring expenses and historical transactions.

2. What-If Finance Simulator
   - Let the user ask: "What happens if I spend ₹20,000 this month?"
   - Show projected balance, FD impact and investment impact.

3. Transaction Explainability Engine
   - Every transaction gets a human-readable explanation of why the balance
     changed and which account operation produced it.

4. Emergency Banking Mode
   - One button temporarily disables high-risk card channels and leaves only
     essential withdrawals / emergency contacts enabled.

5. Beneficiary Time-Lock
   - Newly added beneficiaries remain in a pending state for a chosen
     cooling-off period before large transfers are enabled.

6. Financial Stress Indicator
   - A rule-based score estimates whether upcoming bills could push the
     account below a safe balance threshold.

7. Subscription Kill Switch
   - Detect recurring merchant descriptions in the simulated transaction
     history and let the user disable selected recurring payment patterns.

8. Family Approval Wallet
   - Optional two-person approval for selected transfers, useful as a
     software-engineering demonstration of multi-user authorization.

9. Fraud Investigation Graph
   - Visualize account -> transaction -> merchant relationships using graph
     traversal (BFS/DFS) and flag unusual paths.

10. "Replay the Month" Simulator
    - Replay all transactions chronologically with a timeline slider so the
      student can demonstrate the transaction engine and data structures.

11. Programmable Card
    - Let the user define rules such as:
      "Allow international payments only from 6 PM–10 PM"
      or "ATM withdrawal limit ₹5,000 after 8 PM."

12. Dead-Man-Switch / Inactivity Workflow
    - A demonstrator feature where prolonged account inactivity can trigger a
      simulated notification workflow. This should remain clearly marked as
      a demo and not a real legal inheritance mechanism.

These can be added as separate DSA/OOP modules without replacing the existing
banking functionality.


## Smart Features Lab — 12 Advanced Modules

The latest build adds a dedicated **Smart Features Lab**:

1. Banking Digital Twin — 7/30/90-day balance projection
2. What-If Finance Simulator — simulate extra spending/income
3. Transaction Explainability Engine — human-readable balance explanations
4. Emergency Banking Mode — one-click demo restriction mode
5. Beneficiary Time-Lock — cooling-off period for new beneficiaries
6. Financial Stress Indicator — 0–100 financial stress score
7. Subscription Detector — finds subscription-like transaction patterns
8. Family Approval Wallet — queue-based second-person approval simulation
9. Fraud Investigation Graph — BFS traversal of transfer relationships
10. Replay the Month — chronological transaction replay
11. Programmable Card — store custom card-use rules
12. Inactivity / Dead-Man Workflow — demo inactivity monitoring

These features are deliberately presented as **academic simulations** where a
real banking integration would require secure production infrastructure.

## Card Centre

Debit and credit cards now support:
- generation
- masked-number display
- online payment control
- contactless control
- ATM control
- international control
- daily limits
- block / unblock
- credit limit and outstanding amount
- credit-card bill payment
