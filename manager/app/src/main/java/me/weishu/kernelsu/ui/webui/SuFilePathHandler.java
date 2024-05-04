package me.weishu.kernelsu.ui.webui;

import android.content.Context;
import android.util.Log;
import android.webkit.WebResourceResponse;

import androidx.annotation.NonNull;
import androidx.annotation.WorkerThread;
import androidx.webkit.WebViewAssetLoader;

import com.topjohnwu.superuser.Shell;
import com.topjohnwu.superuser.io.SuFile;
import com.topjohnwu.superuser.io.SuFileInputStream;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.GZIPInputStream;

/**
 * Handler class to open files from file system by root access
 * For more information about android storage please refer to
 * <a href="https://developer.android.com/guide/topics/data/data-storage">Android Developers
 * Docs: Data and file storage overview</a>.
 * <p class="note">
 * To avoid leaking user or app data to the web, make sure to choose {@code directory}
 * carefully, and assume any file under this directory could be accessed by any web page subject
 * to same-origin rules.
 * <p>
 * A typical usage would be like:
 * <pre class="prettyprint">
 * File publicDir = new File(context.getFilesDir(), "public");
 * // Host "files/public/" in app's data directory under:
 * // http://appassets.androidplatform.net/public/...
 * WebViewAssetLoader assetLoader = new WebViewAssetLoader.Builder()
 *          .addPathHandler("/public/", new InternalStoragePathHandler(context, publicDir))
 *          .build();
 * </pre>
 */
public final class SuFilePathHandler implements WebViewAssetLoader.PathHandler {
    private static final String TAG = "SuFilePathHandler";

    /**
     * Default value to be used as MIME type if guessing MIME type failed.
     */
    public static final String DEFAULT_MIME_TYPE = "text/plain";

    /**
     * Forbidden subdirectories of {@link Context#getDataDir} that cannot be exposed by this
     * handler. They are forbidden as they often contain sensitive information.
     * <p class="note">
     * Note: Any future addition to this list will be considered breaking changes to the API.
     */
    private static final String[] FORBIDDEN_DATA_DIRS =
            new String[] {"/data/data", "/data/system"};

    @NonNull
    private final File mDirectory;

    private final Shell mShell;

    /**
     * Creates PathHandler for app's internal storage.
     * The directory to be exposed must be inside either the application's internal data
     * directory {@link Context#getDataDir} or cache directory {@link Context#getCacheDir}.
     * External storage is not supported for security reasons, as other apps with
     * {@link android.Manifest.permission#WRITE_EXTERNAL_STORAGE} may be able to modify the
     * files.
     * <p>
     * Exposing the entire data or cache directory is not permitted, to avoid accidentally
     * exposing sensitive application files to the web. Certain existing subdirectories of
     * {@link Context#getDataDir} are also not permitted as they are often sensitive.
     * These files are ({@code "app_webview/"}, {@code "databases/"}, {@code "lib/"},
     * {@code "shared_prefs/"} and {@code "code_cache/"}).
     * <p>
     * The application should typically use a dedicated subdirectory for the files it intends to
     * expose and keep them separate from other files.
     *
     * @param context {@link Context} that is used to access app's internal storage.
     * @param directory the absolute path of the exposed app internal storage directory from
     *                  which files can be loaded.
     * @throws IllegalArgumentException if the directory is not allowed.
     */
    public SuFilePathHandler(@NonNull Context context, @NonNull File directory, Shell rootShell) {
        try {
            mDirectory = new File(getCanonicalDirPath(directory));
            if (!isAllowedInternalStorageDir(context)) {
                throw new IllegalArgumentException("The given directory \"" + directory
                        + "\" doesn't exist under an allowed app internal storage directory");
            }
            mShell = rootShell;
        } catch (IOException e) {
            throw new IllegalArgumentException(
                    "Failed to resolve the canonical path for the given directory: "
                            + directory.getPath(), e);
        }
    }

    private boolean isAllowedInternalStorageDir(@NonNull Context context) throws IOException {
        String dir = getCanonicalDirPath(mDirectory);

        for (String forbiddenPath : FORBIDDEN_DATA_DIRS) {
            if (dir.startsWith(forbiddenPath)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Opens the requested file from the exposed data directory.
     * <p>
     * The matched prefix path used shouldn't be a prefix of a real web path. Thus, if the
     * requested file cannot be found or is outside the mounted directory a
     * {@link WebResourceResponse} object with a {@code null} {@link InputStream} will be
     * returned instead of {@code null}. This saves the time of falling back to network and
     * trying to resolve a path that doesn't exist. A {@link WebResourceResponse} with
     * {@code null} {@link InputStream} will be received as an HTTP response with status code
     * {@code 404} and no body.
     * <p class="note">
     * The MIME type for the file will be determined from the file's extension using
     * {@link java.net.URLConnection#guessContentTypeFromName}. Developers should ensure that
     * files are named using standard file extensions. If the file does not have a
     * recognised extension, {@code "text/plain"} will be used by default.
     *
     * @param path the suffix path to be handled.
     * @return {@link WebResourceResponse} for the requested file.
     */
    @Override
    @WorkerThread
    @NonNull
    public WebResourceResponse handle(@NonNull String path) {
        try {
            File file = getCanonicalFileIfChild(mDirectory, path);
            if (file != null) {
                InputStream is = openFile(file, mShell);
                String mimeType = guessMimeType(path);
                return new WebResourceResponse(mimeType, null, is);
            } else {
                Log.e(TAG, String.format(
                        "The requested file: %s is outside the mounted directory: %s", path,
                        mDirectory));
            }
        } catch (IOException e) {
            Log.e(TAG, "Error opening the requested path: " + path, e);
        }
        return new WebResourceResponse(null, null, null);
    }

    public static String getCanonicalDirPath(@NonNull File file) throws IOException {
        String canonicalPath = file.getCanonicalPath();
        if (!canonicalPath.endsWith("/")) canonicalPath += "/";
        return canonicalPath;
    }

    public static File getCanonicalFileIfChild(@NonNull File parent, @NonNull String child)
            throws IOException {
        String parentCanonicalPath = getCanonicalDirPath(parent);
        String childCanonicalPath = new File(parent, child).getCanonicalPath();
        if (childCanonicalPath.startsWith(parentCanonicalPath)) {
            return new File(childCanonicalPath);
        }
        return null;
    }

    @NonNull
    private static InputStream handleSvgzStream(@NonNull String path,
                                                @NonNull InputStream stream) throws IOException {
        return path.endsWith(".svgz") ? new GZIPInputStream(stream) : stream;
    }

    public static InputStream openFile(@NonNull File file, @NonNull Shell shell) throws IOException {
        SuFile suFile = new SuFile(file.getAbsolutePath());
        suFile.setShell(shell);
        InputStream fis = SuFileInputStream.open(suFile);
        return handleSvgzStream(file.getPath(), fis);
    }

    /**
     * Use {@link MimeUtil#getMimeFromFileName} to guess MIME type or return the
     * {@link #DEFAULT_MIME_TYPE} if it can't guess.
     *
     * @param filePath path of the file to guess its MIME type.
     * @return MIME type guessed from file extension or {@link #DEFAULT_MIME_TYPE}.
     */
    @NonNull
    public static String guessMimeType(@NonNull String filePath) {
        String mimeType = MimeUtil.getMimeFromFileName(filePath);
        return mimeType == null ? DEFAULT_MIME_TYPE : mimeType;
    }
}
