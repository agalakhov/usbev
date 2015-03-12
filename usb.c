
#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


static void
usb_find_devices()
{
    struct libusb_context *ctx;
    int err = libusb_init(&ctx);
    if (err) {
        fprintf(stderr, "usb: %i: %s\n", err, libusb_error_name(err));
        abort();
    }

    struct libusb_device **devices;
    ssize_t ndevices = libusb_get_device_list(ctx, &devices);
    if (ndevices < 0) {
        fprintf(stderr, "usb: %i: %s\n", (int)ndevices, libusb_error_name(ndevices));
    }
    printf("%u devices found\n", (unsigned) ndevices);
    for (unsigned i = 0; i < ndevices; ++i) {
        struct libusb_device *dev = devices[i];
        struct libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev, &desc);
        if (err) {
            fprintf(stderr, "usb: %i: %s\n", err, libusb_error_name(err));
            abort();
        }
        printf("%04x:%04x\n", desc.idVendor, desc.idProduct);
    }
    libusb_free_device_list(devices, true);

    libusb_exit(ctx);
}


int main()
{
    usb_find_devices();
    return 0;
}
