#include "linux/kallsyms.h"

#define RE_EXPORT_SYMBOL1(ret, func, t1, v1)                                   \
	ret ksu_##func(t1 v1)                                                  \
	{                                                                      \
		return func(v1);                                               \
	}                                                                      \
	EXPORT_SYMBOL(ksu_##func);

#define RE_EXPORT_SYMBOL2(ret, func, t1, v1, t2, v2)                           \
	ret ksu_##func(t1 v1, t2 v2)                                           \
	{                                                                      \
		return func(v1, v2);                                           \
	}                                                                      \
	EXPORT_SYMBOL(ksu_##func);

RE_EXPORT_SYMBOL1(unsigned long, kallsyms_lookup_name, const char *, name)

// RE_EXPORT_SYMBOL2(int, register_kprobe, struct kprobe *, p)
// RE_EXPORT_SYMBOL2(void, unregister_kprobe, struct kprobe *, p)

// RE_EXPORT_SYMBOL2(int, register_kprobe, struct kprobe *, p)
// RE_EXPORT_SYMBOL2(void, unregister_kprobe, struct kprobe *, p)

// int ksu_register_kprobe(struct kprobe *p);
// void ksu_unregister_kprobe(struct kprobe *p);
// int ksu_register_kprobes(struct kprobe **kps, int num);
// void ksu_unregister_kprobes(struct kprobe **kps, int num);

// int ksu_register_kretprobe(struct kretprobe *rp);
// void unregister_kretprobe(struct kretprobe *rp);
// int register_kretprobes(struct kretprobe **rps, int num);
// void unregister_kretprobes(struct kretprobe **rps, int num);
