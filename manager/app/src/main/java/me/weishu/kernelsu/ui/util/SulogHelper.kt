package me.weishu.kernelsu.ui.util

import android.os.SystemClock
import androidx.annotation.StringRes
import com.topjohnwu.superuser.io.SuFile
import com.topjohnwu.superuser.io.SuFileInputStream
import java.io.File
import java.io.InputStreamReader
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.ArrayDeque
import java.util.Locale
import me.weishu.kernelsu.R

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

enum class SulogEventType(val rawType: String) {
    RootExecve("root_execve"),
    SuCompat("sucompat"),
    IoctlGrantRoot("ioctl_grant_root"),
    DaemonRestart("daemon_restart"),
    Dropped("dropped"),
    Unknown("unknown"),
}

enum class SulogEventFilter(val eventType: SulogEventType?) {
    RootExecve(SulogEventType.RootExecve),
    SuCompat(SulogEventType.SuCompat),
    IoctlGrantRoot(SulogEventType.IoctlGrantRoot),
    DaemonRestart(SulogEventType.DaemonRestart),
}

fun defaultSulogEventFilters(): Set<SulogEventFilter> = SulogEventFilter.entries.toSet()

sealed interface SulogEntry {
    val key: String
    val eventType: SulogEventType
    val rawLine: String
    val title: String?
    @get:StringRes val titleRes: Int?
    val timestampText: String?
    val summaryTags: List<String>
    val description: String?
    @get:StringRes val descriptionRes: Int?
    val status: String?
    val detailText: String

    data class RootExecve(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String,
        override val detailText: String,
        val seq: String?,
        val pid: String?,
        val uid: String?,
        val comm: String,
        val file: String,
        val argv: String,
        val retval: Int,
    ) : SulogEntry

    data class SuCompat(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String,
        override val detailText: String,
        val seq: String?,
        val pid: String?,
        val uid: String?,
        val comm: String,
        val file: String,
        val argv: String,
        val retval: Int,
    ) : SulogEntry

    data class IoctlGrantRoot(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String,
        override val detailText: String,
        val seq: String?,
        val pid: String?,
        val uid: String?,
        val comm: String,
        val retval: Int,
    ) : SulogEntry

    data class DaemonRestart(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String?,
        override val detailText: String,
        val bootId: String,
        val restart: String,
    ) : SulogEntry

    data class Dropped(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String?,
        override val detailText: String,
        val dropped: String,
        val firstSeq: String?,
        val lastSeq: String?,
    ) : SulogEntry

    data class Unknown(
        override val key: String,
        override val eventType: SulogEventType,
        override val rawLine: String,
        override val title: String?,
        override val titleRes: Int?,
        override val timestampText: String?,
        override val summaryTags: List<String>,
        override val description: String?,
        override val descriptionRes: Int?,
        override val status: String?,
        override val detailText: String,
    ) : SulogEntry
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
        val filterMatches = entry.eventType == SulogEventType.Dropped || entry.eventType in activeEventTypes
        val queryMatches = normalizedQuery.isEmpty() || entry.searchableText().contains(normalizedQuery)
        filterMatches && queryMatches
    }
}

fun parseSulogLine(
    line: String,
    currentTimeMillis: Long = System.currentTimeMillis(),
    uptimeMillis: Long = SystemClock.uptimeMillis(),
    zoneId: ZoneId = ZoneId.systemDefault(),
): SulogEntry {
    val fields = parseKeyValueLine(line)
    val timestampText = parseSulogTimestampText(
        timestampNs = fields["ts_ns"],
        currentTimeMillis = currentTimeMillis,
        uptimeMillis = uptimeMillis,
        zoneId = zoneId,
    )
    return when (fields["type"]) {
        "root_execve" -> parseRootExecve(line, fields, timestampText)
        "sucompat" -> parseSuCompat(line, fields, timestampText)
        "ioctl_grant_root" -> parseIoctlGrantRoot(line, fields, timestampText)
        "daemon_restart" -> parseDaemonRestart(line, fields, timestampText)
        "dropped" -> parseDropped(line, fields, timestampText)
        else -> SulogEntry.Unknown(
            key = fields["seq"] ?: line,
            eventType = SulogEventType.Unknown,
            rawLine = line,
            title = fields["type"]?.replace('_', ' ')?.replaceFirstChar(Char::uppercase),
            titleRes = if (fields["type"] == null) R.string.sulog_entry_unknown_event else null,
            timestampText = timestampText,
            summaryTags = listOf(line.take(120)),
            description = null,
            descriptionRes = null,
            status = null,
            detailText = line,
        )
    }
}

private fun parseRootExecve(
    line: String,
    fields: Map<String, String>,
    timestampText: String?,
): SulogEntry.RootExecve {
    val file = fields["file"].orEmpty()
    val argv = fields["argv"].orEmpty()
    val comm = fields["comm"].orEmpty()
    val pid = fields["pid"]
    val uid = fields["uid"]
    val retval = fields["retval"]?.toIntOrNull() ?: 0
    val title = File(file).name.ifBlank { comm.ifBlank { "root_execve" } }
    val detailLines = buildList {
        add("Type: root_execve")
        fields["seq"]?.let { add("Sequence: $it") }
        add("Result: ${formatStatus(retval)}")
        timestampText?.let { add("Date: $it") }
        fields["ts_ns"]?.let { add("Timestamp(ns): $it") }
        add("Command: $file")
        if (argv.isNotBlank()) add("Arguments: $argv")
        if (comm.isNotBlank()) add("Caller: $comm")
        pid?.let { add("PID: $it") }
        fields["tgid"]?.let { add("TGID: $it") }
        fields["ppid"]?.let { add("PPID: $it") }
        uid?.let { add("UID: $it") }
        fields["euid"]?.let { add("EUID: $it") }
        fields["version"]?.let { add("Version: $it") }
    }
    return SulogEntry.RootExecve(
        key = fields["seq"] ?: line,
        eventType = SulogEventType.RootExecve,
        rawLine = line,
        title = title,
        titleRes = null,
        timestampText = timestampText,
        summaryTags = listOfNotNull(comm.takeIf { it.isNotBlank() }, pid?.let { "PID $it" }, uid?.let { "UID $it" }),
        description = argv.ifBlank { file },
        descriptionRes = null,
        status = formatStatus(retval),
        detailText = detailLines.joinToString("\n"),
        seq = fields["seq"],
        pid = pid,
        uid = uid,
        comm = comm,
        file = file,
        argv = argv,
        retval = retval,
    )
}

private fun parseSuCompat(
    line: String,
    fields: Map<String, String>,
    timestampText: String?,
): SulogEntry.SuCompat {
    val file = fields["file"].orEmpty()
    val argv = fields["argv"].orEmpty()
    val comm = fields["comm"].orEmpty()
    val pid = fields["pid"]
    val uid = fields["uid"]
    val retval = fields["retval"]?.toIntOrNull() ?: 0
    val detailLines = buildList {
        add("Type: sucompat")
        fields["seq"]?.let { add("Sequence: $it") }
        add("Result: ${formatStatus(retval)}")
        timestampText?.let { add("Date: $it") }
        fields["ts_ns"]?.let { add("Timestamp(ns): $it") }
        add("Command: $file")
        if (argv.isNotBlank()) add("Arguments: $argv")
        if (comm.isNotBlank()) add("Caller: $comm")
        pid?.let { add("PID: $it") }
        fields["tgid"]?.let { add("TGID: $it") }
        fields["ppid"]?.let { add("PPID: $it") }
        uid?.let { add("UID: $it") }
        fields["euid"]?.let { add("EUID: $it") }
        fields["version"]?.let { add("Version: $it") }
    }
    return SulogEntry.SuCompat(
        key = fields["seq"] ?: line,
        eventType = SulogEventType.SuCompat,
        rawLine = line,
        title = null,
        titleRes = R.string.settings_sucompat,
        timestampText = timestampText,
        summaryTags = listOfNotNull(comm.takeIf { it.isNotBlank() }, pid?.let { "PID $it" }, uid?.let { "UID $it" }),
        description = null,
        descriptionRes = R.string.sulog_entry_description_sucompat,
        status = formatStatus(retval),
        detailText = detailLines.joinToString("\n"),
        seq = fields["seq"],
        pid = pid,
        uid = uid,
        comm = comm,
        file = file,
        argv = argv,
        retval = retval,
    )
}

private fun parseIoctlGrantRoot(
    line: String,
    fields: Map<String, String>,
    timestampText: String?,
): SulogEntry.IoctlGrantRoot {
    val comm = fields["comm"].orEmpty()
    val pid = fields["pid"]
    val uid = fields["uid"]
    val retval = fields["retval"]?.toIntOrNull() ?: 0
    val detailLines = buildList {
        add("Type: ioctl_grant_root")
        fields["seq"]?.let { add("Sequence: $it") }
        add("Result: ${formatStatus(retval)}")
        timestampText?.let { add("Date: $it") }
        fields["ts_ns"]?.let { add("Timestamp(ns): $it") }
        if (comm.isNotBlank()) add("Caller: $comm")
        pid?.let { add("PID: $it") }
        fields["tgid"]?.let { add("TGID: $it") }
        fields["ppid"]?.let { add("PPID: $it") }
        uid?.let { add("UID: $it") }
        fields["euid"]?.let { add("EUID: $it") }
        fields["version"]?.let { add("Version: $it") }
    }
    return SulogEntry.IoctlGrantRoot(
        key = fields["seq"] ?: line,
        eventType = SulogEventType.IoctlGrantRoot,
        rawLine = line,
        title = null,
        titleRes = R.string.sulog_entry_title_root_grant_request,
        timestampText = timestampText,
        summaryTags = listOfNotNull(comm.takeIf { it.isNotBlank() }, pid?.let { "PID $it" }, uid?.let { "UID $it" }),
        description = null,
        descriptionRes = R.string.sulog_entry_description_ioctl_grant_root,
        status = formatStatus(retval),
        detailText = detailLines.joinToString("\n"),
        seq = fields["seq"],
        pid = pid,
        uid = uid,
        comm = comm,
        retval = retval,
    )
}

private fun parseDaemonRestart(
    line: String,
    fields: Map<String, String>,
    timestampText: String?,
): SulogEntry.DaemonRestart {
    val restart = fields["restart"].orEmpty()
    val bootId = fields["boot_id"].orEmpty()
    val detailLines = buildList {
        add("Type: daemon_restart")
        timestampText?.let { add("Date: $it") }
        if (restart.isNotBlank()) add("Restart: #$restart")
        if (bootId.isNotBlank()) add("Boot ID: $bootId")
    }
    return SulogEntry.DaemonRestart(
        key = "daemon_restart_${restart}_${bootId}",
        eventType = SulogEventType.DaemonRestart,
        rawLine = line,
        title = null,
        titleRes = R.string.sulog_entry_title_daemon_restarted,
        timestampText = timestampText,
        summaryTags = if (restart.isNotBlank()) listOf("Restart #$restart") else listOf("Daemon restarted"),
        description = bootId.takeIf { it.isNotBlank() }?.let { "Boot ${it.take(8)}…" },
        descriptionRes = null,
        status = null,
        detailText = detailLines.joinToString("\n"),
        bootId = bootId,
        restart = restart,
    )
}

private fun parseDropped(
    line: String,
    fields: Map<String, String>,
    timestampText: String?,
): SulogEntry.Dropped {
    val dropped = fields["dropped"].orEmpty()
    val firstSeq = fields["first_seq"]
    val lastSeq = fields["last_seq"]
    val detailLines = buildList {
        add("Type: dropped")
        timestampText?.let { add("Date: $it") }
        if (dropped.isNotBlank()) add("Dropped events: $dropped")
        firstSeq?.let { add("First sequence: $it") }
        lastSeq?.let { add("Last sequence: $it") }
        fields["ts_ns"]?.let { add("Timestamp(ns): $it") }
    }
    return SulogEntry.Dropped(
        key = fields["seq"] ?: line,
        eventType = SulogEventType.Dropped,
        rawLine = line,
        title = null,
        titleRes = R.string.sulog_entry_title_dropped_events,
        timestampText = timestampText,
        summaryTags = listOfNotNull(dropped.takeIf { it.isNotBlank() }?.let { "$it lost" }),
        description = if (dropped.isNotBlank()) "$dropped sulog events were dropped" else "Sulog events were dropped",
        descriptionRes = null,
        status = null,
        detailText = detailLines.joinToString("\n"),
        dropped = dropped,
        firstSeq = firstSeq,
        lastSeq = lastSeq,
    )
}

private fun SulogEntry.searchableText(): String {
    return buildString {
        append(title)
        append('\n')
        timestampText?.let {
            append(it)
            append('\n')
        }
        append(summaryTags.joinToString(" "))
        append('\n')
        description?.let {
            append(it)
            append('\n')
        }
        append(detailText)
        append('\n')
        append(rawLine)
    }.lowercase()
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

private fun formatStatus(ret: Int): String {
    return if (ret == 0) "Success" else "Exit $ret"
}

internal fun parseSulogTimestampText(
    timestampNs: String?,
    currentTimeMillis: Long = System.currentTimeMillis(),
    uptimeMillis: Long = SystemClock.uptimeMillis(),
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
