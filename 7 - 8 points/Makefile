CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -lrt

all: hotel_admin hotel_client

hotel_admin: hotel_admin.cpp shared_data.h
	$(CXX) hotel_admin.cpp -o hotel_admin $(CXXFLAGS)

hotel_client: hotel_client.cpp shared_data.h
	$(CXX) hotel_client.cpp -o hotel_client $(CXXFLAGS)

clean:
	rm -f hotel_admin hotel_client

.PHONY: all clean