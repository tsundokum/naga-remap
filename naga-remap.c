/*
 * naga-remap - Razer Naga V2 HyperSpeed side button remapper
 *
 * Intercepts keycodes from the Naga's side buttons via evdev,
 * remaps them to key combos or shell commands via uinput.
 * Zero runtime dependencies, single static binary.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "cJSON.h"
#include "config.h"

#define RAZER_VENDOR   0x1532
#define RAZER_PRODUCT  0x00B4
#define PHYS_SUFFIX    "/input2"
#define RECONNECT_SEC  3

static volatile sig_atomic_t g_running = 1;
static int g_debug = 0;
static int g_evdev_fd = -1;
static int g_uinput_fd = -1;

/* ── Signal handling ───────────────────────────────────────────────── */

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_handler = sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Auto-reap children (fire-and-forget commands) */
    struct sigaction sc = {0};
    sc.sa_handler = SIG_DFL;
    sc.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sc, NULL);
}

/* ── Config parsing ────────────────────────────────────────────────── */

static int parse_config(const char *path, config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open config: %s: %s\n", path, strerror(errno));
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, len, f) != (size_t)len) {
        fprintf(stderr, "Short read on config file\n");
        free(buf);
        fclose(f);
        return -1;
    }
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "JSON parse error near: %s\n", cJSON_GetErrorPtr());
        return -1;
    }

    cJSON *mappings = cJSON_GetObjectItem(root, "mappings");
    if (!cJSON_IsArray(mappings)) {
        fprintf(stderr, "Config: 'mappings' must be an array\n");
        cJSON_Delete(root);
        return -1;
    }

    int n = cJSON_GetArraySize(mappings);
    if (n > MAX_MAPPINGS) {
        fprintf(stderr, "Config: too many mappings (%d), using first %d\n", n, MAX_MAPPINGS);
        n = MAX_MAPPINGS;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(mappings, i);
        key_mapping_t *m = &cfg->mappings[cfg->num_mappings];

        cJSON *btn = cJSON_GetObjectItem(item, "button");
        if (!cJSON_IsString(btn)) continue;

        int code = key_name_to_code(btn->valuestring);
        if (code < 0) {
            fprintf(stderr, "Config: unknown key '%s', skipping\n", btn->valuestring);
            continue;
        }
        m->button = code;

        cJSON *desc = cJSON_GetObjectItem(item, "description");
        if (cJSON_IsString(desc))
            snprintf(m->description, MAX_DESC_LEN, "%s", desc->valuestring);

        cJSON *cmd = cJSON_GetObjectItem(item, "command");
        cJSON *keys = cJSON_GetObjectItem(item, "keys");

        if (cJSON_IsString(cmd)) {
            snprintf(m->command, MAX_CMD_LEN, "%s", cmd->valuestring);
        } else if (cJSON_IsArray(keys)) {
            int nk = cJSON_GetArraySize(keys);
            if (nk > MAX_KEYS) {
                fprintf(stderr, "Config: too many keys in mapping '%s' (%d), using first %d\n",
                        m->description, nk, MAX_KEYS);
                nk = MAX_KEYS;
            }
            for (int k = 0; k < nk; k++) {
                cJSON *key = cJSON_GetArrayItem(keys, k);
                if (!cJSON_IsString(key)) continue;
                int kc = key_name_to_code(key->valuestring);
                if (kc < 0) {
                    fprintf(stderr, "Config: unknown key '%s' in mapping '%s'\n",
                            key->valuestring, m->description);
                    continue;
                }
                m->keys[m->num_keys++] = kc;
            }
        } else {
            fprintf(stderr, "Config: mapping '%s' has no 'keys' or 'command'\n",
                    m->description);
            continue;
        }

        cfg->num_mappings++;
    }

    cJSON_Delete(root);
    fprintf(stderr, "Loaded %d mappings from %s\n", cfg->num_mappings, path);
    return 0;
}

/* ── Device detection ──────────────────────────────────────────────── */

static int find_device(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        perror("opendir /dev/input");
        return -1;
    }

    int perm_errors = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        char path[280];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            if (errno == EACCES) perm_errors++;
            continue;
        }

        struct input_id id;
        if (ioctl(fd, EVIOCGID, &id) < 0) {
            close(fd);
            continue;
        }

        if (id.vendor != RAZER_VENDOR || id.product != RAZER_PRODUCT) {
            close(fd);
            continue;
        }

        char phys[256] = {0};
        ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys);

        size_t plen = strlen(phys);
        size_t slen = strlen(PHYS_SUFFIX);
        if (plen >= slen && strcmp(phys + plen - slen, PHYS_SUFFIX) == 0) {
            char name[256] = {0};
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            fprintf(stderr, "Found device: %s (%s) phys=%s\n", name, path, phys);
            closedir(dir);
            return fd;
        }

        close(fd);
    }

    closedir(dir);
    if (perm_errors > 0)
        fprintf(stderr, "Permission denied on %d device(s). "
                "Are you running as root?\n", perm_errors);
    return -1;
}

static void detect_devices(void) {
    DIR *dir = opendir("/dev/input");
    if (!dir) { perror("opendir /dev/input"); return; }

    printf("Scanning for Razer Naga V2 HyperSpeed (1532:00b4)...\n\n");

    struct dirent *ent;
    int found = 0, perm_errors = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        char path[280];
        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            if (errno == EACCES) perm_errors++;
            continue;
        }

        struct input_id id;
        if (ioctl(fd, EVIOCGID, &id) < 0) { close(fd); continue; }

        if (id.vendor == RAZER_VENDOR && id.product == RAZER_PRODUCT) {
            char name[256] = {0}, phys[256] = {0};
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys);

            size_t plen = strlen(phys);
            size_t slen = strlen(PHYS_SUFFIX);
            int is_side = (plen >= slen && strcmp(phys + plen - slen, PHYS_SUFFIX) == 0);

            printf("  %s\n    Name: %s\n    Phys: %s\n    Side buttons: %s\n\n",
                   path, name, phys, is_side ? "YES" : "no");
            found++;
        }
        close(fd);
    }

    closedir(dir);
    if (!found) {
        printf("No matching devices found.\n");
        if (perm_errors > 0)
            printf("\nPermission denied on %d device(s). "
                   "Try running with sudo.\n", perm_errors);
        else
            printf("Is the mouse connected?\n");
    }
}

/* ── uinput virtual device ─────────────────────────────────────────── */

static int setup_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY);
    if (fd < 0) {
        perror("open /dev/uinput");
        return -1;
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0 ||
        ioctl(fd, UI_SET_EVBIT, EV_REP) < 0) {
        perror("UI_SET_EVBIT");
        close(fd);
        return -1;
    }

    /* Register all keys from our lookup table so X11/libinput
       recognizes this as a proper keyboard device */
    for (int i = 0; key_table[i].name != NULL; i++) {
        ioctl(fd, UI_SET_KEYBIT, key_table[i].code);
    }

    struct uinput_setup setup = {0};
    snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "naga-remap virtual keyboard");
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor  = 0x1234;
    setup.id.product = 0x5678;
    setup.id.version = 1;

    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) {
        perror("UI_DEV_SETUP");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("UI_DEV_CREATE");
        close(fd);
        return -1;
    }

    /* Give udev time to create the device node */
    usleep(100000);

    fprintf(stderr, "Virtual input device created\n");
    return fd;
}

static void emit_event(int fd, int type, int code, int value) {
    struct input_event ev = {0};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) < 0)
        perror("write uinput");
}

static void emit_syn(int fd) {
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

/* ── Key combo emission ────────────────────────────────────────────── */

static void emit_key_down(int uinput_fd, const key_mapping_t *m) {
    /* Press each key with its own SYN, like a real keyboard */
    for (int i = 0; i < m->num_keys; i++) {
        emit_event(uinput_fd, EV_KEY, m->keys[i], 1);
        emit_syn(uinput_fd);
    }
}

static void emit_key_up(int uinput_fd, const key_mapping_t *m) {
    /* Release in reverse order, each with its own SYN */
    for (int i = m->num_keys - 1; i >= 0; i--) {
        emit_event(uinput_fd, EV_KEY, m->keys[i], 0);
        emit_syn(uinput_fd);
    }
}

static void emit_key_repeat(int uinput_fd, const key_mapping_t *m) {
    /* Repeat only the last key (the non-modifier) */
    if (m->num_keys > 0) {
        emit_event(uinput_fd, EV_KEY, m->keys[m->num_keys - 1], 2);
        emit_syn(uinput_fd);
    }
}

/* ── Command execution ─────────────────────────────────────────────── */

static void exec_command(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        /* Child: detach and redirect to /dev/null */
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    /* Parent: fire-and-forget (SA_NOCLDWAIT handles reaping) */
}

/* ── Main event loop ───────────────────────────────────────────────── */

static const key_mapping_t *find_mapping(const config_t *cfg, int keycode) {
    for (int i = 0; i < cfg->num_mappings; i++) {
        if (cfg->mappings[i].button == keycode)
            return &cfg->mappings[i];
    }
    return NULL;
}

static void run_loop(int evdev_fd, int uinput_fd, const config_t *cfg) {
    struct input_event ev;

    /* Grab device for exclusive access */
    if (ioctl(evdev_fd, EVIOCGRAB, 1) < 0) {
        perror("EVIOCGRAB");
        return;
    }

    fprintf(stderr, "Device grabbed, listening for events...\n");

    while (g_running) {
        ssize_t n = read(evdev_fd, &ev, sizeof(ev));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read evdev");
            break;  /* Device likely disconnected */
        }
        if (n != sizeof(ev)) continue;
        if (ev.type != EV_KEY) continue;

        if (g_debug) {
            fprintf(stderr, "[event] code=%d (%s) value=%d\n",
                    ev.code, key_code_to_name(ev.code), ev.value);
        }

        const key_mapping_t *m = find_mapping(cfg, ev.code);
        if (!m) {
            if (g_debug)
                fprintf(stderr, "  -> no mapping, dropping\n");
            continue;
        }

        if (m->command[0] != '\0') {
            /* Command mode: fire on key-down only */
            if (ev.value == 1) {
                if (g_debug)
                    fprintf(stderr, "  -> exec: %s\n", m->command);
                exec_command(m->command);
            }
        } else if (m->num_keys > 0) {
            /* Key combo mode */
            if (g_debug)
                fprintf(stderr, "  -> combo: %s (%d keys)\n", m->description, m->num_keys);
            switch (ev.value) {
                case 1: emit_key_down(uinput_fd, m);   break;
                case 0: emit_key_up(uinput_fd, m);     break;
                case 2: emit_key_repeat(uinput_fd, m); break;
            }
        }
    }

    ioctl(evdev_fd, EVIOCGRAB, 0);
}

/* ── Cleanup ───────────────────────────────────────────────────────── */

static void cleanup(void) {
    if (g_evdev_fd >= 0) {
        ioctl(g_evdev_fd, EVIOCGRAB, 0);
        close(g_evdev_fd);
        g_evdev_fd = -1;
    }
    if (g_uinput_fd >= 0) {
        ioctl(g_uinput_fd, UI_DEV_DESTROY);
        close(g_uinput_fd);
        g_uinput_fd = -1;
    }
}

/* ── Config path resolution ────────────────────────────────────────── */

#define DEFAULT_CONFIG_PATH "/etc/naga-remap/config.json"

/* ── Usage ─────────────────────────────────────────────────────────── */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -c <path>   Config file (default: " DEFAULT_CONFIG_PATH ")\n"
        "  -d          Debug mode (log all events to stderr)\n"
        "  --detect    Print matching devices and exit\n"
        "  -h          Show this help\n", prog);
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s", DEFAULT_CONFIG_PATH);

    /* Parse CLI args */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--detect") == 0) {
            detect_devices();
            return 0;
        } else if (strcmp(argv[i], "-d") == 0) {
            g_debug = 1;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            snprintf(config_path, sizeof(config_path), "%s", argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    setup_signals();

    /* Load config */
    config_t cfg;
    if (parse_config(config_path, &cfg) < 0)
        return 1;

    if (cfg.num_mappings == 0) {
        fprintf(stderr, "No valid mappings found, exiting\n");
        return 1;
    }

    /* Set up virtual input device */
    g_uinput_fd = setup_uinput();
    if (g_uinput_fd < 0)
        return 1;

    /* Main loop with reconnection */
    while (g_running) {
        g_evdev_fd = find_device();
        if (g_evdev_fd < 0) {
            fprintf(stderr, "Device not found, retrying in %ds...\n", RECONNECT_SEC);
            for (int i = 0; i < RECONNECT_SEC && g_running; i++)
                sleep(1);
            continue;
        }

        run_loop(g_evdev_fd, g_uinput_fd, &cfg);

        close(g_evdev_fd);
        g_evdev_fd = -1;

        if (g_running) {
            fprintf(stderr, "Device disconnected, retrying in %ds...\n", RECONNECT_SEC);
            for (int i = 0; i < RECONNECT_SEC && g_running; i++)
                sleep(1);
        }
    }

    fprintf(stderr, "Shutting down...\n");
    cleanup();
    return 0;
}
