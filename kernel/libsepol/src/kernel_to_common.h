// #include <stdio.h>
// #include <stdarg.h>
// #include <sys/types.h>
#include <linux/types.h>

#include <sepol/policydb/avtab.h>
#include <sepol/policydb/policydb.h>


#define STACK_SIZE 16
#define DEFAULT_LEVEL "systemlow"
#define DEFAULT_OBJECT "object_r"

// initial sid names aren't actually stored in the pp files, need to a have
// a mapping, taken from the linux kernel
static const char * const selinux_sid_to_str[] = {
	"null",
	"kernel",
	"security",
	"unlabeled",
	"fs",
	"file",
	"file_labels",
	"init",
	"any_socket",
	"port",
	"netif",
	"netmsg",
	"node",
	"igmp_packet",
	"icmp_socket",
	"tcp_socket",
	"sysctl_modprobe",
	"sysctl",
	"sysctl_fs",
	"sysctl_kernel",
	"sysctl_net",
	"sysctl_net_unix",
	"sysctl_vm",
	"sysctl_dev",
	"kmod",
	"policy",
	"scmp_packet",
	"devnull",
};

#define SELINUX_SID_SZ (sizeof(selinux_sid_to_str)/sizeof(selinux_sid_to_str[0]))

static const char * const xen_sid_to_str[] = {
	"null",
	"xen",
	"dom0",
	"domio",
	"domxen",
	"unlabeled",
	"security",
	"ioport",
	"iomem",
	"irq",
	"device",
	"domU",
	"domDM",
};

#define XEN_SID_SZ (sizeof(xen_sid_to_str)/sizeof(xen_sid_to_str[0]))

static const uint32_t avtab_flavors[] = {
	AVTAB_ALLOWED,
	AVTAB_AUDITALLOW,
	AVTAB_AUDITDENY,
	AVTAB_XPERMS_ALLOWED,
	AVTAB_XPERMS_AUDITALLOW,
	AVTAB_XPERMS_DONTAUDIT,
	AVTAB_TRANSITION,
	AVTAB_MEMBER,
	AVTAB_CHANGE,
};

#define AVTAB_FLAVORS_SZ (sizeof(avtab_flavors)/sizeof(avtab_flavors[0]))

struct strs {
	char **list;
	unsigned num;
	size_t size;
};

__attribute__ ((format(printf, 1, 2)))
void sepol_log_err(const char *fmt, ...);
void sepol_indent(FILE *out, int indent);
__attribute__ ((format(printf, 2, 3)))
void sepol_printf(FILE *out, const char *fmt, ...);

__attribute__ ((format(printf, 1, 3)))
char *create_str(const char *fmt, int num, ...);

int strs_init(struct strs **strs, size_t size);
void strs_destroy(struct strs **strs);
void strs_free_all(struct strs *strs);
int strs_add(struct strs *strs, char *s);
__attribute__ ((format(printf, 2, 4)))
int strs_create_and_add(struct strs *strs, const char *fmt, int num, ...);
char *strs_remove_last(struct strs *strs);
int strs_add_at_index(struct strs *strs, char *s, size_t index);
char *strs_read_at_index(struct strs *strs, size_t index);
void strs_sort(struct strs *strs);
unsigned strs_num_items(const struct strs *strs);
size_t strs_len_items(const struct strs *strs);
char *strs_to_str(const struct strs *strs);
void strs_write_each(const struct strs *strs, FILE *out);
void strs_write_each_indented(const struct strs *strs, FILE *out, int indent);
int hashtab_ordered_to_strs(char *key, void *data, void *args);
int ebitmap_to_strs(const struct ebitmap *map, struct strs *strs, char **val_to_name);
char *ebitmap_to_str(const struct ebitmap *map, char **val_to_name, int sort);

int strs_stack_init(struct strs **stack);
void strs_stack_destroy(struct strs **stack);
int strs_stack_push(struct strs *stack, char *s);
char *strs_stack_pop(struct strs *stack);
int strs_stack_empty(const struct strs *stack);

int sort_ocontexts(struct policydb *pdb);
