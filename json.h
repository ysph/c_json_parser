enum json_types {json_false,json_true,json_null,json_object,json_array,json_string,json_number};

typedef struct Item {
    int type;
    int size; //number of children, x2 in objects (key:pair)
    
    void *value;
   
    struct Item **items; //for both arrays and objects
} item_t;