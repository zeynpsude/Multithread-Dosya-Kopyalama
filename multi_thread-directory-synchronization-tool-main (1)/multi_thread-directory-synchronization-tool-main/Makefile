CC     = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = copy_tool
SRCS   = src/main.c src/queue.c src/scanner.c src/worker.c src/log.c

all:
	$(CC) $(CFLAGS) -Iinclude $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) copy_tool.log
