#include "target.h"
#include "objfile_reader.h"
#include "archive_reader.h"
#include "utils/list.h"
#include <stdlib.h>
#include <stdint.h>


/*
 * Archive file front-end entry.
 * Used to track registered front-ends.
 */
struct archive_entry
{
    struct list_head node;
    const struct archive_reader *frontend;
};


/*
 * Object file front-end entry
 * Used to track registered front-ends.
 */
struct objfile_entry
{
    struct list_head node;
    const struct objfile_reader *frontend;
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
static struct list_head objfile_readers = LIST_HEAD_INIT(objfile_readers);


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

    struct archive_entry *entry = malloc(sizeof(struct archive_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&archive_readers, &entry->node);
}


void objfile_reader_register(const struct objfile_reader *fe)
{
    if (fe == NULL || fe->name == NULL) {
        return;
    }

    if (fe->probe_file == NULL || fe->parse_file == NULL) {
        return;
    }

    struct objfile_entry *entry = malloc(sizeof(struct objfile_entry));
    if (entry == NULL) {
        return;
    }

    entry->frontend = fe;
    list_insert_tail(&objfile_readers, &entry->node);
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
    list_for_each_entry_safe(entry, &objfile_readers, struct objfile_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &archive_readers, struct archive_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }

    list_for_each_entry_safe(entry, &targets, struct target_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }
}


const struct objfile_reader * objfile_reader_probe(const uint8_t *data, size_t size, uint32_t *march)
{
    uint32_t m = 0;

    list_for_each_entry(entry, &objfile_readers, struct objfile_entry, node) {
        const struct objfile_reader *fe = entry->frontend;

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
    list_for_each_entry(entry, &archive_readers, struct archive_entry, node) {
        const struct archive_reader *fe = entry->frontend;

        if (fe->probe_file(data, size)) {
            return fe;
        }
    }
    return NULL;
}


const struct target * target_lookup(uint32_t march) 
{
    list_for_each_entry(entry, &targets, struct target_entry, node) {
        if (entry->march == march) {
            return entry->backend;
        }
    }

    return NULL;
}


