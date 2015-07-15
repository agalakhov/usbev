
#include "usb_id.h"
#include "usb_ids_knx.h"

#include <ev.h>
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
            break;
            if (libusb_claim_interface(ret, 0)) {
                fprintf(stderr, "skipping busy device\n");
                libusb_close(ret);
                continue;
            }
            libusb_release_interface(ret, 0);
            break;
        }
    }
    libusb_free_device_list(devices, true);
    return ret;
}

static void
ev_usb_register(struct ev_loop *loop, struct libusb_context *usbctx)
{
    (void) loop;
    const struct libusb_pollfd** fds = libusb_get_pollfds(usbctx);
    free(fds);
}

volatile int evcount = 5;

void
callback(struct libusb_transfer *transfer)
{
        const uint8_t *buf = transfer->buffer;
        int size = transfer->actual_length;
        int max = (size > 2) ? buf[2] + 3 : 1000;
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
        if (--evcount) {
            int err = libusb_submit_transfer(transfer);
            if (err) {
                fprintf(stderr, "usb: xfer %i: %s\n", err, libusb_error_name(err));
                abort();
            }
        }
}

int main()
{
    struct ev_loop *loop = ev_default_loop(0);
    if (! loop) {
        fprintf(stderr, "Unable to initialize event loop. Check $LIBEV_FLAGS.\n");
        abort();
    }

    struct libusb_context *ctx;
    int err = libusb_init(&ctx);
    if (err) {
        fprintf(stderr, "usb: %i: %s\n", err, libusb_error_name(err));
        abort();
    }

    ev_usb_register(loop, ctx);

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

    {
        int size = 0;
        uint8_t buf[] = {
            0x01, 0x13, 0x09,
            0x00, 0x08,         0x00, 0x01,
            0x0f,
            0x01,
            0x00, 0x00, 0x05
        };
        int err = libusb_interrupt_transfer(dev, 0x01, buf, sizeof(buf), &size, 0);
        if (err || size != sizeof(buf)) {
            fprintf(stderr, "usb: wio %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    struct libusb_transfer *xfer = libusb_alloc_transfer(0);
    if (! xfer) {
        fprintf(stderr, "Can't allocate USB transfer.\n");
        abort();
    }
    uint8_t buf[64];

    {
        libusb_fill_interrupt_transfer(xfer, dev, 0x82, buf, sizeof(buf), callback, NULL, 0);
        int err = libusb_submit_transfer(xfer);
        if (err) {
            fprintf(stderr, "usb: xfer %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    while (evcount) {
        int err = libusb_handle_events(ctx);
        if (err) {
            fprintf(stderr, "usb: events %i: %s\n", err, libusb_error_name(err));
            abort();
        }
    }

    libusb_free_transfer(xfer);

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
    ev_loop_destroy(loop);
    return 0;
}
