CC=gcc
CFLAGS=-Wall -Wextra
LIBS=-lpcsc -lcurl -ljson-c
SRCS=main.c card_operations.c api_handler.c
OBJS=$(SRCS:.c=.o)
TARGET=nfc_reader

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean 