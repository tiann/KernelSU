import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.drawable.Drawable
import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Checkbox
import androidx.compose.material.Switch
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import me.weishu.kernelsu.Natives
import java.util.*

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
    Row(modifier = Modifier
        .fillMaxWidth()
        .clickable {
            onItemClick()
        }) {

        Image(
            painter = rememberDrawablePainter(drawable = superUserData.icon),
            contentDescription = superUserData.name.toString(),
            modifier = Modifier
                .padding(4.dp)
                .width(48.dp)
                .height(48.dp)
        )

        Column {
            Text(
                superUserData.name.toString(),
                modifier = Modifier.padding(4.dp),
                color = Color.Black,
                fontSize = 16.sp
            )
            Text(
                superUserData.description,
                modifier = Modifier.padding(4.dp),
                color = Color.Gray,
                fontSize = 12.sp
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            modifier = Modifier.padding(4.dp)
        )
    }
}

private fun getAppList(context: Context): List<SuperUserData> {
    val pm = context.packageManager
    val allowList = Natives.getAllowList()
    val denyList = Natives.getDenyList();

    Log.i("mylog", "allowList: ${Arrays.toString(allowList)}")
    Log.i("mylog", "denyList: ${Arrays.toString(denyList)}")

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

    // add deny list
    for (uid in denyList) {
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
        Text("No apps found")
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