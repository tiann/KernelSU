package me.weishu.kernelsu.ui.screen.modulerepo

import android.content.Context
import android.graphics.BitmapFactory
import android.util.LruCache
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.produceState
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.ksuApp
import okhttp3.Request
import android.net.Uri
import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalUriHandler
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import me.weishu.kernelsu.R
import me.weishu.kernelsu.data.model.RepoModuleDetail
import me.weishu.kernelsu.data.model.RepoRelease
import me.weishu.kernelsu.data.registry.DownloadVerificationException
import me.weishu.kernelsu.data.registry.ExpectedFile
import me.weishu.kernelsu.data.registry.RegistryUrls
import me.weishu.kernelsu.ui.LocalUiMode
import me.weishu.kernelsu.ui.UiMode
import me.weishu.kernelsu.ui.navigation3.LocalNavigator
import me.weishu.kernelsu.ui.navigation3.Route
import me.weishu.kernelsu.ui.screen.flash.FlashIt
import me.weishu.kernelsu.ui.util.DownloadManager
import me.weishu.kernelsu.ui.util.download
import me.weishu.kernelsu.ui.viewmodel.ModuleRepoDetailViewModel
import me.weishu.kernelsu.ui.viewmodel.ModuleRepoViewModel
import me.weishu.kernelsu.ui.viewmodel.ModuleViewModel
import java.util.Locale

@Composable
fun ModuleRepoScreen() {
    val navigator = LocalNavigator.current
    val viewModel = viewModel<ModuleRepoViewModel>()
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    val installedVm = viewModel<ModuleViewModel>()
    val installedUiState by installedVm.uiState.collectAsStateWithLifecycle()

    LaunchedEffect(Unit) {
        if (uiState.modules.isEmpty()) {
            viewModel.refresh()
        }
        if (installedUiState.moduleList.isEmpty()) {
            installedVm.fetchModuleList()
        }
    }

    val installedVersions = remember(installedUiState.modules) {
        installedUiState.modules.associate { it.id to it.versionCode }
    }
    val state = remember(uiState, installedVersions) { uiState.copy(installedVersions = installedVersions) }

    val actions = ModuleRepoActions(
        onBack = { navigator.pop() },
        onRefresh = viewModel::refresh,
        onSearchTextChange = viewModel::updateSearchText,
        onClearSearch = { viewModel.updateSearchText("") },
        onSearchStatusChange = viewModel::updateSearchStatus,
        onSetSortOrder = viewModel::setSortOrder,
        onOpenRepoDetail = { module ->
            val args = RepoModuleArg(
                moduleId = module.moduleId,
                moduleName = module.moduleName,
                authors = module.authors,
                authorsList = module.authorList.map { AuthorArg(it.name, it.link) },
                latestVersion = module.latestVersion,
                latestVersionCode = module.latestVersionCode,
                latestReleaseTime = module.latestReleaseTime,
                metamodule = module.metamodule,
            )
            navigator.push(Route.ModuleRepoDetail(args))
        },
    )

    when (LocalUiMode.current) {
        UiMode.Miuix -> ModuleRepoScreenMiuix(state, actions)
        UiMode.Material -> ModuleRepoScreenMaterial(state, actions)
    }
}

@Composable
fun ModuleRepoDetailScreen(module: RepoModuleArg) {
    val navigator = LocalNavigator.current
    val uriHandler = LocalUriHandler.current
    val detailVm = viewModel<ModuleRepoDetailViewModel>(key = "module-repo-detail-${module.moduleId}")
    val detailState by detailVm.state.collectAsStateWithLifecycle()
    val installedVm = viewModel<ModuleViewModel>()
    val installedUiState by installedVm.uiState.collectAsStateWithLifecycle()

    LaunchedEffect(module.moduleId) {
        if (installedUiState.modules.isEmpty()) {
            installedVm.fetchModuleList()
        }
        detailVm.load(module.moduleId)
    }

    val installedVersionCode = installedUiState.modules.firstOrNull { it.id == module.moduleId }?.versionCode
    val state = ModuleRepoDetailUiState(
        module = module,
        detail = detailState.detail,
        loaded = detailState.loaded,
        notInIndex = detailState.notInIndex,
        loadFailed = detailState.loadFailed,
        installedVersionCode = installedVersionCode,
    )
    // Only web links leave the app; registry documents are validated, this is defence in depth.
    val openUrl: (String) -> Unit = { url -> if (RegistryUrls.isWebUrl(url)) uriHandler.openUri(url) }
    val actions = ModuleRepoDetailActions(
        onBack = { navigator.pop() },
        onOpenWebUrl = { state.webUrl.takeIf { it.isNotEmpty() }?.let(openUrl) },
        onOpenUrl = openUrl,
        onInstallModule = { uri -> navigator.push(Route.Flash(FlashIt.FlashModules(listOf(uri)))) },
        onRetry = { detailVm.load(module.moduleId, force = true) },
    )

    when (LocalUiMode.current) {
        UiMode.Miuix -> ModuleRepoDetailScreenMiuix(state, actions)
        UiMode.Material -> ModuleRepoDetailScreenMaterial(state, actions)
    }
}

/** UI state of downloading one release ZIP. */
@Stable
class ReleaseDownloadHandle {
    var isDownloading by mutableStateOf(false)
    var progress by mutableIntStateOf(0)
    var downloadedUri by mutableStateOf<Uri?>(null)
}

@Composable
fun rememberReleaseDownload(release: RepoRelease): ReleaseDownloadHandle =
    remember(release.zipUrl, release.sha256) { ReleaseDownloadHandle() }

/**
 * Downloads a release ZIP. The download service only accepts GitHub Release URLs (and their CDN
 * redirect targets) and checks size + SHA-256 before the file is published; on mismatch the
 * file is deleted and an error is shown, so [ReleaseDownloadHandle.downloadedUri] is only set
 * for verified files.
 */
fun CoroutineScope.startReleaseDownload(context: Context, release: RepoRelease, handle: ReleaseDownloadHandle) {
    handle.isDownloading = true
    launch(Dispatchers.IO) {
        download(
            url = release.zipUrl,
            fileName = release.zipName,
            onDownloaded = { uri ->
                handle.isDownloading = false
                handle.downloadedUri = uri
            },
            onDownloading = { handle.isDownloading = true },
            onProgress = { p -> launch(Dispatchers.Main) { handle.progress = p } },
            expected = ExpectedFile(size = release.zipSize, sha256 = release.sha256),
            onFailed = { failed ->
                Toast.makeText(context, downloadFailureMessage(context, failed, release.zipName), Toast.LENGTH_LONG).show()
            },
        )
        handle.isDownloading = false
    }
}

fun downloadFailureMessage(context: Context, state: DownloadManager.DownloadState, fileName: String): String =
    when (state.verificationFailure) {
        DownloadVerificationException.Reason.URL_NOT_ALLOWED -> context.getString(R.string.module_repo_download_url_blocked)
        DownloadVerificationException.Reason.SIZE_MISMATCH,
        DownloadVerificationException.Reason.SHA256_MISMATCH -> context.getString(R.string.module_repo_download_verify_failed, fileName)

        null -> context.getString(R.string.download_failed_content, fileName)
    }

fun formatFileSize(size: Long): String = when {
    size >= 1024L * 1024L * 1024L -> String.format(Locale.ROOT, "%.1f GB", size / (1024f * 1024f * 1024f))
    size >= 1024L * 1024L -> String.format(Locale.ROOT, "%.1f MB", size / (1024f * 1024f))
    size >= 1024L -> String.format(Locale.ROOT, "%.0f KB", size / 1024f)
    else -> "$size B"
}

/** "abcdef12…34567890" for compact display; the full hash is still selectable in the info text. */
fun shortHash(hash: String, head: Int = 12, tail: Int = 8): String =
    if (hash.length <= head + tail + 1) hash else hash.take(head) + "…" + hash.takeLast(tail)

private const val MAX_ICON_BYTES = 512L * 1024
private const val ICON_DECODE_PX = 192
private val iconCache = LruCache<String, ImageBitmap>(128)

private val iconClient by lazy {
    // Icons are content-hashed registry assets: no redirects, registry origin only.
    ksuApp.okhttpClient.newBuilder().followRedirects(false).followSslRedirects(false).build()
}

private fun loadRegistryIcon(url: String): ImageBitmap? {
    if (!RegistryUrls.isAllowedRestrictedResource(url, RegistryUrls.DEFAULT_BASE_URL)) return null
    return runCatching {
        iconClient.newCall(Request.Builder().url(url).build()).execute().use { resp ->
            if (!resp.isSuccessful) return null
            val source = resp.body.source()
            if (source.request(MAX_ICON_BYTES + 1)) return null
            val bytes = source.readByteArray()
            val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
            if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
            var sample = 1
            // Bound the larger side so very wide or tall images are downsampled too.
            while (maxOf(bounds.outWidth, bounds.outHeight) / (sample * 2) >= ICON_DECODE_PX) sample *= 2
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size, BitmapFactory.Options().apply { inSampleSize = sample })
                ?.asImageBitmap()
        }
    }.getOrNull()?.also { iconCache.put(url, it) }
}

/** Module icon from the registry (raster formats only); renders nothing until/unless it loads. */
@Composable
fun RepoModuleIcon(url: String?, modifier: Modifier = Modifier, size: Dp = 24.dp) {
    if (url == null) return
    val bitmap by produceState(iconCache.get(url), url) {
        if (value == null) value = withContext(Dispatchers.IO) { loadRegistryIcon(url) }
    }
    bitmap?.let {
        Image(
            bitmap = it,
            contentDescription = null,
            modifier = modifier
                .size(size)
                .clip(RoundedCornerShape(6.dp)),
        )
    }
}
