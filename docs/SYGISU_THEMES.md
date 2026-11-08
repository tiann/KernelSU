# SygiSU theme packs

A theme pack is a `.zip` of plain data files. Import it in **Settings -> Themes**.

```
MyTheme.zip
  theme.xml              optional: <theme name="My Theme" author="you" version="1.0"/>
  theme/
    colors_light.xml     colors used in light mode
    colors_dark.xml      colors used in dark mode
    colors_amoled.xml    extra overrides for the AMOLED black mode (applied after dark)
    colors_anything.xml  no suffix = applies to every mode (any number of extra files)
    dimens.xml           spacing / sizes (see below)
    shapes.xml           corner radii
    typography.xml       fonts and text styles
  icons/                 home.png, superuser.png, module.png, settings.png (png/webp/jpg)
  fonts/                 .ttf / .otf
  etc/                   anything else (images, notes); you may add more folders
```

A `_light`, `_dark` or `_amoled` file-name suffix limits a file to that mode. Later files (alphabetical) win.

## colors (`<color name="...">#RRGGBB</color>`)

Names are the Material 3 color roles: `primary, onPrimary, primaryContainer, onPrimaryContainer,
secondary, onSecondary, secondaryContainer, onSecondaryContainer, tertiary, onTertiary,
tertiaryContainer, onTertiaryContainer, background, onBackground, surface, onSurface,
surfaceVariant, onSurfaceVariant, surfaceTint, surfaceBright, surfaceDim, surfaceContainer,
surfaceContainerLow, surfaceContainerLowest, surfaceContainerHigh, surfaceContainerHighest,
inverseSurface, inverseOnSurface, inversePrimary, error, onError, errorContainer,
onErrorContainer, outline, outlineVariant, scrim`.

If a foreground/background pair would be hard to read (contrast below 3:1), that pair is ignored and
the default colors are used, so a theme cannot hide text.

## dimens (`<dimen name="...">28dp</dimen>`)

| name | what it changes | default |
|---|---|---|
| `nav_corner` | corner radius of the floating navigation pill | 36 |
| `nav_item_size` | size of each navigation button | 54 |
| `nav_padding` | padding inside the pill | 6 |

Integers (`<integer name="...">1</integer>`): `icons_use_original_colors` = 1 keeps your icon colors
instead of tinting them.

## shapes (`<corner name="small" dp="14"/>`)

`extraSmall, small, medium, large, extraLarge` (default 8, 12, 18, 26, 34).

## typography

```xml
<font name="default" file="fonts/MyFont.ttf"/>
<font name="display" file="fonts/Other.otf"/>
<text role="titleLarge" font="display" size="24" weight="700"/>
```
`font name="default"` applies to every text style. Roles are the Material 3 roles
(`displayLarge` ... `labelSmall`).

## icons

`icons/<id>.png`. Ids: `home`, `superuser`, `module`, `settings`.

## Safety

Packs are data only. Import rejects anything that is not an image, font, XML or text file, unsafe
paths, XML with DOCTYPE/ENTITY, and oversized packs. Warning dialogs and root-grant prompts are not
themable beyond colors. If a theme crashes the app twice in a row, SygiSU reverts to the default theme.

See `themes/examples/SygiSU-Midnight.zip` for a working example.
