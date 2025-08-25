package me.weishu.kernelsu.ui.component

import androidx.compose.runtime.Composable
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.ksuApp

@Composable
fun KsuIsValid(
    content: @Composable () -> Unit
) {
    val isManager = Natives.becomeManager(ksuApp.packageName)
    val ksuVersion = if (isManager) Natives.version else null

    if (ksuVersion != null) {
        content()
    }
}
