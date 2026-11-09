package me.weishu.kernelsu.ui.util

import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.font.FontStyle

enum class ShellTokenKind { COMMENT, STRING, VARIABLE, KEYWORD }

data class ShellToken(val kind: ShellTokenKind, val start: Int, val end: Int)

@Immutable
data class ShellHighlightColors(
    val comment: Color,
    val string: Color,
    val variable: Color,
    val keyword: Color,
) {
    companion object {
        private val light = ShellHighlightColors(
            comment = Color(0xFF6A737D),
            string = Color(0xFF032F62),
            variable = Color(0xFFE36209),
            keyword = Color(0xFFD73A49),
        )
        private val dark = ShellHighlightColors(
            comment = Color(0xFF8B949E),
            string = Color(0xFFA5D6FF),
            variable = Color(0xFFFFA657),
            keyword = Color(0xFFFF7B72),
        )

        /** Fixed code colors that stay readable on [background], independent of the app accent. */
        fun forBackground(background: Color): ShellHighlightColors =
            if (background.luminance() < 0.5f) dark else light
    }
}

private val shellKeywords = setOf(
    "if", "then", "else", "elif", "fi", "for", "while", "until", "do", "done",
    "case", "esac", "select", "function", "in", "!", "{", "}",
)

/** Keywords after which the next word is a command again. */
private val commandPrefixKeywords = setOf("if", "then", "else", "elif", "do", "while", "until", "!", "{")

private fun isNameStart(c: Char) = c == '_' || c in 'a'..'z' || c in 'A'..'Z'
private fun isNameChar(c: Char) = isNameStart(c) || c in '0'..'9'

/**
 * Lexes POSIX shell source just enough for highlighting: comments, quoted strings, parameter
 * expansions (`$x`, `${x}`, `$(`, `$1`, `$?`, ...), reserved words in command position and the
 * name of `NAME=value` assignments. Tokens are non-overlapping and in source order; variables
 * inside double quotes are emitted between the string pieces around them.
 */
fun shellTokens(src: String): List<ShellToken> {
    val tokens = mutableListOf<ShellToken>()
    var i = 0
    val n = src.length
    var commandPosition = true
    // Previous reserved word on this command, to recognise `in` of for / case / select.
    var pendingIn = false
    var wordsSinceFor = 0

    fun variableEnd(from: Int): Int {
        // src[from] == '$'
        val next = from + 1
        if (next >= n) return next
        val c = src[next]
        return when {
            c == '{' -> {
                val close = src.indexOf('}', next + 1)
                if (close < 0) n else close + 1
            }

            c == '(' -> next + 1
            isNameStart(c) -> {
                var j = next + 1
                while (j < n && isNameChar(src[j])) j++
                j
            }

            c in '0'..'9' || c in "?@#$!*-" -> next + 1
            else -> from
        }
    }

    while (i < n) {
        val c = src[i]
        when {
            c == '\n' -> {
                commandPosition = true
                pendingIn = false
                i++
            }

            c == ' ' || c == '\t' -> i++

            c == '\\' -> {
                i += 2
                commandPosition = false
            }

            c == '#' -> {
                val end = src.indexOf('\n', i).let { if (it < 0) n else it }
                tokens += ShellToken(ShellTokenKind.COMMENT, i, end)
                i = end
            }

            c == ';' || c == '&' || c == '|' || c == '(' || c == ')' -> {
                commandPosition = true
                pendingIn = false
                i++
            }

            c == '\'' -> {
                val close = src.indexOf('\'', i + 1)
                val end = if (close < 0) n else close + 1
                tokens += ShellToken(ShellTokenKind.STRING, i, end)
                i = end
                commandPosition = false
                if (pendingIn) wordsSinceFor++
            }

            c == '"' -> {
                var segmentStart = i
                var j = i + 1
                while (j < n && src[j] != '"') {
                    when {
                        src[j] == '\\' -> j += 2
                        src[j] == '$' -> {
                            val end = variableEnd(j)
                            if (end > j) {
                                if (j > segmentStart) tokens += ShellToken(ShellTokenKind.STRING, segmentStart, j)
                                tokens += ShellToken(ShellTokenKind.VARIABLE, j, end)
                                segmentStart = end
                                j = end
                            } else {
                                j++
                            }
                        }

                        else -> j++
                    }
                }
                val end = minOf(j + 1, n)
                if (end > segmentStart) tokens += ShellToken(ShellTokenKind.STRING, segmentStart, end)
                i = end
                commandPosition = false
                if (pendingIn) wordsSinceFor++
            }

            c == '$' -> {
                val end = variableEnd(i)
                if (end > i) {
                    tokens += ShellToken(ShellTokenKind.VARIABLE, i, end)
                    // `$(` opens a command substitution.
                    commandPosition = src[end - 1] == '('
                    if (pendingIn) wordsSinceFor++
                    i = end
                } else {
                    i++
                    commandPosition = false
                }
            }

            else -> {
                var j = i
                while (j < n && src[j] !in " \t\n;&|()'\"\\$") j++
                if (j == i) j = i + 1
                val word = src.substring(i, j)
                if (commandPosition && word in shellKeywords) {
                    tokens += ShellToken(ShellTokenKind.KEYWORD, i, j)
                    commandPosition = word in commandPrefixKeywords
                    pendingIn = word == "for" || word == "case" || word == "select"
                    wordsSinceFor = 0
                } else if (pendingIn && word == "in" && wordsSinceFor == 1) {
                    tokens += ShellToken(ShellTokenKind.KEYWORD, i, j)
                    pendingIn = false
                    commandPosition = false
                } else {
                    val eq = word.indexOf('=')
                    if (commandPosition && eq > 0 && isNameStart(word[0]) && (1 until eq).all { isNameChar(word[it]) }) {
                        // NAME=value keeps command position: `A=1 B=2 cmd`.
                        tokens += ShellToken(ShellTokenKind.VARIABLE, i, i + eq)
                    } else {
                        commandPosition = false
                    }
                    if (pendingIn) wordsSinceFor++
                }
                i = j
            }
        }
    }
    return tokens
}

fun highlightShell(src: String, colors: ShellHighlightColors): AnnotatedString {
    val builder = AnnotatedString.Builder(src)
    for (token in shellTokens(src)) {
        val style = when (token.kind) {
            ShellTokenKind.COMMENT -> SpanStyle(color = colors.comment, fontStyle = FontStyle.Italic)
            ShellTokenKind.STRING -> SpanStyle(color = colors.string)
            ShellTokenKind.VARIABLE -> SpanStyle(color = colors.variable)
            ShellTokenKind.KEYWORD -> SpanStyle(color = colors.keyword)
        }
        builder.addStyle(style, token.start, token.end)
    }
    return builder.toAnnotatedString()
}

@Composable
fun rememberShellHighlight(src: String, colors: ShellHighlightColors): AnnotatedString =
    remember(src, colors) { highlightShell(src, colors) }
