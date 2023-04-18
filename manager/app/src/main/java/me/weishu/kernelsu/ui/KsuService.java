package me.weishu.kernelsu.ui;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.annotation.NonNull;

import com.topjohnwu.superuser.ipc.RootService;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import me.weishu.kernelsu.IKsuInterface;

/**
 * @author weishu
 * @date 2023/4/18.
 */

public class KsuService extends RootService {

    private static final String TAG = "KsuService";

    class Stub extends IKsuInterface.Stub {
        @Override
        public List<PackageInfo> getPackages() {
            List<PackageInfo> list = getInstalledPackagesAll();
            Log.i(TAG, "getPackages: " + list.size());
            return list;
        }
    }

    @Override
    public IBinder onBind(@NonNull Intent intent) {
        return new Stub();
    }

    List<Integer> getUserIds() {
        List<Integer> result = new ArrayList<>();
        UserManager um = (UserManager) getSystemService(Context.USER_SERVICE);
        List<UserHandle> userProfiles = um.getUserProfiles();
        for (UserHandle userProfile : userProfiles) {
            int userId = userProfile.hashCode();
            if (userId == 0) {
                continue;
            }
            result.add(userProfile.hashCode());
        }
        return result;
    }

    ArrayList<PackageInfo> getInstalledPackagesAll() {
        ArrayList<PackageInfo> packages = new ArrayList<>();
        for (Integer userId : getUserIds()) {
            Log.i(TAG, "getInstalledPackagesAll: " + userId);
            packages.addAll(getInstalledPackagesAsUser(userId));
        }
        return packages;
    }

    List<PackageInfo> getInstalledPackagesAsUser(int userId) {
        try {
            PackageManager pm = getPackageManager();
            Method getInstalledPackagesAsUser = pm.getClass().getDeclaredMethod("getInstalledPackagesAsUser", int.class, int.class);
            return (List<PackageInfo>) getInstalledPackagesAsUser.invoke(pm, 0, userId);
        } catch (Throwable e) {
            Log.e(TAG, "err", e);
        }

        return new ArrayList<>();
    }
}
