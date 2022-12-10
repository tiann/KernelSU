@file:OptIn(ExperimentalMaterial3Api::class)

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.drawable.Drawable
import android.os.Build
import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import me.weishu.kernelsu.Natives
import java.util.*

private const val TAG = "SuperUser"

class SuperUserData(
    val name: CharSequence,
    val description: String,
    val icon: Drawable,
    val uid: Int,
    initialChecked: Boolean = false
) {
    var checked: Boolean by mutableStateOf(initialChecked)
}

@Composable
fun SuperUserItem(
    superUserData: SuperUserData,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    onItemClick: () -> Unit
) {

    Column {
        ListItem(
            headlineText = { Text(superUserData.name.toString()) },
            supportingText = { Text(superUserData.description) },
            leadingContent = {
                Image(
                    painter = rememberDrawablePainter(drawable = superUserData.icon),
                    contentDescription = superUserData.name.toString(),
                    modifier = Modifier
                        .padding(4.dp)
                        .width(48.dp)
                        .height(48.dp)
                )
            },
            trailingContent = {
                Switch(
                    checked = checked,
                    onCheckedChange = onCheckedChange,
                    modifier = Modifier.padding(4.dp)
                )
            }
        )
        Divider(thickness = Dp.Hairline)
    }
}

private fun getAppList(context: Context): List<SuperUserData> {
    val pm = context.packageManager
    val allowList = Natives.getAllowList()
    val denyList = Natives.getDenyList();

    Log.i(TAG, "allowList: ${Arrays.toString(allowList)}")
    Log.i(TAG, "denyList: ${Arrays.toString(denyList)}")

    val result = mutableListOf<SuperUserData>()

    // add allow list
    for (uid in allowList) {
        val packagesForUid = pm.getPackagesForUid(uid)
        if (packagesForUid == null || packagesForUid.isEmpty()) {
            continue
        }

        packagesForUid.forEach { packageName ->
            val applicationInfo = pm.getApplicationInfo(packageName, 0)
            result.add(
                SuperUserData(
                    name = applicationInfo.loadLabel(pm),
                    description = applicationInfo.packageName,
                    icon = applicationInfo.loadIcon(pm),
                    uid = uid,
                    initialChecked = true
                )
            )
        }
    }

    val defaultDenyList = denyList.toMutableList()

    val shellUid = if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) android.os.Process.SHELL_UID else 2000;
    if (!allowList.contains(shellUid) && !denyList.contains(shellUid)) {
        // shell uid is not in allow list, add it to default deny list
        defaultDenyList.add(shellUid)
    }

    // add deny list
    for (uid in defaultDenyList) {
        val packagesForUid = pm.getPackagesForUid(uid)
        if (packagesForUid == null || packagesForUid.isEmpty()) {
            continue
        }

        packagesForUid.forEach { packageName ->
            val applicationInfo = pm.getApplicationInfo(packageName, 0)
            result.add(
                SuperUserData(
                    name = applicationInfo.loadLabel(pm),
                    description = applicationInfo.packageName,
                    icon = applicationInfo.loadIcon(pm),
                    uid = uid,
                    initialChecked = false
                )
            )
        }
    }

    return result
}

@SuppressLint("QueryPermissionsNeeded")
@Composable
fun SuperUser() {

    val context = LocalContext.current

    val list = getAppList(context)
    val apps = remember { list.toMutableStateList() }

    if (apps.isEmpty()) {
        Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text("No apps request superuser")
        }
        return
    }

    LazyColumn() {
        items(apps, key = { it.description }) { app ->
            SuperUserItem(
                superUserData = app,
                checked = app.checked,
                onCheckedChange = { checked ->
                    val success = Natives.allowRoot(app.uid, checked)
                    if (success) {
                        app.checked = checked
                    } else {
                        Toast.makeText(
                            context,
                            "Failed to allow root: ${app.uid}",
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                },
                onItemClick = {
                    // TODO
                }
            )
        }
    }
}

@Preview
@Composable
fun Preview_SuperUser() {
    SuperUser()
}