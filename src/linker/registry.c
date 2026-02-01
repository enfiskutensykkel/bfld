#include "target.h"
#include "objectfile_reader.h"
#include "archive_reader.h"
#include "utils/list.h"
#include <stdlib.h>
#include <stdint.h>


/*
 * Archive file front-end entry.
 * Used to track registered front-ends.
 */
struct archive_reader_entry
{
    struct list_head node;
    const struct archive_reader *frontend;
};


/*
 * Object file front-end entry
 * Used to track registered front-ends.
 */
struct objectfile_reader_entry
{
    struct list_head node;
    const struct objectfile_reader *frontend;
};


/*
 * Linker machine code architecture target
 */
struct target_entry
{
    struct list_head node;
    const struct target *backend;
    uint32_t march;
};


/*
 * List of object file front-ends.
 */ 
static struct list_head objectfile_readers = LIST_HEAD_INIT(objectfile_readers);


/*
 * List of archive file front-ends.
 */
static struct list_head archive_readers = LIST_HEAD_INIT(archive_readers);


/*
 * List of linker targets.
 */
static struct list_head targets = LIST_HEAD_INIT(targets);


void archive_reader_register(const struct archive_reader *fe)
{
    if (fe == NULL || fe->name == NULL) {
        return;
    }

    if (fe->probe_file == NULL || fe->parse_file == NULL) {
        return;
    }

    struct archive_reader_entry *entry = malloc(sizeof(struct archive_reader_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&archive_readers, &entry->node);
}


void objectfile_reader_register(const struct objectfile_reader *fe)
{
    if (fe == NULL || fe->name == NULL) {
        return;
    }

    if (fe->probe_file == NULL || fe->parse_file == NULL) {
        return;
    }

    struct objectfile_reader_entry *entry = malloc(sizeof(struct objectfile_reader_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&objectfile_readers, &entry->node);
}


void target_register(const struct target *target, uint32_t march)
{
    if (target == NULL || target->name == NULL || march == 0) {
        return;
    }

    if (target->apply_reloc == NULL) {
        return;
    }

    if (target_lookup(march) != NULL) {
        // already registered
        return;
    }

    struct target_entry *entry = malloc(sizeof(struct target_entry));
    if (entry == NULL) {
        return;
    }

    entry->backend = target;
    entry->march = march;
    list_insert_tail(&targets, &entry->node);
}


/*
 * Remove all registered front-ends and back-ends.
 */
__attribute__((destructor(65535)))
static void remove_registered(void)
{
    list_for_each_entry_safe(entry, &objectfile_readers, struct objectfile_reader_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &archive_readers, struct archive_reader_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &targets, struct target_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }
}


const struct objectfile_reader * objectfile_reader_probe(const uint8_t *data, size_t size, uint32_t *march)
{
    uint32_t m = 0;

    list_for_each_entry(entry, &objectfile_readers, struct objectfile_reader_entry, node) {
        const struct objectfile_reader *fe = entry->frontend;

        if (fe->probe_file(data, size, &m)) {
            if (march != NULL) {
                *march = m;
            }
            return fe;
        }
    }
    return NULL;
}


const struct archive_reader * archive_reader_probe(const uint8_t *data, size_t size)
{
    list_for_each_entry(entry, &archive_readers, struct archive_reader_entry, node) {
        const struct archive_reader *fe = entry->frontend;

        if (fe->probe_file(data, size)) {
            return fe;
        }
    }
    return NULL;
}


const struct target * target_lookup(uint32_t march) 
{
    if (march == 0) {
        // TODO: look up the current platform
        list_for_each_entry(entry, &targets, struct target_entry, node) {
            march = entry->march;
            break;
        }
    }

    list_for_each_entry(entry, &targets, struct target_entry, node) {
        if (entry->march == march) {
            return entry->backend;
        }
    }

    return NULL;
}


