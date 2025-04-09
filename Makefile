CC = gcc
PKGCONFIG = pkg-config

GTK_CFLAGS = $(shell $(PKGCONFIG) --cflags gtk4)
GTK_LDFLAGS = $(shell $(PKGCONFIG) --libs gtk4)

CFLAGS = -I/usr/include/wlroots-0.18 -I/usr/include/cjson $(GTK_CFLAGS) -Wall -Wextra -Werror -Wno-unused-parameter -Wshadow -Wconversion -Wsign-conversion
LDFLAGS = -L/usr/lib/wlroots-0.18 -lwlroots-0.18 -lwayland-client -lcjson $(GTK_LDFLAGS)
TARGET = wayfire-window
SRC = wayfire-window.c wlr-foreign-toplevel-management-unstable-v1-protocol.c wayfire-ipc.c find-icon.c
ASSETS_DIR = ~/.local/share/waybar-wayfire-window/assets

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)
	mkdir -p $(ASSETS_DIR)
	cp -r assets/* $(ASSETS_DIR)

debug: $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS) -g

find-icon: find-icon.c find-icon-cli.c
	$(CC) -o find-icon-cli find-icon.c find-icon-cli.c ${GTK_CFLAGS} ${GTK_LDFLAGS}

clean:
	rm -f $(TARGET)
