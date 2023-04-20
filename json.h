enum json_types {json_false,json_true,json_null,json_object,json_array,json_string,json_number};
enum number_type {num_int, num_float};

typedef struct Item {
    int type;
    int size; // number of children
    
    int num_type;
    int64_t int_num;
    double f_num;
    
    char *str;

    struct Item **obj_items;
    struct Item *obj_value;
    struct Item *prev;
} item_t;