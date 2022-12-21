/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

%{
	#include <stdint.h>
	#include <sepol/errcodes.h>
	#include "cil_internal.h"
	#include "cil_lexer.h"
	#include "cil_log.h"
	#include "cil_mem.h"
	char *value =  NULL;
	int line = 1;
%}

%option nounput
%option noinput
%option noyywrap
%option prefix="cil_yy"

digit		[0-9]
alpha		[a-zA-Z]
spec_char	[\[\]\.\@\=\/\*\-\_\$\%\+\-\!\|\&\^\:\~\`\#\{\}\'\<\>\?\,]
symbol		({digit}|{alpha}|{spec_char})+
white		[ \t]
newline		[\n\r]
qstring		\"[^"\n\0]*\"
hll_lm          ^;;\*
comment		;

%%
{newline}	line++; return NEWLINE;
{hll_lm}	value=yytext; return HLL_LINEMARK;
{comment}	value=yytext; return COMMENT;
"("		value=yytext; return OPAREN;
")"		value=yytext; return CPAREN;
{symbol}	value=yytext; return SYMBOL;
{white}		;
{qstring}	value=yytext; return QSTRING;
<<EOF>>		return END_OF_FILE;
.		value=yytext; return UNKNOWN;
%%

int cil_lexer_setup(char *buffer, uint32_t size)
{
	size = (yy_size_t)size;
	if (yy_scan_buffer(buffer, size) == NULL) {
		cil_log(CIL_INFO, "Lexer failed to setup buffer\n");
		return SEPOL_ERR;
	}

	line = 1;

	return SEPOL_OK;
}

void cil_lexer_destroy(void)
{
	yylex_destroy();
}

int cil_lexer_next(struct token *tok)
{
	tok->type = yylex();
	tok->value = value;
	tok->line = line;

	return SEPOL_OK;
}
