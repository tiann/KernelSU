# KernelSU kernel plugin API

The only exported entry point for the plugin ABI is `ksu_get_api()`.  The
current ABI is `KSU_KERNEL_API_VERSION` (currently `1`); it is independent of
the userspace ioctl version.  A caller passes the size of its table and may
use only fields that fit in that size and whose capability bit is present.

`ksu_get_api()` takes a reference on `kernelsu.ko`.  A plugin must call the
returned `release()` function before unloading.  Event and reboot registrations
require a non-NULL owner, but they do not pin the owner module: a plugin
unregisters from its exit routine, and holding a reference there would make it
impossible to unload.  Unregister returns only after SRCU readers and queued
reboot task work have stopped using the registration, so no callback code runs
after it returns.  A cookie is single-use; unregistering it twice is a bug.

Production builds of `kernelsu.ko` delete their sysfs kobject.  The kernel
then cannot create the `holders` link for a module that imports `ksu_get_api`
through the normal symbol dependency, and plain `insmod` fails with `ENOENT`
without any kernel log.  Load plugins with `ksud insmod`: ksuinit resolves
`ksu_get_api` from `/proc/kallsyms` into an absolute symbol, the kernel records
no dependency, and the reference taken by `ksu_get_api()` is what keeps
`kernelsu.ko` loaded.  `CONFIG_KSU_DEBUG` builds keep the kobject and load
either way.

Self-unregister from an event or reboot callback is deferred until the callback
returns.  Unregistering another handler running on the same task returns
`-EDEADLK`; unregister it from a later work item instead.

Callers may request an older API version; the stable table prefix is copied when
the requested version is supported by the current core.  A newer requested
version is rejected.

Event contexts are read-only snapshots.  Their `size` and `version` fields
must be checked before reading optional fields.  Path and mount target
pointers are valid only while the synchronous callback is running and must
not be retained.  A plugin that needs deferred processing must copy the
fields it needs first.

Callbacks run without KernelSU policy or mount-list locks held.  PRE callbacks
run in ascending priority order and POST callbacks in reverse order.  Set
`KSU_EVENT_HANDLER_REPLAY` when a handler should receive an already-established
replayable state event during registration.  An observer's return value is
ignored.  API v1 defines no policy events, so registering with
`KSU_EVENT_HANDLER_POLICY` is rejected with `-EINVAL`; the flag is reserved
for a future event that explicitly documents a veto.  Callbacks on UID, exec
and umount paths must be short and non-blocking.  Unregistering the running
handler from inside its callback is deferred and succeeds; unregistering a
different handler from a callback returns `-EDEADLK`.

Reboot handlers run as task work on the calling task after the `reboot()`
syscall returns, never inside the kprobe.  A handler must validate the caller
itself: the magic pair is a routing key, not a permission check.

`api/test_module.c` is a smoke test that only uses `ksu_get_api()`.  Build it
with `ddk build -e CONFIG_KSU=m -e CONFIG_KSU_API_TEST=y` and load it with
`ksud insmod ksu_api_test.ko`; it logs the registration checks at load time and
prints per-event counters on unload.

The syscall-table and LSM hook functions remain legacy, non-stable capabilities
for KMI-specific integrations.  Plugins must not assume any KernelSU private
structure layout or register competing core syscall handlers.
