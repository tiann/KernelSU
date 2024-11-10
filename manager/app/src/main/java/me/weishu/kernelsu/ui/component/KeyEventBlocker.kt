package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.input.key.KeyEvent
import androidx.compose.ui.input.key.onKeyEvent

@Composable
fun KeyEventBlocker(predicate: (KeyEvent) -> Boolean) {
    val requester = remember { FocusRequester() }
    Box(
        Modifier
            .onKeyEvent {
                predicate(it)
            }
            .focusRequester(requester)
            .focusable()
    )
    LaunchedEffect(Unit) {
        requester.requestFocus()
    }
}