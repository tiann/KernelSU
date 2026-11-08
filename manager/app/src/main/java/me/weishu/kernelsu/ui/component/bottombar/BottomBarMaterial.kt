package me.weishu.kernelsu.ui.component.bottombar

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Surface
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.graphics.vector.rememberVectorPainter
import androidx.compose.ui.unit.dp
import me.weishu.kernelsu.ui.theme.engine.sygiDp
import me.weishu.kernelsu.ui.theme.engine.sygiIconPainter
import me.weishu.kernelsu.ui.theme.engine.sygiInt
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.union
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Extension
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material.icons.outlined.Extension
import androidx.compose.material.icons.outlined.Home
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.Shield
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Icon
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import me.weishu.kernelsu.Natives
import me.weishu.kernelsu.R
import me.weishu.kernelsu.ui.LocalMainPagerState

@Composable
fun BottomBarMaterial(navigationBadge: NavigationBadgeState) {
    val fullFeatured = Natives.isFullFeatured()
    if (!fullFeatured) return

    val mainPagerState = LocalMainPagerState.current

    // Icon ids are what theme packs override: icons/home.png, superuser.png, module.png, settings.png
    val items = listOf(
        Triple("home", R.string.home, Icons.Filled.Home),
        Triple("superuser", R.string.superuser, Icons.Filled.Shield),
        Triple("module", R.string.module, Icons.Filled.Extension),
        Triple("settings", R.string.settings, Icons.Filled.Settings)
    )

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .windowInsetsPadding(
                WindowInsets.systemBars.union(WindowInsets.displayCutout).only(
                    WindowInsetsSides.Horizontal + WindowInsetsSides.Bottom
                )
            )
            .padding(horizontal = 24.dp, vertical = 10.dp),
        contentAlignment = Alignment.Center
    ) {
        // Floating pill: corner radius, height and spacing can all be changed by a theme pack
        // (dimens: nav_corner, nav_item_size, nav_padding).
        Surface(
            shape = RoundedCornerShape(sygiDp("nav_corner", 36.dp)),
            color = MaterialTheme.colorScheme.surfaceContainerHigh,
            tonalElevation = 3.dp,
            shadowElevation = 10.dp,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        ) {
            Row(
                modifier = Modifier.padding(sygiDp("nav_padding", 6.dp)),
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                items.forEachIndexed { index, (id, label, fallback) ->
                    val selected = mainPagerState.selectedPage == index
                    val tint = if (sygiInt("icons_use_original_colors", 0) == 1) Color.Unspecified
                    else if (selected) MaterialTheme.colorScheme.onSecondaryContainer
                    else MaterialTheme.colorScheme.onSurfaceVariant
                    Box(
                        modifier = Modifier
                            .size(sygiDp("nav_item_size", 54.dp))
                            .clip(CircleShape)
                            .background(if (selected) MaterialTheme.colorScheme.secondaryContainer else Color.Transparent)
                            .clickable {
                                if (!selected) mainPagerState.animateToPage(index)
                            },
                        contentAlignment = Alignment.Center
                    ) {
                        NavigationIconWithBadge(
                            painter = sygiIconPainter(id, fallback),
                            contentDescription = stringResource(label),
                            badge = badgeFor(index, navigationBadge),
                            tint = tint,
                        )
                    }
                }
            }
        }
    }
}

@Composable
internal fun NavigationIconWithBadge(
    icon: ImageVector,
    contentDescription: String?,
    badge: NavBadge?,
) {
    NavigationIconWithBadge(
        painter = rememberVectorPainter(icon),
        contentDescription = contentDescription,
        badge = badge,
    )
}

@Composable
internal fun NavigationIconWithBadge(
    painter: Painter,
    contentDescription: String?,
    badge: NavBadge?,
    tint: Color = LocalContentColor.current,
) {
    if (badge != null) {
        BadgedBox(
            badge = {
                when (badge.tone) {
                    BadgeTone.Alert -> Badge {
                        Text(badge.count.toString())
                    }

                    BadgeTone.Accent -> Badge(
                        containerColor = MaterialTheme.colorScheme.primary,
                        contentColor = MaterialTheme.colorScheme.onPrimary,
                    ) {
                        Text(badge.count.toString())
                    }
                }
            }
        ) {
            Icon(painter, contentDescription, tint = tint)
        }
    } else {
        Icon(painter, contentDescription, tint = tint)
    }
}
