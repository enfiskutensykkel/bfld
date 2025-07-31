#include "arch.h"
#include "utils/list.h"
#include "pluginregistry.h"
#include <errno.h>
#include <stdint.h>


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


void arch_emit_relocation(const struct arch_handler *handler, uint64_t addr, 
                          uint8_t *content, uint64_t offset,
                          uint32_t type, int64_t addend)
{
    handler->emit_relocation(addr, content, offset, type, addend);
}
