#include "arch.h"
#include "reloc.h"
#include "utils/list.h"
#include "pluginregistry.h"
#include <errno.h>
#include <stdint.h>
#include "logging.h"


static struct list_head arch_handlers = LIST_HEAD_INIT(arch_handlers);


__attribute__((destructor(65535)))
static void unregister_loaders(void)
{
    plugin_clear_registry(&arch_handlers);
}


int arch_handler_register(const struct arch_handler *handler)
{
    if (handler == NULL) {
        return EINVAL;
    }

    list_for_each_entry(entry, &arch_handlers, struct plugin_registry_entry, list_node) {
        const struct arch_handler *handler = entry->plugin;

        if (handler->arch_type == handler->arch_type) {
            return EEXIST;
        }
    }

    return plugin_register(&arch_handlers, handler->name, handler);
}


void arch_apply_relocation(const struct arch_handler *handler,
                           const struct relocation *reloc,
                           uint64_t base_addr,
                           uint8_t *content)
{
    if (reloc->type == 0 || reloc->type > 255) {
        log_error("Invalid relocation type 0x%x", reloc->type);
        return;
    }

    handler->apply_relocation(reloc, content);
}
