#include <linux/cpu.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>

#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>
#include <linux/rcupdate.h>

#include <linux/delay.h> // msleep

#include "klog.h"

struct perm_data {
  struct list_head list;
  uid_t uid;
  bool allow;
};

static struct list_head allow_list;

#define KERNEL_SU_DIR "/data/adb/kernelsu"

static struct workqueue_struct *ksu_workqueue;
static struct work_struct ksu_save_work;
static struct work_struct ksu_load_work;

bool persistent_allow_list(void);

bool ksu_allow_uid(uid_t uid, bool allow) {

  // find the node first!
  struct perm_data *p = NULL;
  struct list_head *pos = NULL;
  bool result = false;
  list_for_each(pos, &allow_list) {
    p = list_entry(pos, struct perm_data, list);
    pr_info("ksu_allow_uid :%d, allow: %d\n", p->uid, p->allow);
    if (uid == p->uid) {
      p->allow = allow;
      result = true;
      goto exit;
    }
  }

  // not found, alloc a new node!
  p = (struct perm_data *)kmalloc(sizeof(struct perm_data), GFP_KERNEL);
  if (!p) {
    pr_err("alloc allow node failed.\n");
    return false;
  }
  p->uid = uid;
  p->allow = allow;

  list_add_tail(&p->list, &allow_list);
  result = true;

exit:

  persistent_allow_list();

  return result;
}

bool ksu_is_allow_uid(uid_t uid) {
  struct perm_data *p = NULL;
  struct list_head *pos = NULL;
  list_for_each(pos, &allow_list) {
    p = list_entry(pos, struct perm_data, list);
    pr_info("uid :%d, allow: %d\n", p->uid, p->allow);
    if (uid == p->uid) {
      return p->allow;
    }
  }

  return false;
}

bool ksu_get_allow_list(int *array, int *length, bool allow) {
  struct perm_data *p = NULL;
  struct list_head *pos = NULL;
  int i = 0;
  list_for_each(pos, &allow_list) {
    p = list_entry(pos, struct perm_data, list);
    pr_info("uid: %d allow: %d\n", p->uid, p->allow);
    if (p->allow == allow) {
      array[i++] = p->uid;
    }
  }
  *length = i;

  return true;
}

void do_persistent_allow_list(struct work_struct *work) {
  struct perm_data *p = NULL;
  struct list_head *pos = NULL;
  loff_t off = 0;

  struct file *fp = filp_open("/data/adb/ksu_list", O_WRONLY | O_CREAT, 0644);

  if (IS_ERR(fp)) {
    pr_err("work creat file failed: %d\n", PTR_ERR(fp));
    return;
  }
  pr_info("work create file success!\n");

  list_for_each(pos, &allow_list) {
    p = list_entry(pos, struct perm_data, list);
    pr_info("uid :%d, allow: %d\n", p->uid, p->allow);
    kernel_write(fp, &p->uid, sizeof(p->uid), &off);
    kernel_write(fp, &p->allow, sizeof(p->allow), &off);
  }

  filp_close(fp, 0);
}

void do_load_allow_list(struct work_struct *work) {

  loff_t off = 0;
  ssize_t ret = 0;
  struct file *fp = NULL;

  fp = filp_open("/data/adb/", O_RDONLY, 0);
  if (IS_ERR(fp)) {
    pr_err("work open '/data/adb' failed: %d\n", PTR_ERR(fp));
    mdelay(2000);

    queue_work(ksu_workqueue, &ksu_load_work);
    return;
  }
  filp_close(fp, 0);

  // load allowlist now!
  fp = filp_open("/data/adb/ksu_list", O_RDONLY, 0);

  if (IS_ERR(fp)) {
    pr_err("work open file failed: %d\n", PTR_ERR(fp));
    return;
  }
  pr_info("work open file success!\n");

  while (true) {
    u32 uid;
    bool allow = false;
    ret = kernel_read(fp, &uid, sizeof(uid), &off);
    pr_info("kernel read ret: %d, off: %ld\n", ret, off);
    if (ret <= 0) {
      pr_info("read err: %d\n", ret);
      break;
    }
    ret = kernel_read(fp, &allow, sizeof(allow), &off);

    pr_info("load_allow_uid: %d, allow: %d\n", uid, allow);

    ksu_allow_uid(uid, allow);
  }

  filp_close(fp, 0);
}

static int init_work(void) {
  ksu_workqueue = alloc_workqueue("kernelsu_work", 0, 0);
  INIT_WORK(&ksu_save_work, do_persistent_allow_list);
  INIT_WORK(&ksu_load_work, do_load_allow_list);
  return 0;
}

// make sure allow list works cross boot
bool persistent_allow_list(void) {
  queue_work(ksu_workqueue, &ksu_save_work);
  return true;
}

bool load_allow_list(void) {
  queue_work(ksu_workqueue, &ksu_load_work);
  return true;
}

bool ksu_allowlist_init(void) {

  INIT_LIST_HEAD(&allow_list);

  init_work();

  // load_allow_list();

  return true;
}

bool ksu_allowlist_exit(void) {

  destroy_workqueue(ksu_workqueue);

  return true;
}