CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pthread -lrt
TARGET_ADMIN = hotel_admin
TARGET_CLIENT = hotel_client
TARGET_OBSERVER = observer

all: $(TARGET_ADMIN) $(TARGET_CLIENT) $(TARGET_OBSERVER)

$(TARGET_ADMIN): hotel_admin.cpp shared_data.h
	$(CXX) $(CXXFLAGS) -o $(TARGET_ADMIN) hotel_admin.cpp

$(TARGET_CLIENT): hotel_client.cpp shared_data.h
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLIENT) hotel_client.cpp

$(TARGET_OBSERVER): observer.cpp shared_data.h
	$(CXX) $(CXXFLAGS) -o $(TARGET_OBSERVER) observer.cpp

clean:
	rm -f $(TARGET_ADMIN) $(TARGET_CLIENT) $(TARGET_OBSERVER)
	-sudo rm -f /dev/shm/*hotel* 2>/dev/null
	-sudo rm -f /dev/mqueue/*hotel* 2>/dev/null

.PHONY: all clean