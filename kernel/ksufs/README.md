# KSUFS

based on overlayfs

## Step1

```bash
sed -i 's/ovl_/ksu_ovl_/g' *
sed -i 's/OVL_/KSU_OVL_/g' *
mv ovl_entry.h ksu_ovl_entry.h
sed -i 's/I_KSU_OVL_INUSE/I_OVL_INUSE/g' *
```

## Step2

overlayfs.h:

#define pr_fmt(fmt) "overlayfs: " fmt

#define pr_fmt(fmt) "ksufs: " fmt

## Step3

```c
static struct file_system_type ksu_ovl_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "overlay",
	.mount		= ksu_ovl_mount,
	.kill_sb	= kill_anon_super,
};
MODULE_ALIAS_FS("overlay");
```

```c
static struct file_system_type ksu_ovl_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "ksufs",
	.mount		= ksu_ovl_mount,
	.kill_sb	= kill_anon_super,
};
MODULE_ALIAS_FS("ksufs");
```


## Step4

ksu_ovl_getattr:

if (err)

if (true)

## Step5

Makefile:

obj-y += ksufs.o

ksufs-objs := super.o namei.o util.o inode.o file.o dir.o readdir.o \
		copy_up.o export.o