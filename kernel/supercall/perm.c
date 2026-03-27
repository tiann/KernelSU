#include <linux/types.h>

#include "supercall/internal.h"
#include "manager/manager_identity.h"
#include "policy/allowlist.h"

bool only_manager(void)
{
    return is_manager();
}

bool only_root(void)
{
    return current_uid().val == 0;
}

bool manager_or_root(void)
{
    return current_uid().val == 0 || is_manager();
}

bool always_allow(void)
{
    return true;
}

bool allowed_for_su(void)
{
    return is_manager() || ksu_is_allow_uid_for_current(current_uid().val);
}
