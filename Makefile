CC = clang
CFLAGS = -g -Wall -Werror
CFLAGS += $(shell pkg-config --cflags json-c libcurl)
LDFLAGS += $(shell pkg-config --libs json-c libcurl)
TARGET = gpt3

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(TARGET).c
