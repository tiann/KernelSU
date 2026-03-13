package me.weishu.kernelsu.ui.screen.module

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import me.weishu.kernelsu.data.model.Module
import me.weishu.kernelsu.ui.util.module.Shortcut

@Stable
class ModuleShortcutState internal constructor(
    moduleId: String?,
    name: String,
    iconUri: String?,
    selectedTypeName: String?,
    supportsActionShortcut: Boolean,
    supportsWebUiShortcut: Boolean,
    defaultActionIconUri: String?,
    defaultWebUiIconUri: String?,
) {
    var moduleId by mutableStateOf(moduleId)
        private set

    var name by mutableStateOf(name)

    var iconUri by mutableStateOf(iconUri)
        private set

    var selectedType by mutableStateOf(selectedTypeName?.let(ShortcutType::valueOf))
        private set

    var supportsActionShortcut by mutableStateOf(supportsActionShortcut)
        private set

    var supportsWebUiShortcut by mutableStateOf(supportsWebUiShortcut)
        private set

    var defaultActionIconUri by mutableStateOf(defaultActionIconUri)
        private set

    var defaultWebUiIconUri by mutableStateOf(defaultWebUiIconUri)
        private set

    var previewIcon by mutableStateOf<ImageBitmap?>(null)
        internal set

    var hasExistingShortcut by mutableStateOf(false)
        internal set

    val availableTypes: List<ShortcutType>
        get() = buildList {
            if (supportsActionShortcut) add(ShortcutType.Action)
            if (supportsWebUiShortcut) add(ShortcutType.WebUI)
        }

    val defaultShortcutIconUri: String?
        get() = selectedType?.let(::defaultIconFor)

    fun bindModule(module: Module) {
        moduleId = module.id
        name = module.name
        iconUri = null
        selectedType = null
        supportsActionShortcut = module.hasActionScript
        supportsWebUiShortcut = module.hasWebUi
        defaultActionIconUri = module.actionIconPath
            ?.takeIf { it.isNotBlank() }
            ?.let { "su:$it" }
        defaultWebUiIconUri = module.webUiIconPath
            ?.takeIf { it.isNotBlank() }
            ?.let { "su:$it" }
    }

    fun selectType(type: ShortcutType) {
        selectedType = type
        iconUri = defaultIconFor(type)
    }

    fun updateName(value: String) {
        name = value
    }

    fun updateIconUri(value: String?) {
        iconUri = value
    }

    fun resetIconToDefault() {
        iconUri = defaultShortcutIconUri
    }

    fun createShortcut(context: Context) {
        val currentModuleId = moduleId
        val type = selectedType
        if (currentModuleId.isNullOrBlank() || name.isBlank() || type == null) {
            return
        }
        createModuleShortcut(
            context = context,
            moduleId = currentModuleId,
            name = name,
            iconUri = iconUri,
            type = type,
        )
        hasExistingShortcut = true
    }

    fun deleteShortcut(context: Context) {
        val currentModuleId = moduleId
        val type = selectedType
        if (currentModuleId.isNullOrBlank() || type == null) {
            return
        }
        deleteModuleShortcut(context, currentModuleId, type)
        hasExistingShortcut = false
    }

    private fun defaultIconFor(type: ShortcutType): String? {
        return when (type) {
            ShortcutType.Action -> defaultActionIconUri ?: defaultWebUiIconUri
            ShortcutType.WebUI -> defaultWebUiIconUri ?: defaultActionIconUri
        }
    }

    companion object {
        val Saver = listSaver<ModuleShortcutState, Any?>(
            save = {
                listOf(
                    it.moduleId,
                    it.name,
                    it.iconUri,
                    it.selectedType?.name,
                    it.supportsActionShortcut,
                    it.supportsWebUiShortcut,
                    it.defaultActionIconUri,
                    it.defaultWebUiIconUri,
                )
            },
            restore = {
                ModuleShortcutState(
                    moduleId = it[0] as String?,
                    name = it[1] as String,
                    iconUri = it[2] as String?,
                    selectedTypeName = it[3] as String?,
                    supportsActionShortcut = it[4] as Boolean,
                    supportsWebUiShortcut = it[5] as Boolean,
                    defaultActionIconUri = it[6] as String?,
                    defaultWebUiIconUri = it[7] as String?,
                )
            }
        )
    }
}

@Composable
fun rememberModuleShortcutState(
    context: Context,
): ModuleShortcutState {
    val state = rememberSaveable(saver = ModuleShortcutState.Saver) {
        ModuleShortcutState(
            moduleId = null,
            name = "",
            iconUri = null,
            selectedTypeName = null,
            supportsActionShortcut = false,
            supportsWebUiShortcut = false,
            defaultActionIconUri = null,
            defaultWebUiIconUri = null,
        )
    }

    LaunchedEffect(state.iconUri) {
        val uri = state.iconUri
        if (uri.isNullOrBlank()) {
            state.previewIcon = null
            return@LaunchedEffect
        }
        val bitmap = withContext(Dispatchers.IO) {
            Shortcut.loadShortcutBitmap(context, uri)
        }
        state.previewIcon = bitmap?.asImageBitmap()
    }

    LaunchedEffect(state.moduleId, state.selectedType) {
        val moduleId = state.moduleId
        val type = state.selectedType
        if (moduleId.isNullOrBlank() || type == null) {
            state.hasExistingShortcut = false
            return@LaunchedEffect
        }
        state.hasExistingShortcut = withContext(Dispatchers.IO) {
            hasModuleShortcut(context, moduleId, type)
        }
    }

    return remember(state) { state }
}
