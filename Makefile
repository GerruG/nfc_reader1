CC=gcc
CFLAGS=-Wall -Wextra
LIBS=-lpcsclite -lcurl -ljson-c

TARGET=nfc_reader
SRCS=nfc_reader.c
OBJS=$(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 