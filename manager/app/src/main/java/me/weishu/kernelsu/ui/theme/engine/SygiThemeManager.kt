package me.weishu.kernelsu.ui.theme.engine

import android.content.Context
import androidx.core.content.edit
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import me.weishu.kernelsu.ksuApp
import java.io.File

/**
 * Owns installed themes and the active one.
 *
 * Crash guard: every launch increments a counter that is cleared once the UI has rendered.
 * If two launches in a row never got that far, the next launch reverts to the default theme,
 * so a broken theme can never lock you out of your root manager.
 */
object SygiThemeManager {
    private const val PREFS = "settings"
    private const val KEY_ACTIVE = "active_theme"
    private const val KEY_BOOT_ATTEMPTS = "theme_boot_attempts"
    private const val MAX_FAILED_BOOTS = 2

    private val _active = MutableStateFlow<SygiThemePack?>(null)
    val active: StateFlow<SygiThemePack?> = _active

    @Volatile
    private var loaded = false

    private val prefs get() = ksuApp.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    val themesDir: File get() = File(ksuApp.filesDir, "themes")

    /** Call once in MainActivity.onCreate, before setContent. */
    fun init() {
        val attempts = prefs.getInt(KEY_BOOT_ATTEMPTS, 0)
        if (attempts >= MAX_FAILED_BOOTS) {
            prefs.edit {
                remove(KEY_ACTIVE)
                putInt(KEY_BOOT_ATTEMPTS, 0)
            }
        } else {
            prefs.edit { putInt(KEY_BOOT_ATTEMPTS, attempts + 1) }
        }
        load()
    }

    /** Call once the UI has rendered its first frame. */
    fun onStartupSucceeded() {
        if (prefs.getInt(KEY_BOOT_ATTEMPTS, 0) != 0) prefs.edit { putInt(KEY_BOOT_ATTEMPTS, 0) }
    }

    /** For activities that do not call [init] (for example the WebUI). */
    fun ensureLoaded() {
        if (!loaded) load()
    }

    fun load() {
        val id = prefs.getString(KEY_ACTIVE, null)
        _active.value = id?.let { runCatching { SygiThemeParser.parse(File(themesDir, it), it) }.getOrNull() }
        loaded = true
    }

    fun activeId(): String? = prefs.getString(KEY_ACTIVE, null)

    fun list(): List<SygiThemeInfo> =
        themesDir.listFiles { f -> f.isDirectory && !f.name.startsWith(".") }
            ?.sortedBy { it.name }
            ?.map { SygiThemeParser.readInfo(it, it.name) }
            .orEmpty()

    /** Apply an installed theme, or pass null to go back to the default SygiSU look. */
    fun apply(id: String?) {
        prefs.edit {
            if (id == null) remove(KEY_ACTIVE) else putString(KEY_ACTIVE, id)
        }
        load()
    }

    fun delete(id: String) {
        File(themesDir, id).deleteRecursively()
        if (activeId() == id) apply(null)
    }
}
