CC = gcc
AR = ar
CFLAGS = -Wall -fPIC -I. -I/usr/include/openssl -I/usr/include/libuv
LDFLAGS = -lssl -lcrypto -luv

SRCS = uvhttp.c llhttp.c http.c api.c
OBJS = $(SRCS:.c=.o)
TARGET = libuvhttp.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
