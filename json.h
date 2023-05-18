#define FATAL_ERROR     -2
#define FLOAT_DIGITS    9

#define LAST_ITEM       0x10
#define EMPTY_ITEM      0x20

static const enum json_types {
    json_undefined,
    json_false,
    json_true,
    json_null,
    json_object,
    json_array,
    json_string,
    json_number
} __attribute__ ((__packed__)) json_types;

static const enum control_chars {
    NUL = 0,
    BEL = 7,
    BS  = 8,
    TAB = 9,
    LF  = 10,
    VT  = 11,
    FF  = 12,
    CR  = 13,
    SUB = 26,
    ESC = 27
} __attribute__ ((__packed__)) control_chars;

// transcode depends on length of utf char
static const unsigned char fByte[7] = {0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc};

typedef struct Item {
    int8_t type;
    void *value;
} item_t;

int parse_whitespace(FILE *fptr, int *restrict position, int *restrict line);
static int parse_number(FILE *fptr, int *restrict position, int *restrict line, item_t *parent);
int parse_value(FILE *fptr, int *restrict position, int *restrict line, item_t *parent);
int parse_array(FILE *fptr, int *restrict position, int *restrict line, item_t *parent);
int parse_object(FILE *fptr, int *restrict position, int *restrict line, item_t *parent);
int parse_bool(FILE *fptr, int *restrict position, int *restrict line, char *bool_str);
int parse_json(char *str);

int print_all(item_t *head, int indent_s, size_t depth);
int free_everything(item_t *Node);