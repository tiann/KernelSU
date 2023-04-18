// IKsuInterface.aidl
package me.weishu.kernelsu;

import android.content.pm.PackageInfo;
import java.util.List;

interface IKsuInterface {
    List<PackageInfo> getPackages();
}