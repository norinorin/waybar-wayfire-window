CC = gcc
PKGCONFIG = pkg-config

GTK_CFLAGS = $(shell $(PKGCONFIG) --cflags gtk4)
GTK_LDFLAGS = $(shell $(PKGCONFIG) --libs gtk4)

CFLAGS = -I/usr/include/wlroots0.17 -I./include -I/usr/include/cjson $(GTK_CFLAGS)
LDFLAGS = -L/usr/lib/wlroots0.17 -lwlroots -lwayland-client -lcjson $(GTK_LDFLAGS)
TARGET = wayfire-window
SRC = wayfire-window.c wlr-foreign-toplevel-management-unstable-v1-protocol.c wayfire-ipc.c find-icon.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

debug: $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS) -g

clean:
	rm -f $(TARGET)
