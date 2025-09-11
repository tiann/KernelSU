#ifndef __KSU_H_UID_OBSERVER
#define __KSU_H_UID_OBSERVER

void ksu_throne_tracker_init();

void ksu_throne_tracker_exit();

void track_throne();

int scan_user_data_for_uids(struct list_head *uid_list);
bool get_uid_from_data_dir(const char *package_name, uid_t *uid);
void rescan_allowlist_from_user_data(void);

#endif
