/* 
 * (c) 2008 Steve Bennett <steveb@workware.net.au>
 *
 * Implements the regexp and regsub commands for Jim
 *
 * Uses C library regcomp()/regexec() for the matching.
 *
 * The FreeBSD license
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 *
 * Based on code originally from Tcl 6.7:
 *
 * Copyright 1987-1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <regex.h>
#include <string.h>

#define JIM_EXTENSION
#include "jim.h"

/* REVISIT: Would be useful in jim.h */
static void Jim_SetIntResult(Jim_Interp *interp, jim_wide wide)
{
    Jim_SetResult(interp, Jim_NewIntObj(interp, wide));
}

/**
 * REVISIT: Should cache a number of compiled regexps for performance reasons.
 */
static regex_t * 
compile_regexp(Jim_Interp *interp, const char *pattern, int flags)
{
    int ret;

    regex_t *result = (regex_t *)Jim_Alloc(sizeof(*result));

    if ((ret = regcomp(result, pattern, REG_EXTENDED | flags)) != 0) {
        char buf[100];
        regerror(ret, result, buf, sizeof(buf));
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp), "couldn't compile regular expression pattern: ", buf, NULL);
        Jim_Free(result);
        return NULL;
    }
    return result;
}

int Jim_RegexpCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int opt_indices = 0;
    int opt_all = 0;
    int opt_inline = 0;
    regex_t *regex;
    int match, i, j;
    long offset = 0;
    regmatch_t *pmatch = NULL;
    int source_len;
    int result = JIM_OK;
    const char *pattern;
    const char *source_str;
    int num_matches = 0;
    int num_vars;
    Jim_Obj *resultListObj = NULL;
    int regcomp_flags = 0;

    if (argc < 3) {
        wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv, "?-nocase? ?-line? ?-indices? ?-start offset? ?-all? ?-inline? exp string ?matchVar? ?subMatchVar ...?");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        if (Jim_CompareStringImmediate(interp, argv[i], "-indices")) {
            opt_indices = 1;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-nocase")) {
            regcomp_flags |= REG_ICASE;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-line")) {
            regcomp_flags |= REG_NEWLINE;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-all")) {
            opt_all = 1;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-inline")) {
            opt_inline = 1;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-start")) {
            if (++i == argc) {
                goto wrongNumArgs;
            }
            if (Jim_GetLong(interp, argv[i], &offset) != JIM_OK) {
                return JIM_ERR;
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "--")) {
            i++;
            break;
        }
        else {
            const char *opt = Jim_GetString(argv[i], NULL);
            if (*opt == '-') {
                /* Bad option */
                goto wrongNumArgs;
            }
            break;
        }
    }
    if (argc - i < 2) {
        goto wrongNumArgs;
    }

    pattern = Jim_GetString(argv[i], NULL);
    regex = compile_regexp(interp, pattern, regcomp_flags);
    if (regex == NULL) {
        return JIM_ERR;
    }

    source_str = Jim_GetString(argv[i + 1], &source_len);

    num_vars = argc - i - 2;

    if (opt_inline) {
        if (num_vars) {
            Jim_SetResultString(interp, "regexp match variables not allowed when using -inline", -1);
            result = JIM_ERR;
            goto done;
        }
        /* REVISIT: Ugly! */
        num_vars = 100;
    }

    pmatch = Jim_Alloc((num_vars + 1) * sizeof(*pmatch));

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset > source_len) {
            source_str += source_len;
        } else if (offset > 0) {
            source_str += offset;
        }
    }

    if (opt_inline) {
        resultListObj = Jim_NewListObj(interp, NULL, 0);
    }

    next_match:
    match = regexec(regex, source_str, num_vars + 1, pmatch, 0);
    if (match >= REG_BADPAT) {
        char buf[100];
        regerror(match, regex, buf, sizeof(buf));
        Jim_SetResultString(interp, "", 0);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "error while matching pattern: ", buf, NULL);
        result = JIM_ERR;
        goto done;
    }

    if (match == REG_NOMATCH) {
        goto done;
    }

    num_matches++;

    if (opt_all && !opt_inline) {
        /* Just count the number of matches, so skip the substitution h*/
        goto try_next_match;
    }

    /*
     * If additional variable names have been specified, return
     * index information in those variables.
     */

    //fprintf(stderr, "source_str=%s, [0].rm_eo=%d\n", source_str, pmatch[0].rm_eo);

    j = 0;
    for (i += 2; opt_inline ? pmatch[j].rm_so != -1 : i < argc; i++, j++) {
        Jim_Obj *resultObj;

        if (opt_indices) {
            resultObj = Jim_NewListObj(interp, NULL, 0);
        }
        else {
            resultObj = Jim_NewStringObj(interp, "", 0);
        }

        if (pmatch[j].rm_so == -1) {
            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, -1));
            }
        } else {
            int len = pmatch[j].rm_eo - pmatch[j].rm_so;
            if (opt_indices) {
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, offset + pmatch[j].rm_so));
                Jim_ListAppendElement(interp, resultObj, Jim_NewIntObj(interp, offset + pmatch[j].rm_so + len - 1));
            } else {
                Jim_AppendString(interp, resultObj, source_str + pmatch[j].rm_so, len);
            }
        }

        if (opt_inline) {
            Jim_ListAppendElement(interp, resultListObj, resultObj);
        }
        else {
            /* And now set the result variable */
            result = Jim_SetVariable(interp, argv[i], resultObj);

            if (result != JIM_OK) {
                Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
                Jim_AppendStrings(interp, Jim_GetResult(interp), "couldn't set variable \"", Jim_GetString(argv[i], NULL), "\"", NULL);
                Jim_FreeObj(interp, resultObj);
                break;
            }
        }
    }

    try_next_match:
    if (opt_all && pattern[0] != '^' && *source_str) {
        if (pmatch[0].rm_eo) {
            source_str += pmatch[0].rm_eo;
        }
        else {
            source_str++;
        }
        if (*source_str) {
            goto next_match;
        }
    }

    done:
    if (result == JIM_OK) {
        if (opt_inline) {
            Jim_SetResult(interp, resultListObj);
        }
        else {
            Jim_SetIntResult(interp, num_matches);
        }
    }

    Jim_Free(pmatch);
    regfree(regex);
    Jim_Free(regex);
    return result;
}

#define MAX_SUB_MATCHES 10

int Jim_RegsubCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int regcomp_flags = 0;
    int opt_all = 0;
    long offset = 0;
    regex_t *regex;
    const char *p;
    int result = JIM_ERR;
    regmatch_t pmatch[MAX_SUB_MATCHES + 1];
    int num_matches = 0;

    int i;
    Jim_Obj *varname;
    Jim_Obj *resultObj;
    const char *source_str;
    int source_len;
    const char *replace_str;
    const char *pattern;

    if (argc < 5) {
        wrongNumArgs:
        Jim_WrongNumArgs(interp, 1, argv, "?-nocase? ?-all? exp string subSpec varName");
        return JIM_ERR;
    }

    for (i = 1; i < argc; i++) {
        if (Jim_CompareStringImmediate(interp, argv[i], "-nocase")) {
            regcomp_flags |= REG_ICASE;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-line")) {
            regcomp_flags |= REG_NEWLINE;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-all")) {
            opt_all = 1;
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "-start")) {
            if (++i == argc) {
                goto wrongNumArgs;
            }
            if (Jim_GetLong(interp, argv[i], &offset) != JIM_OK) {
                return JIM_ERR;
            }
        }
        else if (Jim_CompareStringImmediate(interp, argv[i], "--")) {
            i++;
            break;
        }
        else {
            const char *opt = Jim_GetString(argv[i], NULL);
            if (*opt == '-') {
                /* Bad option */
                goto wrongNumArgs;
            }
            break;
        }
    }
    if (argc - i != 4) {
        goto wrongNumArgs;
    }

    pattern = Jim_GetString(argv[i], NULL);
    regex = compile_regexp(interp, pattern, regcomp_flags);
    if (regex == NULL) {
        return JIM_ERR;
    }

    source_str = Jim_GetString(argv[i + 1], &source_len);
    replace_str = Jim_GetString(argv[i + 2], NULL);
    varname = argv[i + 3];

    /* Create the result string */
    resultObj = Jim_NewStringObj(interp, "", 0);

    /* If an offset has been specified, adjust for that now.
     * If it points past the end of the string, point to the terminating null
     */
    if (offset) {
        if (offset > source_len) {
            offset = source_len;
        } else if (offset < 0) {
            offset = 0;
        }
    }

    /* Copy the part before -start */
    Jim_AppendString(interp, resultObj, source_str, offset);

    /*
     * The following loop is to handle multiple matches within the
     * same source string;  each iteration handles one match and its
     * corresponding substitution.  If "-all" hasn't been specified
     * then the loop body only gets executed once.
     */

    for (p = source_str + offset; *p != 0; ) {
        const char *src;
        int match = regexec(regex, p, MAX_SUB_MATCHES, pmatch, 0);
        if (match >= REG_BADPAT) {
            char buf[100];
            regerror(match, regex, buf, sizeof(buf));
            Jim_SetResultString(interp, "", 0);
            Jim_AppendStrings(interp, Jim_GetResult(interp), "error while matching pattern: ", buf, NULL);
            goto done;
        }
        if (match == REG_NOMATCH) {
            break;
        }

        num_matches++;

        /*
         * Copy the portion of the source string before the match to the
         * result variable.
         */
        Jim_AppendString(interp, resultObj, p, pmatch[0].rm_so);

        /*
         * Append the subSpec (replace_str) argument to the variable, making appropriate
         * substitutions.  This code is a bit hairy because of the backslash
         * conventions and because the code saves up ranges of characters in
         * subSpec to reduce the number of calls to Jim_SetVar.
         */
    
        for (src = replace_str; *src; src++) {
            int index;
            int c = *src;

            if (c == '&') {
                index = 0;
            }
            else if (c == '\\') {
                c = *++src;
                if ((c >= '0') && (c <= '9')) {
                    index = c - '0';
                }
                else if ((c == '\\') || (c == '&')) {
                    Jim_AppendString(interp, resultObj, src, 1);
                    continue;
                }
                else {
                    Jim_AppendString(interp, resultObj, src - 1, 2);
                    continue;
                }
            }
            else {
                Jim_AppendString(interp, resultObj, src, 1);
                continue;
            }
            if ((index < MAX_SUB_MATCHES) && pmatch[index].rm_so != -1 && pmatch[index].rm_eo != -1) {
                Jim_AppendString(interp, resultObj, p + pmatch[index].rm_so, pmatch[index].rm_eo - pmatch[index].rm_so);
            }
        }

        p += pmatch[0].rm_eo;

        if (!opt_all || pmatch[0].rm_eo == 0 || pattern[0] == '^') {
            /* If we are doing a single match, or we haven't moved with this match
             * or this is an anchored match, we stop */
            break;
        }
    }

    /*
     * Copy the portion of the string after the last match to the
     * result variable.
     */
    Jim_AppendString(interp, resultObj, p, -1);

    /* And now set the result variable */
    result = Jim_SetVariable(interp, varname, resultObj);

    if (result == JIM_OK) {
        Jim_SetIntResult(interp, num_matches);
    }
    else {
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        Jim_AppendStrings(interp, Jim_GetResult(interp), "couldn't set variable \"", Jim_GetString(varname, NULL), "\"", NULL);
        Jim_FreeObj(interp, resultObj);
    }

    done:
    regfree(regex);
    Jim_Free(regex);
    return result;
}

int Jim_OnLoad(Jim_Interp *interp)
{
        Jim_InitExtension(interp);
        if (Jim_PackageProvide(interp, "regexp", "1.0", JIM_ERRMSG) != JIM_OK) {
                return JIM_ERR;
        }
        Jim_CreateCommand(interp, "regexp", Jim_RegexpCmd, NULL, NULL);
        Jim_CreateCommand(interp, "regsub", Jim_RegsubCmd, NULL, NULL);
        return JIM_OK;
}
