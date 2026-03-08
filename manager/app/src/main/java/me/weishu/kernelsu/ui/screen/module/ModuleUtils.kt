package me.weishu.kernelsu.ui.screen.module

import android.content.Context
import me.weishu.kernelsu.ui.util.module.Shortcut

enum class ShortcutType {
    Action,
    WebUI
}

fun hasModuleShortcut(context: Context, moduleId: String, type: ShortcutType): Boolean {
    return when (type) {
        ShortcutType.Action -> Shortcut.hasModuleActionShortcut(context, moduleId)
        ShortcutType.WebUI -> Shortcut.hasModuleWebUiShortcut(context, moduleId)
    }
}

fun deleteModuleShortcut(context: Context, moduleId: String, type: ShortcutType) {
    when (type) {
        ShortcutType.Action -> Shortcut.deleteModuleActionShortcut(context, moduleId)
        ShortcutType.WebUI -> Shortcut.deleteModuleWebUiShortcut(context, moduleId)
    }
}

fun createModuleShortcut(
    context: Context,
    moduleId: String,
    name: String,
    iconUri: String?,
    type: ShortcutType
) {
    when (type) {
        ShortcutType.Action -> {
            Shortcut.createModuleActionShortcut(
                context = context,
                moduleId = moduleId,
                name = name,
                iconUri = iconUri
            )
        }

        ShortcutType.WebUI -> {
            Shortcut.createModuleWebUiShortcut(
                context = context,
                moduleId = moduleId,
                name = name,
                iconUri = iconUri
            )
        }
    }
}
