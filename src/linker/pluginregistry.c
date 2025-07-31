#include "logging.h"
#include "objfile.h"
#include "objfile_loader.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pluginregistry.h"


void plugin_unregister(struct plugin_registry_entry *plugin)
{
    list_remove(&plugin->list_node);
    free(plugin->name);
    free(plugin);
}


struct plugin_registry_entry * plugin_find_entry(struct list_head *registry, const char *name)
{
    list_for_each_entry(entry, registry, struct plugin_registry_entry, list_node) {
        if (strcmp(name, entry->name) == 0) {
            return entry;
        }
    }

    return NULL;
}


int plugin_register(struct list_head *registry, const char *name, const void *plugin)
{
    struct plugin_registry_entry *entry;

    if (name == NULL) {
        return EINVAL;
    }

    entry = plugin_find_entry(registry, name);
    if (entry != NULL) {
        return EEXIST;
    }

    entry = malloc(sizeof(struct plugin_registry_entry));
    if (entry == NULL) {
        return ENOMEM;
    }

    entry->name = strdup(name);
    if (entry->name == NULL) {
        free(entry);
        return ENOMEM;
    }

    entry->plugin = plugin;
    list_insert_tail(registry, &entry->list_node);
    return 0;
}


void plugin_clear_registry(struct list_head *registry)
{
    list_for_each_entry_safe(entry, registry, struct plugin_registry_entry, list_node) {
        plugin_unregister(entry);
    }
}

