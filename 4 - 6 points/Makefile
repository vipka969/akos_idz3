CXX = g++
CXXFLAGS = -std=c++11 -lpthread -lrt
TARGET = hotel_admin
SOURCES = hotel_admin.cpp
SHM_NAME = hotel_shm_46

$(TARGET): $(SOURCES)
	$(CXX) $(SOURCES) -o $(TARGET) $(CXXFLAGS)

clean:
	rm -f $(TARGET)
	rm -f /dev/shm/$(SHM_NAME)

run: $(TARGET)
	./$(TARGET)

run-with-args: $(TARGET)
	./$(TARGET) 30

help:
	@echo "Доступные команды:"
	@echo "  make           - компиляция программы"
	@echo "  make run       - запуск с 25 клиентами (по умолчанию)"
	@echo "  make run-with-args - запуск с 30 клиентами"
	@echo "  make clean     - очистка"
	@echo "  make help      - эта справка"

.PHONY: clean run run-with-args help