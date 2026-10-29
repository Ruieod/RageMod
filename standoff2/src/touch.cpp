// Ввод для меню берём прямо с /dev/input: Unity на Android держит ввод в Java-слое
// (UnityPlayerActivity), экспортов для перехвата в libunity.so нет — нативных точек
// вроде AInputQueue_getEvent у неигрового NativeActivity тоже.
// Нужен root: /dev/input доступен только ему.
#include "touch.h"

#include <cstring>
#include <mutex>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/input.h>

#include "config.h"
#include "log.h"

namespace touch {
namespace {

struct Device {
    int fd = -1;
    bool is_touch = false;
    int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
};

std::vector<Device> g_devs;
std::mutex g_mu;
Event g_queue[128];
int g_head = 0, g_tail = 0;
int g_screen_w = 1920, g_screen_h = 1080;
bool g_found_touch = false;

void push(const Event& e) {
    std::lock_guard<std::mutex> lk(g_mu);
    int next = (g_head + 1) % 128;
    if (next == g_tail) return; // переполнение — роняем самое новое, меню переживёт
    g_queue[g_head] = e;
    g_head = next;
}

bool has_bit(const unsigned long* bits, int nr) {
    return (bits[nr / (8 * sizeof(long))] >> (nr % (8 * sizeof(long)))) & 1ul;
}

void probe(const char* path) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;

    unsigned long ev_bits[(EV_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    unsigned long abs_bits[(ABS_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    unsigned long key_bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits);

    bool want = false;
    Device d;
    d.fd = fd;

    if (has_bit(ev_bits, EV_ABS)) {
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);
        int ax = has_bit(abs_bits, ABS_MT_POSITION_X) ? ABS_MT_POSITION_X
                 : (has_bit(abs_bits, ABS_X) ? ABS_X : -1);
        int ay = has_bit(abs_bits, ABS_MT_POSITION_Y) ? ABS_MT_POSITION_Y
                 : (has_bit(abs_bits, ABS_Y) ? ABS_Y : -1);
        if (ax >= 0 && ay >= 0) {
            struct input_absinfo ix{}, iy{};
            if (ioctl(fd, EVIOCGABS(ax), &ix) == 0 && ioctl(fd, EVIOCGABS(ay), &iy) == 0 &&
                ix.maximum > ix.minimum && iy.maximum > iy.minimum) {
                d.is_touch = true;
                d.min_x = ix.minimum;
                d.max_x = ix.maximum;
                d.min_y = iy.minimum;
                d.max_y = iy.maximum;
                want = true;
                g_found_touch = true;
                LOGI("touch: %s x[%d..%d] y[%d..%d]", path, d.min_x, d.max_x, d.min_y, d.max_y);
            }
        }
    }
    if (!want && has_bit(ev_bits, EV_KEY)) {
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
        if (has_bit(key_bits, KEY_VOLUMEDOWN) || has_bit(key_bits, KEY_VOLUMEUP)) {
            want = true;
            LOGI("touch: %s keys", path);
        }
    }

    if (!want) {
        close(fd);
        return;
    }
    g_devs.push_back(d);
}

// сырые координаты тачскрина -> кадр игры (игра в landscape, панель обычно в portrait)
void map_point(const Device& d, int rx, int ry, float* out_x, float* out_y) {
    float nx = (float)(rx - d.min_x) / (float)(d.max_x - d.min_x);
    float ny = (float)(ry - d.min_y) / (float)(d.max_y - d.min_y);
    if (g_cfg.touch_swap_xy) {
        float t = nx;
        nx = ny;
        ny = t;
    }
    if (g_cfg.touch_invert_x) nx = 1.f - nx;
    if (g_cfg.touch_invert_y) ny = 1.f - ny;
    *out_x = nx * (float)g_screen_w;
    *out_y = ny * (float)g_screen_h;
}

void* reader(void*) {
    pthread_setname_np(pthread_self(), "rm-input");
    std::vector<struct pollfd> fds;
    for (const Device& d : g_devs) fds.push_back({d.fd, POLLIN, 0});
    if (fds.empty()) return nullptr;

    int cur_x = 0, cur_y = 0, slot = 0;
    bool down = false, dirty = false;

    while (true) {
        int r = poll(fds.data(), fds.size(), 1000);
        if (r < 0) break;
        if (r == 0) continue;
        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN)) continue;
            struct input_event ev[64];
            ssize_t n = read(fds[i].fd, ev, sizeof(ev));
            if (n < (ssize_t)sizeof(struct input_event)) continue;
            const Device& d = g_devs[i];
            for (size_t k = 0; k < (size_t)n / sizeof(struct input_event); k++) {
                const struct input_event& e = ev[k];
                if (e.type == EV_KEY) {
                    if (e.code == KEY_VOLUMEDOWN && e.value == 1) push({Event::MenuToggle, 0, 0});
                    if (e.code == BTN_TOUCH) {
                        down = e.value != 0;
                        dirty = true;
                    }
                    continue;
                }
                if (e.type == EV_ABS) {
                    switch (e.code) {
                        case ABS_MT_SLOT: slot = e.value; break;
                        case ABS_MT_TRACKING_ID:
                            if (slot == 0) {
                                down = e.value != -1;
                                dirty = true;
                            }
                            break;
                        case ABS_MT_POSITION_X:
                        case ABS_X:
                            if (slot == 0) {
                                cur_x = e.value;
                                dirty = true;
                            }
                            break;
                        case ABS_MT_POSITION_Y:
                        case ABS_Y:
                            if (slot == 0) {
                                cur_y = e.value;
                                dirty = true;
                            }
                            break;
                        default: break;
                    }
                    continue;
                }
                if (e.type == EV_SYN && e.code == SYN_REPORT && dirty && d.is_touch) {
                    float x = 0, y = 0;
                    map_point(d, cur_x, cur_y, &x, &y);
                    static bool was_down = false;
                    Event out{};
                    out.x = x;
                    out.y = y;
                    out.type = down ? (was_down ? Event::Move : Event::Down) : Event::Up;
                    was_down = down;
                    push(out);
                    dirty = false;
                    slot = 0;
                }
            }
        }
    }
    return nullptr;
}

} // namespace

void start() {
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        LOGE("touch: /dev/input недоступен (нет root?) — меню только через конфиг %s", config::path());
        return;
    }
    struct dirent* de;
    while ((de = readdir(dir))) {
        if (std::strncmp(de->d_name, "event", 5) != 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        probe(path);
    }
    closedir(dir);
    if (g_devs.empty()) {
        LOGE("touch: устройств ввода не нашли");
        return;
    }
    pthread_t th;
    pthread_create(&th, nullptr, reader, nullptr);
    pthread_detach(th);
}

void set_screen(int w, int h) {
    if (w > 0) g_screen_w = w;
    if (h > 0) g_screen_h = h;
}

bool poll(Event* out) {
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_tail == g_head) return false;
    *out = g_queue[g_tail];
    g_tail = (g_tail + 1) % 128;
    return true;
}

bool available() { return g_found_touch; }

} // namespace touch
