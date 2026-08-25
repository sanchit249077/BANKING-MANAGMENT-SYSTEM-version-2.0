
CXX=g++
CXXFLAGS=-std=c++17 -O2 -Wall -Wextra

all: smartbank

smartbank:
	mkdir -p data
	$(CXX) $(CXXFLAGS) backend/server.cpp backend/BankSystem.cpp -o smartbank

run: smartbank
	./smartbank

clean:
	rm -f smartbank
