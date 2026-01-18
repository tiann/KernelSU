package me.weishu.kernelsu.ui.component.sharedTransition

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

@Parcelize
enum class TransitionSource : Parcelable {
    NULL,
    FAB,
    LIST_CARD
}