#define FATAL_ERROR -2

enum json_types {json_false,json_true,json_null,json_object,json_array,json_string,json_number};

static const int CC_SIZE = 10;
static const int CONTROL_CHARS[] = {0, 7, 8, 9, 10, 11, 12, 13, 26, 27}; // see ascii table
static const unsigned char fByte[7] = {0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc};

typedef struct Item {
    int_fast8_t type;
    size_t size; //number of children, x2 in objects (key:pair)
    
    void *value;
   
    struct Item **items; //for both arrays and objects
} item_t;