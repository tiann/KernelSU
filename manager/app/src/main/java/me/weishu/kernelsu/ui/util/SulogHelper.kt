package me.weishu.kernelsu.ui.util

import android.os.SystemClock
import com.topjohnwu.superuser.io.SuFile
import com.topjohnwu.superuser.io.SuFileInputStream
import java.io.InputStreamReader
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.ArrayDeque
import java.util.Locale

private const val SULOG_DIR = "/data/adb/ksu/log"
private const val SULOG_LINE_LIMIT = 1000
private const val SULOG_FILE_PREFIX = "sulog-"
private const val SULOG_FILE_SUFFIX = ".log"
private val SULOG_FILE_NAME_REGEX = Regex("""$SULOG_FILE_PREFIX(\d{4}-\d{2}-\d{2})(?:-(\d+))?$SULOG_FILE_SUFFIX""")
private const val NS_PER_MILLISECOND = 1_000_000L
private val SULOG_TIMESTAMP_FORMATTER: DateTimeFormatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss", Locale.US)

data class SulogFile(
    val name: String,
    val path: String,
)

enum class SulogFileCleanAction {
    Clear,
    Delete,
}

enum class SulogEventType {
    RootExecve,
    SuCompat,
    IoctlGrantRoot,
    DaemonEvent,
    Dropped,
    Unknown,
}

enum class SulogEventFilter(val eventType: SulogEventType?) {
    RootExecve(SulogEventType.RootExecve),
    SuCompat(SulogEventType.SuCompat),
    IoctlGrantRoot(SulogEventType.IoctlGrantRoot),
    DaemonEvent(SulogEventType.DaemonEvent),
}

fun defaultSulogEventFilters(): Set<SulogEventFilter> = SulogEventFilter.entries.toSet()

data class SulogEntry(
    val key: String,
    val eventType: SulogEventType,
    val rawLine: String,
    val timestampText: String?,
    val fields: Map<String, String>,
) {
    val searchableText: String by lazy {
        buildString {
            append(rawLine)
            append('\n')
            timestampText?.let {
                append(it)
                append('\n')
            }
            fields.values.forEach {
                append(it)
                append(' ')
            }
        }.lowercase()
    }
}

internal fun parseSulogFileNames(fileNames: List<String>): List<String> {
    return fileNames
        .mapNotNull { name ->
            val match = SULOG_FILE_NAME_REGEX.matchEntire(name) ?: return@mapNotNull null
            val date = LocalDate.parse(match.groupValues[1])
            val rotation = match.groupValues[2].takeIf { it.isNotEmpty() }?.toInt() ?: 0
            Triple(name, date, rotation)
        }
        .sortedWith(compareByDescending<Triple<String, LocalDate, Int>> { it.second }.thenByDescending { it.third })
        .map { it.first }
}

internal fun resolveSulogFileCleanAction(
    files: List<SulogFile>,
    selectedFilePath: String,
): SulogFileCleanAction {
    val latestFilePath = files.firstOrNull()?.path
    return if (selectedFilePath == latestFilePath) {
        SulogFileCleanAction.Clear
    } else {
        SulogFileCleanAction.Delete
    }
}

fun listSulogFiles(): List<SulogFile> {
    val directory = SuFile(SULOG_DIR)
    val fileNames = directory.list().orEmpty().toList()
    return parseSulogFileNames(fileNames).map { name ->
        SulogFile(name = name, path = "$SULOG_DIR/$name")
    }
}

fun readSulogFile(path: String): List<String> {
    val suFile = SuFile(path)
    if (!suFile.isFile) {
        return emptyList()
    }

    val lines = ArrayDeque<String>(SULOG_LINE_LIMIT)
    SuFileInputStream.open(suFile).use { input ->
        InputStreamReader(input).buffered().useLines { sequence ->
            sequence.forEach { line ->
                if (lines.size == SULOG_LINE_LIMIT) {
                    lines.removeFirst()
                }
                lines.addLast(line)
            }
        }
    }
    return lines.toList()
}

fun parseSulogLines(lines: List<String>): List<SulogEntry> {
    val currentTimeMillis = System.currentTimeMillis()
    val uptimeMillis = SystemClock.uptimeMillis()
    val zoneId = ZoneId.systemDefault()
    return lines.mapNotNull { line ->
        line.takeIf { it.isNotBlank() }?.let {
            parseSulogLine(
                line = it,
                currentTimeMillis = currentTimeMillis,
                uptimeMillis = uptimeMillis,
                zoneId = zoneId,
            )
        }
    }
}

fun buildVisibleSulogEntries(
    entries: List<SulogEntry>,
    searchText: String,
    selectedFilters: Set<SulogEventFilter>,
): List<SulogEntry> {
    val normalizedQuery = searchText.trim().lowercase()
    val activeEventTypes = selectedFilters.mapNotNull { it.eventType }.toSet()
    return entries.asReversed().filter { entry ->
        val alwaysVisible = entry.eventType == SulogEventType.Dropped || entry.eventType == SulogEventType.Unknown
        val filterMatches = alwaysVisible || entry.eventType in activeEventTypes
        val queryMatches = normalizedQuery.isEmpty() || entry.searchableText.contains(normalizedQuery)
        filterMatches && queryMatches
    }
}

fun parseSulogLine(
    line: String,
    currentTimeMillis: Long = System.currentTimeMillis(),
    uptimeMillis: Long = -1L,
    zoneId: ZoneId = ZoneId.systemDefault(),
): SulogEntry {
    val fields = parseKeyValueLine(line)
    val eventType = when (fields["type"]) {
        "root_execve" -> SulogEventType.RootExecve
        "sucompat" -> SulogEventType.SuCompat
        "ioctl_grant_root" -> SulogEventType.IoctlGrantRoot
        "daemon_restart", "daemon_start" -> SulogEventType.DaemonEvent
        "dropped" -> SulogEventType.Dropped
        else -> SulogEventType.Unknown
    }
    val key = when (eventType) {
        SulogEventType.DaemonEvent -> "daemon_restart_${fields["restart"]}_${fields["boot_id"]}"
        else -> fields["seq"] ?: line
    }
    val timestampText = parseSulogTimestampText(
        timestampNs = fields["ts_ns"],
        currentTimeMillis = currentTimeMillis,
        uptimeMillis = uptimeMillis,
        zoneId = zoneId,
    )
    return SulogEntry(
        key = key,
        eventType = eventType,
        rawLine = line,
        timestampText = timestampText,
        fields = fields,
    )
}

private fun parseKeyValueLine(line: String): Map<String, String> {
    return buildMap {
        var index = 0
        while (index < line.length) {
            while (index < line.length && line[index].isWhitespace()) {
                index++
            }
            if (index >= line.length) break

            val keyStart = index
            while (index < line.length && line[index] != '=' && !line[index].isWhitespace()) {
                index++
            }
            if (index >= line.length || line[index] != '=') {
                while (index < line.length && !line[index].isWhitespace()) {
                    index++
                }
                continue
            }

            val key = line.substring(keyStart, index)
            index++
            val (value, nextIndex) = if (index < line.length && line[index] == '"') {
                parseQuotedSulogValue(line, index + 1)
            } else {
                parseUnquotedSulogValue(line, index)
            }
            if (key.isNotEmpty()) {
                put(key, value)
            }
            index = nextIndex
        }
    }
}

private fun parseQuotedSulogValue(line: String, startIndex: Int): Pair<String, Int> {
    val value = StringBuilder()
    var index = startIndex
    while (index < line.length) {
        when (val ch = line[index]) {
            '"' -> return value.toString() to (index + 1)
            '\\' -> index = appendEscapedSulogChar(line, index, value)
            else -> {
                value.append(ch)
                index++
            }
        }
    }
    return value.toString() to index
}

private fun parseUnquotedSulogValue(line: String, startIndex: Int): Pair<String, Int> {
    var index = startIndex
    while (index < line.length && !line[index].isWhitespace()) {
        index++
    }
    return line.substring(startIndex, index) to index
}

private fun appendEscapedSulogChar(line: String, slashIndex: Int, value: StringBuilder): Int {
    val nextIndex = slashIndex + 1
    if (nextIndex >= line.length) {
        value.append('\\')
        return nextIndex
    }

    return when (val escaped = line[nextIndex]) {
        '\\' -> {
            value.append('\\')
            nextIndex + 1
        }
        '"' -> {
            value.append('"')
            nextIndex + 1
        }
        'n' -> {
            value.append('\n')
            nextIndex + 1
        }
        'r' -> {
            value.append('\r')
            nextIndex + 1
        }
        't' -> {
            value.append('\t')
            nextIndex + 1
        }
        'x' -> appendHexEscapedSulogChar(line, nextIndex, value)
        else -> {
            value.append(escaped)
            nextIndex + 1
        }
    }
}

private fun appendHexEscapedSulogChar(line: String, xIndex: Int, value: StringBuilder): Int {
    val hexStart = xIndex + 1
    val hexEnd = hexStart + 2
    if (hexEnd <= line.length) {
        val code = line.substring(hexStart, hexEnd).toIntOrNull(16)
        if (code != null) {
            value.append(code.toChar())
            return hexEnd
        }
    }

    value.append('x')
    return xIndex + 1
}

internal fun parseSulogTimestampText(
    timestampNs: String?,
    currentTimeMillis: Long = System.currentTimeMillis(),
    uptimeMillis: Long = -1L,
    zoneId: ZoneId = ZoneId.systemDefault(),
): String? {
    val timestampNanos = timestampNs?.toLongOrNull() ?: return null
    if (timestampNanos < 0 || uptimeMillis < 0) return null

    val bootTimeMillis = currentTimeMillis - uptimeMillis
    if (bootTimeMillis < 0) return null

    val eventTimeMillis = bootTimeMillis + (timestampNanos / NS_PER_MILLISECOND)
    if (eventTimeMillis < 0) return null

    return Instant.ofEpochMilli(eventTimeMillis)
        .atZone(zoneId)
        .format(SULOG_TIMESTAMP_FORMATTER)
}

internal fun String.toSulogDisplayName(): String {
    return if (startsWith(SULOG_FILE_PREFIX) && endsWith(SULOG_FILE_SUFFIX)) {
        removePrefix(SULOG_FILE_PREFIX).removeSuffix(SULOG_FILE_SUFFIX)
    } else {
        this
    }
}

fun cleanSulogFile(path: String): Boolean {
    val suFile = SuFile(path)
    return suFile.clear()
}

fun deleteSulogFile(path: String): Boolean {
    val suFile = SuFile(path)
    return suFile.delete()
}
