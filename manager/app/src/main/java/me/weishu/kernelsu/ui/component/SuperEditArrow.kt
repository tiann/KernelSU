package me.weishu.kernelsu.ui.component

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.component.filter.FilterNumber
import top.yukonga.miuix.kmp.basic.BasicComponentColors
import top.yukonga.miuix.kmp.basic.BasicComponentDefaults
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.extra.RightActionColors
import top.yukonga.miuix.kmp.extra.SuperArrow
import top.yukonga.miuix.kmp.extra.SuperArrowDefaults
import top.yukonga.miuix.kmp.extra.SuperDialog

@Composable
fun SuperEditArrow(
    title: String,
    titleColor: BasicComponentColors = BasicComponentDefaults.titleColor(),
    defaultValue: Int = -1,
    summaryColor: BasicComponentColors = BasicComponentDefaults.summaryColor(),
    leftAction: @Composable (() -> Unit)? = null,
    rightActionColor: RightActionColors = SuperArrowDefaults.rightActionColors(),
    modifier: Modifier = Modifier,
    insideMargin: PaddingValues = BasicComponentDefaults.InsideMargin,
    enabled: Boolean = true,
    onValueChange: ((Int) -> Unit)? = null
) {
    val showDialog = remember { mutableStateOf(false) }
    val dialogTextFieldValue = remember { mutableIntStateOf(defaultValue) }

    SuperArrow(
        title = title,
        titleColor = titleColor,
        summary = dialogTextFieldValue.intValue.toString(),
        summaryColor = summaryColor,
        leftAction = leftAction,
        rightActionColor = rightActionColor,
        modifier = modifier,
        insideMargin = insideMargin,
        onClick = {
            showDialog.value = true
        },
        holdDownState = showDialog.value,
        enabled = enabled
    )

    EditDialog(
        title,
        showDialog,
        dialogTextFieldValue = dialogTextFieldValue.intValue,
    ) {
        dialogTextFieldValue.intValue = it
        onValueChange?.invoke(dialogTextFieldValue.intValue)
    }

}

@Composable
private fun EditDialog(
    title: String,
    showDialog: MutableState<Boolean>,
    dialogTextFieldValue: Int,
    onValueChange: (Int) -> Unit,
) {
    val inputTextFieldValue = remember { mutableIntStateOf(dialogTextFieldValue) }
    val filter = remember(key1 = inputTextFieldValue.intValue) { FilterNumber(dialogTextFieldValue) }

    SuperDialog(
        title = title,
        show = showDialog,
        onDismissRequest = {
            showDialog.value = false
            filter.setInputValue(dialogTextFieldValue.toString())
        }
    ) {
        TextField(
            modifier = Modifier.padding(bottom = 16.dp),
            value = filter.getInputValue(),
            maxLines = 1,
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Number,
            ),
            onValueChange = filter.onValueChange()
        )
        Row(
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            TextButton(
                text = stringResource(android.R.string.cancel),
                onClick = {
                    showDialog.value = false
                    filter.setInputValue(dialogTextFieldValue.toString())
                },
                modifier = Modifier.weight(1f)
            )
            Spacer(Modifier.width(20.dp))
            TextButton(
                text = stringResource(R.string.confirm),
                onClick = {
                    showDialog.value = false
                    with(filter.getInputValue().text) {
                        if (isEmpty()) {
                            onValueChange(0)
                            filter.setInputValue("0")
                        } else {
                            onValueChange(this@with.toInt())
                        }

                    }
                },
                modifier = Modifier.weight(1f),
                colors = ButtonDefaults.textButtonColorsPrimary()
            )
        }
    }
}
