package me.weishu.kernelsu;

/**
 * @author weishu
 * @date 2022/12/8.
 */
public final class Natives {

    static {
       System.loadLibrary("kernelsu");
    }

    // become root manager, return true if success.
    public static native boolean becomeManager(String pkg);

    public static native int getVersion();

    // get the uid list of allowed su processes.
    public static native int[] getAllowList();

    public static native int[] getDenyList();

    public static native boolean allowRoot(int uid, boolean allow);

    public static native boolean isSafeMode();

    public static native boolean isAllowlistMode();

    public static native boolean setAllowlistMode(boolean isAllowlist);

    public static native boolean isUidInAllowlist(int uid);

    public static native boolean isUidInDenylist(int uid);

    public static native boolean addUidToAllowlist(int uid);

    public static native boolean removeUidFromAllowlist(int uid);

    public static native boolean addUidToDenylist(int uid);

    public static native boolean removeUidFromDenylist(int uid);
}
