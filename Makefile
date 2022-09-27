BLUEZ_SRCS := lib/bluetooth.c lib/hci.c lib/sdp.c lib/uuid.c
BLUEZ_SRCS += attrib/att.c attrib/gatt.c attrib/gattrib.c attrib/utils.c
BLUEZ_SRCS += btio/btio.c src/log.c src/shared/mgmt.c
BLUEZ_SRCS += src/shared/crypto.c src/shared/att.c src/shared/queue.c src/shared/util.c
BLUEZ_SRCS += src/shared/io-glib.c src/shared/timeout-glib.c
BLUEZ_SRCS := $(addprefix bluez-5.47/,$(BLUEZ_SRCS))
CFLAGS  := $(shell pkg-config --cflags bluez glibmm-2.4) -Ibluez-5.47 -D_GNU_SOURCE '-DVERSION="foo"' -O3
LDFLAGS := $(shell pkg-config --libs bluez glibmm-2.4) -lbluetooth -pthread
CPPFLAGS := $(CFLAGS) -std=c++20

BLUEZ_OBJS := $(BLUEZ_SRCS:.c=.o)

all: main

main.o: main.cpp
	g++ $(CPPFLAGS) -c $< -o $@

Encoder.o: Encoder.cpp Encoder.h
	g++ $(CPPFLAGS) -c $< -o $@

Timer.o: Timer.cpp Timer.h
	g++ $(CPPFLAGS) -c $< -o $@

main: main.o $(BLUEZ_OBJS) Encoder.o Timer.o
	g++ $^ -o $@ $(LDFLAGS)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

ss: simplescan.c
	gcc $< -o $@ -lbluetooth

test: main
	sudo ./$<

clean:
	rm -f *.o main $(shell find . -name '*.o')
