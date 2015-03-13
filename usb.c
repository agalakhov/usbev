
#include "usb_id.h"
#include "usb_ids_knx.h"

#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef bool (*predicate_t) (void *closure, struct libusb_device *cand);

static bool
list_predicate(void *closure, struct libusb_device *cand)
{
    const struct usb_id *ids = closure;
    struct libusb_device_descriptor desc;
    int err = libusb_get_device_descriptor(cand, &desc);
    if (err) {
        fprintf(stderr, "usb: %i: %s\n", err, libusb_error_name(err));
        return false;
    }
    for (const struct usb_id *id = ids; id->vendor; ++id) {
        if (desc.idVendor == id->vendor && desc.idProduct == id->product) {
            printf("opening %04x:%04x\n", desc.idVendor, desc.idProduct);
            return true;
        }
    }
    return false;
}

static struct libusb_device_handle *
usb_find_and_open(struct libusb_context *ctx, predicate_t predicate, void *closure)
{
    struct libusb_device_handle *ret = NULL;
    struct libusb_device **devices;
    ssize_t ndevices = libusb_get_device_list(ctx, &devices);
    if (ndevices < 0) {
        fprintf(stderr, "usb: %i: %s\n", (int)ndevices, libusb_error_name(ndevices));
    }
    for (unsigned i = 0; i < ndevices; ++i) {
        if (predicate(closure, devices[i])) {
            int err = libusb_open(devices[i], &ret);
            if (err) {
                fprintf(stderr, "usb: can't open device: %i: %s\n", err, libusb_error_name(err));
                continue;
            }
            if (libusb_claim_interface(ret, 0)) {
                fprintf(stderr, "skipping busy device\n");
                continue;
            }
            libusb_release_interface(ret, 0);
            break;
        }
    }
    libusb_free_device_list(devices, true);
    return ret;
}


int main()
{
    struct libusb_context *ctx;
    int err = libusb_init(&ctx);
    if (err) {
        fprintf(stderr, "usb: %i: %s\n", err, libusb_error_name(err));
        abort();
    }

    struct libusb_device_handle *dev = usb_find_and_open(ctx, list_predicate, (void *)usb_ids_knx);
    if (! dev) {
        fprintf(stderr, "No USB devices found.\n");
        abort();
    }

    bool kernel = false;

    {
        int err = libusb_kernel_driver_active(dev, 0);
        if (err < 0) {
            fprintf(stderr, "usb: kernel %i: %s\n", err, libusb_error_name(err));
            abort();
        }
        kernel = (err == 1);
    }

    if (kernel) {
        int err = libusb_detach_kernel_driver(dev, 0);
        if (err < 0) {
            fprintf(stderr, "usb: kernel %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    {
        int err =  libusb_claim_interface(dev, 0);
        if (err) {
            fprintf(stderr, "usb: claim %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    while (true) {
        uint8_t buf[64];
        int size = 0;
        int err = libusb_interrupt_transfer(dev, 0x82, buf, sizeof(buf), &size, 0);
        int max = (size > 2) ? buf[2] + 3 : 1000;
        if (err) {
            fprintf(stderr, "usb: io %i: %s\n", err, libusb_error_name(err));
            abort();
        }
        for (int i = 0; i < size; ++i) {
            switch (i) {
                case 0: printf("\033[1;33m"); break;
                case 3: printf("\033[1;31m"); break;
                case 11: printf("\033[1;32m"); break;
                case 12: printf("\033[1;37m"); break;
                default: break;
            }
            if (i == max) {
                printf("\033[1;34m");
            }
            printf("%s%02x", i ? " " : "", buf[i]);
        }
        printf("\033[0m\n");
    }

    {
        int err =  libusb_release_interface(dev, 0);
        if (err) {
            fprintf(stderr, "usb: release %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    if (kernel) {
        int err = libusb_attach_kernel_driver(dev, 0);
        if (err < 0) {
            fprintf(stderr, "usb: kernel %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    libusb_close(dev);

    libusb_exit(ctx);
    return 0;
}
