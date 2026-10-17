package me.weishu.kernelsu.ui.util

import me.weishu.kernelsu.ui.util.ShellTokenKind.COMMENT
import me.weishu.kernelsu.ui.util.ShellTokenKind.KEYWORD
import me.weishu.kernelsu.ui.util.ShellTokenKind.STRING
import me.weishu.kernelsu.ui.util.ShellTokenKind.VARIABLE
import org.junit.Assert.assertEquals
import org.junit.Test

class ShellHighlightTest {

    private fun lex(src: String): List<Pair<ShellTokenKind, String>> =
        shellTokens(src).map { it.kind to src.substring(it.start, it.end) }

    @Test
    fun commentsOnlyStartAWord() {
        assertEquals(
            listOf(COMMENT to "#!/system/bin/sh", COMMENT to "# note"),
            lex("#!/system/bin/sh\necho a#b # note"),
        )
        assertEquals(listOf(VARIABLE to "$#"), lex("echo $#"))
    }

    @Test
    fun stringsAndExpansions() {
        assertEquals(listOf(STRING to "'a \$x'"), lex("echo 'a \$x'"))
        assertEquals(
            listOf(STRING to "\"a ", VARIABLE to "\${B:-c}", STRING to " \\\"", VARIABLE to "\$1", STRING to "\""),
            lex("echo \"a \${B:-c} \\\"\$1\""),
        )
        assertEquals(
            listOf(VARIABLE to "\$(", KEYWORD to "if"),
            lex("x \$(if"),
        )
        assertEquals(listOf(VARIABLE to "\$HOME", VARIABLE to "\$?"), lex("cd \$HOME; echo \$?"))
    }

    @Test
    fun keywordsOnlyInCommandPosition() {
        assertEquals(
            listOf(KEYWORD to "if", KEYWORD to "then", KEYWORD to "fi"),
            lex("if [ -f x ]; then echo done if; fi"),
        )
        assertEquals(
            listOf(KEYWORD to "for", KEYWORD to "in", KEYWORD to "do", VARIABLE to "\$f", KEYWORD to "done"),
            lex("for f in a b; do echo \$f; done"),
        )
        assertEquals(
            listOf(KEYWORD to "case", STRING to "\"", VARIABLE to "\$x", STRING to "\"", KEYWORD to "in", KEYWORD to "esac"),
            lex("case \"\$x\" in a) echo in ;; esac"),
        )
    }

    @Test
    fun assignmentsInCommandPosition() {
        assertEquals(
            listOf(VARIABLE to "A", VARIABLE to "B", VARIABLE to "\$A"),
            lex("A=1 B=x echo C=\$A"),
        )
    }
}
