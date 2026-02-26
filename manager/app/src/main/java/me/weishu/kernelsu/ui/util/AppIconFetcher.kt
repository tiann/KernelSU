package me.weishu.kernelsu.ui.util

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import androidx.annotation.Px
import coil3.ImageLoader
import coil3.asImage
import coil3.decode.DataSource
import coil3.fetch.FetchResult
import coil3.fetch.Fetcher
import coil3.fetch.ImageFetchResult
import coil3.request.Options
import coil3.size.Dimension
import me.zhanghai.android.appiconloader.AppIconLoader


class AppIconFetcher(
    private val loader: AppIconLoader,
    private val applicationInfo: ApplicationInfo
) : Fetcher {

    override suspend fun fetch(): FetchResult {
        val icon = loader.loadIcon(applicationInfo)
        return ImageFetchResult(
            icon.asImage(),
            true, DataSource.DISK
        )
    }

    class Factory(
        @field:Px private val iconSize: Int,
        private val shrinkNonAdaptiveIcons: Boolean = false,
    ) : Fetcher.Factory<PackageInfo> {
        override fun create(
            data: PackageInfo,
            options: Options,
            imageLoader: ImageLoader
        ): AppIconFetcher? {
            return data.applicationInfo?.let {
                AppIconFetcher(
                    AppIconLoader(iconSize, shrinkNonAdaptiveIcons, options.context),
                    it
                )
            }
        }
    }
}