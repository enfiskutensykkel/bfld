#ifndef __PLUGIN_REGISTRY_H__
#define __PLUGIN_REGISTRY_H__

#include <utils/list.h>


/*
 * Linked list of plugins
 */
struct plugin_registry_entry
{
    struct list_head list_node;
    char *name;
    const void *plugin;
};


int plugin_register(struct list_head *registry, const char *name, const void *plugin);


void plugin_unregister(struct plugin_registry_entry *entry);


struct plugin_registry_entry * plugin_find_entry(struct list_head *registry, const char *name);


void plugin_clear_registry(struct list_head *registry);


#endif
