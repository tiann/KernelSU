#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include "klog.h" // IWYU pragma: keep

// we should have no usages
// module_mutex become static since 5.15, so disable this
/*
static void del_usage_links(struct module *mod)
{
	struct module_use *use;

	mutex_lock(&module_mutex);
	list_for_each_entry(use, &mod->target_list, target_list)
		sysfs_remove_link(use->target->holders_dir, mod->name);
	mutex_unlock(&module_mutex);
}*/

static void __init module_remove_modinfo_attrs(struct module *mod, int end)
{
    struct module_attribute *attr;
    int i;

    for (i = 0; (attr = &mod->modinfo_attrs[i]); i++) {
        if (end >= 0 && i > end)
            break;
        /* pick a field to test for end of list */
        if (!attr->attr.name)
            break;
        sysfs_remove_file(&mod->mkobj.kobj, &attr->attr);
        if (attr->free)
            attr->free(mod);
    }
    kfree(mod->modinfo_attrs);
    // ADDED:
    mod->modinfo_attrs = kzalloc(sizeof(struct module_attribute), GFP_KERNEL);
}

struct module_notes_attrs {
    struct kobject *dir;
    unsigned int notes;
    struct bin_attribute attrs[];
};

static void __init free_notes_attrs(struct module_notes_attrs *notes_attrs, unsigned int i)
{
    if (notes_attrs->dir) {
        while (i-- > 0)
            sysfs_remove_bin_file(notes_attrs->dir, &notes_attrs->attrs[i]);
        kobject_put(notes_attrs->dir);
    }
    kfree(notes_attrs);
}

static void __init remove_notes_attrs(struct module *mod)
{
    if (mod->notes_attrs)
        free_notes_attrs(mod->notes_attrs, mod->notes_attrs->notes);
    // ADDED
    mod->notes_attrs = NULL;
}

struct module_sect_attr {
    struct bin_attribute battr;
    unsigned long address;
};

struct module_sect_attrs {
    struct attribute_group grp;
    unsigned int nsections;
    struct module_sect_attr attrs[];
};

static void __init free_sect_attrs(struct module_sect_attrs *sect_attrs)
{
    unsigned int section;

    for (section = 0; section < sect_attrs->nsections; section++)
        kfree(sect_attrs->attrs[section].battr.attr.name);
    kfree(sect_attrs);
}

static void __init remove_sect_attrs(struct module *mod)
{
    if (mod->sect_attrs) {
        sysfs_remove_group(&mod->mkobj.kobj, &mod->sect_attrs->grp);
        /* We are positive that no one is using any sect attrs
		 * at this point.  Deallocate immediately. */
        free_sect_attrs(mod->sect_attrs);
        mod->sect_attrs = NULL;
    }
}

static void mod_kobject_put(struct module *mod)
{
    DECLARE_COMPLETION_ONSTACK(c);
    mod->mkobj.kobj_completion = &c;
    kobject_put(&mod->mkobj.kobj);
    wait_for_completion(&c);
}

static void __init mod_sysfs_fini(struct module *mod)
{
    remove_notes_attrs(mod);
    remove_sect_attrs(mod);
    mod_kobject_put(mod);
}

void my_release(struct kobject *kobj)
{
    struct module_kobject *mk = container_of(kobj, struct module_kobject, kobj);

    if (mk->kobj_completion)
        complete(mk->kobj_completion);
}

struct kobj_type my_type = { .release = my_release };

// copy from mod_sysfs_teardown of module.c(module/main.c)
// unstable, may panic, so it's an optional feature
void __init remove_my_kobj(void)
{
    struct module *mod = THIS_MODULE;
    // del_usage_links(mod);
    module_remove_modinfo_attrs(mod, -1);
    module_param_sysfs_remove(mod);
    kobject_put(mod->mkobj.drivers_dir);
    kobject_put(mod->holders_dir);
    mod_sysfs_fini(mod);

    // ADDED
    pr_info("refcnt: %d state_in_sysfs: %d state_initialized: %d parent: 0x%lx\n",
            mod->mkobj.kobj.kref.refcount.refs.counter, mod->mkobj.kobj.state_in_sysfs,
            mod->mkobj.kobj.state_initialized, (unsigned long)mod->mkobj.kobj.parent);
    mod->mkobj.kobj.state_initialized = 1;
    mod->mkobj.kobj.kref.refcount.refs.counter = 1;
    mod->mkobj.kobj.ktype = &my_type;
    mod->mkobj.kobj.name = NULL;
    mod->mkobj.kobj.parent = NULL;
    mod->holders_dir = NULL;
}
