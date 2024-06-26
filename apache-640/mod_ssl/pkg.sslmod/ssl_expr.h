/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_expr.h
**  Expression Handling (Header)
*/

/* ====================================================================
 * Copyright (c) 1998-2000 Ralf S. Engelschall. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.modssl.org/)."
 *
 * 4. The names "mod_ssl" must not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission. For written permission, please contact
 *    rse@engelschall.com.
 *
 * 5. Products derived from this software may not be called "mod_ssl"
 *    nor may "mod_ssl" appear in their names without prior
 *    written permission of Ralf S. Engelschall.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.modssl.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY RALF S. ENGELSCHALL ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL RALF S. ENGELSCHALL OR
 * HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

                             /* ``May all your PUSHes be POPed.'' */

#ifndef SSL_EXPR_H
#define SSL_EXPR_H

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE !FALSE
#endif

#ifndef YY_NULL
#define YY_NULL 0
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef BOOL
#define BOOL unsigned int
#endif

#ifndef NULL
#define NULL (void *)0
#endif

#ifndef NUL
#define NUL '\0'
#endif

#ifndef YYDEBUG
#define YYDEBUG 0
#endif

typedef enum {
    op_NOP, op_ListElement,
    op_True, op_False, op_Not, op_Or, op_And, op_Comp,
    op_EQ, op_NE, op_LT, op_LE, op_GT, op_GE, op_IN, op_REG, op_NRE,
    op_Digit, op_String, op_Regex, op_Var, op_Func
} ssl_expr_node_op;

typedef struct {
    ssl_expr_node_op node_op;
    void *node_arg1;
    void *node_arg2;
} ssl_expr_node;

typedef ssl_expr_node ssl_expr;

typedef struct {
	pool     *pool;
    char     *inputbuf;
    int       inputlen;
    char     *inputptr;
    ssl_expr *expr;
} ssl_expr_info_type;

extern ssl_expr_info_type ssl_expr_info;
extern char *ssl_expr_error;

#define yylval  ssl_expr_yylval
#define yyerror ssl_expr_yyerror
#define yyinput ssl_expr_yyinput

extern int ssl_expr_yyparse(void);
extern int ssl_expr_yyerror(char *);
extern int ssl_expr_yylex(void);

extern ssl_expr *ssl_expr_comp(pool *, char *);
extern int       ssl_expr_exec(request_rec *, ssl_expr *);
extern char     *ssl_expr_get_error(void);
extern ssl_expr *ssl_expr_make(ssl_expr_node_op, void *, void *);
extern BOOL      ssl_expr_eval(request_rec *, ssl_expr *);

#endif /* SSL_EXPR_H */
