

enum input_sect_type
{
    INPUT_SECT_TYPE_UNKNOWN,
    INPUT_SECT_TYPE_TEXT, 
    INPUT_SECT_TYPE_DATA,
    INPUT_SECT_TYPE_BSS
};


/*
 * Input section from an object file, containing data contents that is to be 
 * loaded in to memory.
 */
struct input_sect
{
    struct input_objfile *objfile;  // Object file this section came from
    struct list_head entry;         // Linked list entry
    uint32_t idx;                   // Section index
    size_t size;                    // Section size
    uint64_t offset;                // Offset from start of object file
    enum input_sect_type type;      // What kind of content is this?
    const void *content;            // Pointer to content data
};



struct input_sym
{
    struct list_head entry;
    uint32_t idx;
    uint64_t value;
};



struct input_rel
{
    struct list_head entry;
};


