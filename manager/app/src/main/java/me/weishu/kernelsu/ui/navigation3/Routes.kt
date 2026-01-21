package me.weishu.kernelsu.ui.navigation3

import androidx.navigation3.runtime.NavKey
import kotlinx.serialization.Contextual
import kotlinx.serialization.Serializable
import me.weishu.kernelsu.ui.screen.FlashIt
import me.weishu.kernelsu.ui.screen.RepoModuleArg
import me.weishu.kernelsu.ui.util.FlashItSerializer
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel

/**
 * Type-safe navigation keys for Navigation3.
 * Each destination is a NavKey (data object/data class) and can be saved/restored in the back stack.
 */
sealed interface Route : NavKey {
    @Serializable
    data object Main : Route

    @Serializable
    data object Home : Route

    @Serializable
    data object SuperUser : Route

    @Serializable
    data object Module : Route

    @Serializable
    data object Settings : Route

    @Serializable
    data object About : Route

    @Serializable
    data object AppProfileTemplate : Route

    @Serializable
    data class TemplateEditor(val template: TemplateViewModel.TemplateInfo, val readOnly: Boolean) : Route

    @Serializable
    data class AppProfile(val packageName: String) : Route

    @Serializable
    data object Install : Route

    @Serializable
    data class ModuleRepoDetail(val module: @Contextual RepoModuleArg) : Route

    @Serializable
    data object ModuleRepo : Route

    @Serializable
    data class Flash(@Serializable(with = FlashItSerializer::class) val flashIt: FlashIt) : Route

    @Serializable
    data class ExecuteModuleAction(val moduleId: String) : Route
}
