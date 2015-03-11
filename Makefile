CFLAGS_libusb = $(shell pkg-config --cflags libusb-1.0)
LIBS_libusb = $(shell pkg-config --libs libusb-1.0)

main.exe : usb.c
	gcc -O2 --std=c11 -o $@ $< -lusb $(CFLAGS_libusb) $(LIBS_libusb)
