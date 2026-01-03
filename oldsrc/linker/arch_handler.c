#include "archtypes.h"
#include "arch_handler.h"
#include "utils/list.h"
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include "logging.h"


static struct list_head handlers = LIST_HEAD_INIT(handlers);


struct handler_entry
{
    struct list_head node;
    const struct arch_handler *handler;
};


__attribute__((destructor(65535)))
static void remove_handlers(void)
{
    list_for_each_entry_safe(entry, &handlers, struct handler_entry, node) {
        list_remove(&entry->node);
        free(entry);
    }
}


const struct arch_handler * arch_handler_find(enum arch_type type)
{
    list_for_each_entry(entry, &handlers, struct handler_entry, node) {
        const struct arch_handler *handler = entry->handler;

        if (handler->arch_type == type) {
            return handler;
        }
    }

    return NULL;
}


int arch_handler_register(const struct arch_handler *handler)
{
    if (handler == NULL) {
        return EINVAL;
    }

    struct handler_entry *entry = malloc(sizeof(struct handler_entry));
    if (entry == NULL) {
        return ENOMEM;
    }

    entry->handler = handler;
    list_insert_tail(&handlers, &entry->node);
    return 0;
}


void arch_apply_relocation(const struct arch_handler *handler,
                           const struct arch_relocation *reloc,
                           uint64_t base_addr,
                           uint8_t *content)
{
    if (reloc->type == 0 || reloc->type >= ARCH_HANDLER_MAX_RELOC_TYPES) {
        log_error("Invalid relocation type 0x%x", reloc->type);
        return;
    }

    handler->apply_relocation(reloc, base_addr, content);
}
