#!/usr/bin/env bash
set -e
mkdir -p data
g++ -std=c++17 -O2 -Wall -Wextra backend/server.cpp backend/BankSystem.cpp -o smartbank
./smartbank
