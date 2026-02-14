/*
 * config.h - Configuration structures and parser for naga-remap
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <linux/input-event-codes.h>

#define MAX_KEYS        8
#define MAX_MAPPINGS    24
#define MAX_CMD_LEN     512
#define MAX_DESC_LEN    64

typedef struct {
    int button;                     /* source keycode (e.g. KEY_KP1) */
    char description[MAX_DESC_LEN];
    /* key combo mode */
    int keys[MAX_KEYS];             /* keycodes to emit */
    int num_keys;
    /* command mode */
    char command[MAX_CMD_LEN];      /* shell command (empty = key mode) */
} key_mapping_t;

typedef struct {
    key_mapping_t mappings[MAX_MAPPINGS];
    int num_mappings;
} config_t;

/* Key name -> keycode lookup table */
typedef struct {
    const char *name;
    int code;
} key_entry_t;

static const key_entry_t key_table[] = {
    /* Numpad keys (Razer Naga side buttons emit these) */
    {"KEY_KP0",        KEY_KP0},
    {"KEY_KP1",        KEY_KP1},
    {"KEY_KP2",        KEY_KP2},
    {"KEY_KP3",        KEY_KP3},
    {"KEY_KP4",        KEY_KP4},
    {"KEY_KP5",        KEY_KP5},
    {"KEY_KP6",        KEY_KP6},
    {"KEY_KP7",        KEY_KP7},
    {"KEY_KP8",        KEY_KP8},
    {"KEY_KP9",        KEY_KP9},
    {"KEY_KPMINUS",    KEY_KPMINUS},
    {"KEY_KPPLUS",     KEY_KPPLUS},
    {"KEY_KPDOT",      KEY_KPDOT},
    {"KEY_KPENTER",    KEY_KPENTER},
    {"KEY_KPSLASH",    KEY_KPSLASH},
    {"KEY_KPASTERISK", KEY_KPASTERISK},
    {"KEY_NUMLOCK",    KEY_NUMLOCK},

    /* Modifier keys */
    {"KEY_LEFTCTRL",   KEY_LEFTCTRL},
    {"KEY_RIGHTCTRL",  KEY_RIGHTCTRL},
    {"KEY_LEFTSHIFT",  KEY_LEFTSHIFT},
    {"KEY_RIGHTSHIFT", KEY_RIGHTSHIFT},
    {"KEY_LEFTALT",    KEY_LEFTALT},
    {"KEY_RIGHTALT",   KEY_RIGHTALT},
    {"KEY_LEFTMETA",   KEY_LEFTMETA},
    {"KEY_RIGHTMETA",  KEY_RIGHTMETA},

    /* Letters */
    {"KEY_A", KEY_A}, {"KEY_B", KEY_B}, {"KEY_C", KEY_C}, {"KEY_D", KEY_D},
    {"KEY_E", KEY_E}, {"KEY_F", KEY_F}, {"KEY_G", KEY_G}, {"KEY_H", KEY_H},
    {"KEY_I", KEY_I}, {"KEY_J", KEY_J}, {"KEY_K", KEY_K}, {"KEY_L", KEY_L},
    {"KEY_M", KEY_M}, {"KEY_N", KEY_N}, {"KEY_O", KEY_O}, {"KEY_P", KEY_P},
    {"KEY_Q", KEY_Q}, {"KEY_R", KEY_R}, {"KEY_S", KEY_S}, {"KEY_T", KEY_T},
    {"KEY_U", KEY_U}, {"KEY_V", KEY_V}, {"KEY_W", KEY_W}, {"KEY_X", KEY_X},
    {"KEY_Y", KEY_Y}, {"KEY_Z", KEY_Z},

    /* Numbers */
    {"KEY_0", KEY_0}, {"KEY_1", KEY_1}, {"KEY_2", KEY_2}, {"KEY_3", KEY_3},
    {"KEY_4", KEY_4}, {"KEY_5", KEY_5}, {"KEY_6", KEY_6}, {"KEY_7", KEY_7},
    {"KEY_8", KEY_8}, {"KEY_9", KEY_9},

    /* Function keys */
    {"KEY_F1",  KEY_F1},  {"KEY_F2",  KEY_F2},  {"KEY_F3",  KEY_F3},
    {"KEY_F4",  KEY_F4},  {"KEY_F5",  KEY_F5},  {"KEY_F6",  KEY_F6},
    {"KEY_F7",  KEY_F7},  {"KEY_F8",  KEY_F8},  {"KEY_F9",  KEY_F9},
    {"KEY_F10", KEY_F10}, {"KEY_F11", KEY_F11}, {"KEY_F12", KEY_F12},

    /* Navigation */
    {"KEY_ESC",       KEY_ESC},
    {"KEY_TAB",       KEY_TAB},
    {"KEY_ENTER",     KEY_ENTER},
    {"KEY_SPACE",     KEY_SPACE},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
    {"KEY_DELETE",    KEY_DELETE},
    {"KEY_INSERT",    KEY_INSERT},
    {"KEY_HOME",      KEY_HOME},
    {"KEY_END",       KEY_END},
    {"KEY_PAGEUP",    KEY_PAGEUP},
    {"KEY_PAGEDOWN",  KEY_PAGEDOWN},
    {"KEY_UP",        KEY_UP},
    {"KEY_DOWN",      KEY_DOWN},
    {"KEY_LEFT",      KEY_LEFT},
    {"KEY_RIGHT",     KEY_RIGHT},

    /* Punctuation / symbols */
    {"KEY_MINUS",        KEY_MINUS},
    {"KEY_EQUAL",        KEY_EQUAL},
    {"KEY_LEFTBRACE",    KEY_LEFTBRACE},
    {"KEY_RIGHTBRACE",   KEY_RIGHTBRACE},
    {"KEY_BACKSLASH",    KEY_BACKSLASH},
    {"KEY_SEMICOLON",    KEY_SEMICOLON},
    {"KEY_APOSTROPHE",   KEY_APOSTROPHE},
    {"KEY_GRAVE",        KEY_GRAVE},
    {"KEY_COMMA",        KEY_COMMA},
    {"KEY_DOT",          KEY_DOT},
    {"KEY_SLASH",        KEY_SLASH},
    {"KEY_CAPSLOCK",     KEY_CAPSLOCK},

    /* Media keys */
    {"KEY_MUTE",         KEY_MUTE},
    {"KEY_VOLUMEDOWN",   KEY_VOLUMEDOWN},
    {"KEY_VOLUMEUP",     KEY_VOLUMEUP},
    {"KEY_PLAYPAUSE",    KEY_PLAYPAUSE},
    {"KEY_NEXTSONG",     KEY_NEXTSONG},
    {"KEY_PREVIOUSSONG", KEY_PREVIOUSSONG},
    {"KEY_STOPCD",       KEY_STOPCD},

    /* Navigation (browser/file manager) */
    {"KEY_BACK",         KEY_BACK},
    {"KEY_FORWARD",      KEY_FORWARD},

    /* Misc */
    {"KEY_PRINT",        KEY_PRINT},
    {"KEY_SCROLLLOCK",   KEY_SCROLLLOCK},
    {"KEY_PAUSE",        KEY_PAUSE},
    {"KEY_COMPOSE",      KEY_COMPOSE},

    {NULL, 0}
};

static int key_name_to_code(const char *name) {
    for (int i = 0; key_table[i].name != NULL; i++) {
        if (strcmp(name, key_table[i].name) == 0)
            return key_table[i].code;
    }
    return -1;
}

static const char *key_code_to_name(int code) {
    for (int i = 0; key_table[i].name != NULL; i++) {
        if (key_table[i].code == code)
            return key_table[i].name;
    }
    return "UNKNOWN";
}

#endif /* CONFIG_H */
