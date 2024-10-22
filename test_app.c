#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <string.h>

#define EVENT_DEVICE "/dev/input/event4"

void print_event(const struct input_event *ev) {
    const char *type;
    const char *code;

    switch (ev->type) {
        case EV_SYN: type = "Sync"; break;
        case EV_KEY: type = "Key"; break;
        case EV_REL: type = "Relative"; break;
        case EV_ABS: type = "Absolute"; break;
        default: type = "Unknown"; break;
    }

    switch (ev->code) {
        case BTN_LEFT: code = "Left Button"; break;
        case BTN_RIGHT: code = "Right Button"; break;
        case BTN_MIDDLE: code = "Middle Button"; break;
        case REL_X: code = "Relative X"; break;
        case REL_Y: code = "Relative Y"; break;
        default: code = "Unknown"; break;
    }

    printf("Event: time %ld.%06ld, type %s, code %s, value %d\n",
           ev->time.tv_sec, ev->time.tv_usec, type, code, ev->value);
}

int main() {
    int fd;
    struct input_event ev;

    // 打开输入设备
    fd = open(EVENT_DEVICE, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s: %s\n", EVENT_DEVICE, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Reading events from %s\n", EVENT_DEVICE);

    // 循环读取事件
    while (read(fd, &ev, sizeof(struct input_event)) == sizeof(struct input_event)) {
        print_event(&ev);
    }

    close(fd);
    return EXIT_SUCCESS;
}