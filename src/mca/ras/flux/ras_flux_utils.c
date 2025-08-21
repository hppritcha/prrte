#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

#include "prte_config.h"
#include "src/include/constants.h"
#include "ras_flux.h"

/* RFC29: prefix/suffix: printable, non-whitespace ASCII, excluding '[', ']', ',' */
static int valid_text_component(const char *p, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c < 32 || c > 126) return 0;            /* not printable ASCII */
        if (isspace(c)) return 0;                   /* no whitespace */
        if (c == '[' || c == ']' || c == ',') return 0;
    }
    return 1;
}

/* Count how many ids are represented by idlist substring. */
static int count_idlist(const char *idlist, size_t len, uint64_t *out_count)
{
    if (!idlist || !out_count) {
        return -1;
    }

    if (len == 0) {
        return -1; /* empty [] treated as invalid */
    }

    const char *p = idlist;
    const char *end = idlist + len;
    uint64_t total = 0;

    while (p < end) {
        if (!isdigit((unsigned char)*p)) { 
            return -1; 
        }

        char *num_end = NULL;
        errno = 0;
        unsigned long long a_ull = strtoull(p, &num_end, 10);
        if (errno == ERANGE) {
            return -1;
        }
        if (num_end == p)  {
            return -1;
        }
        if ((const char *)num_end > end) {
            return -1;
        }

        uint64_t a = (uint64_t)a_ull;

        p = (const char *)num_end;

        uint64_t b = a;
        if (p < end && *p == '-') {
            p++;
            if (p >= end || !isdigit((unsigned char)*p)) {
                return -1;
            }

            errno = 0;
            unsigned long long b_ull = strtoull(p, &num_end, 10);
            if (errno == ERANGE) {
                return -1;
            }
            if (num_end == p) { 
                return -1; 
            }
            if ((const char *)num_end > end) { 
                return -1; 
            }
            b = (uint64_t)b_ull;
            p = (const char *)num_end;

            if (b < a) { 
                return -1;  /* bad range */
            }
        }

        /* add inclusive range length */
        uint64_t add = (b - a) + 1;
        if (UINT64_MAX - total < add) { 
            return -1; 
        }
        total += add;

        if (p == end) break;

        if (*p == ',') {
            p++;
            if (p == end) { 
                return -1; /* trailing comma */
            }
            continue;
        }

        /* any other char inside idlist is invalid (no whitespace allowed) */
        return -1;
    }

    *out_count = total;
    return 0;
}

static void free_outputs(char **out, size_t n)
{
    if (!out) return;
    for (size_t i = 0; i < n; i++) {
        free(out[i]);
        out[i] = NULL;
    }
}

/* Emit one host string into out[] */
static int emit_host(char **out, size_t cap, size_t *count,
                     const char *prefix, size_t plen,
                     uint64_t id, int pad_width,
                     const char *suffix, size_t slen)
{
    if (*count >= cap) {
        return -1;
    }

    /* format id with zero-padding to pad_width (minimum width) */
    char idbuf[64];
    if (pad_width < 0) pad_width = 0;
    if (pad_width > 60) pad_width = 60; /* sane guard */

    int idlen = snprintf(idbuf, sizeof idbuf, "%0*" PRIu64, pad_width, id);
    if (idlen < 0 || (size_t)idlen >= sizeof idbuf) {
        return -1;
    }

    size_t need = plen + (size_t)idlen + slen + 1;
    char *s = (char *)malloc(need);
    if (!s) {
        return -1;
    }

    memcpy(s, prefix, plen);
    memcpy(s + plen, idbuf, (size_t)idlen);
    memcpy(s + plen + (size_t)idlen, suffix, slen);
    s[need - 1] = '\0';

    out[*count] = s;
    (*count)++;
    return 0;
}

/*
 * Parse idlist substring [idlist_start, idlist_start+idlist_len)
 * and emit hosts prefix + id + suffix for each id.
 *
 * Rules implemented:
 *  - items separated by commas
 *  - each item: N or N-M (inclusive)
 *  - N and M are non-negative integers
 *  - pad_width is determined by digit-width of the FIRST numeric token,
 *    preserving its leading zeros across all ids.
 */
static int expand_idlist_and_emit(const char *idlist_start, size_t idlist_len,
                                 char **out, size_t cap, size_t *count,
                                 const char *prefix, size_t plen,
                                 const char *suffix, size_t slen)
{
    const char *p = idlist_start;
    const char *end = idlist_start + idlist_len;

    if (idlist_len == 0) { /* empty [] is not described as valid; treat as error */
        return -1;
    }

    int pad_width = -1; /* set from first numeric token */

    while (p < end) {
        /* token start */
        const char *tok = p;

        /* parse first number (a) */
        if (tok >= end || !isdigit((unsigned char)*tok)) {
            return -1;
        }
        char *num_end = NULL;
        errno = 0;
        unsigned long long a_ull = strtoull(tok, &num_end, 10);
        if (errno == ERANGE) {
            return -1;
        }
        if (num_end == tok) { 
            return -1;
        }
        if ((const char *)num_end > end) { errno = EINVAL; return -1; }
        uint64_t a = (uint64_t)a_ull;

        if (pad_width < 0) {
            /* width of first numeric token, including leading zeros */
            ptrdiff_t w = (const char *)num_end - tok;
            if (w <= 0 || w > INT_MAX) { 
                return -1; 
            }
            pad_width = (int)w;
        }

        p = (const char *)num_end;

        uint64_t b = a;
        if (p < end && *p == '-') {
            p++;
            if (p >= end || !isdigit((unsigned char)*p)) {
                errno = EINVAL;
                return -1;
            }
            errno = 0;
            unsigned long long b_ull = strtoull(p, &num_end, 10);
            if (errno == ERANGE) {
                return -1;
            }
            if (num_end == p) { 
                return -1; 
            }
            if ((const char *)num_end > end) { 
                return -1; 
            }
            b = (uint64_t)b_ull;
            p = (const char *)num_end;

            if (b < a) { 
                return -1; 
            }
        }

        /* emit ids for this token */
        for (uint64_t v = a; v <= b; v++) {
            if (emit_host(out, cap, count, prefix, plen, v, pad_width, suffix, slen) < 0) {
                return -1;
            }
            if (v == UINT64_MAX) break; /* safety (though b>=a and a parsed) */
        }

        if (p == end) break;

        if (*p == ',') {
            p++;
            if (p == end) { 
                return -1; /* trailing comma */
            }
            continue;
        }

        /* Any other char in idlist is invalid (no whitespace allowed) */
        return -1;
    }

    return 0;
}

/*
 * process_hostlist():
 *   - hostlist: RFC29 hostlist string (https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/spec_29.html)
 *   - array_of_hosts: output array of char* (each element malloc'd)
 *   - num_hosts: IN capacity, OUT count
 *
 * Returns PRTE_SUCCESS on success, otherwise PRTE error code
 */
int prte_ras_flux_process_hostlist(const char *hostlist, char **array_of_hosts, int *num_hosts)
{
    if (!hostlist || !array_of_hosts || !num_hosts || *num_hosts < 0) {
        return PRTE_ERR_BAD_PARAM;
    }

    size_t cap = (size_t)*num_hosts;
    size_t count = 0;

    /* Empty hostlist expression "" => empty list */
    if (*hostlist == '\0') {
        *num_hosts = 0;
        return PRTE_SUCCESS;
    }

    const char *p = hostlist;
    const char *expr_start = p;
    int bracket_depth = 0;

    while (1) {
        char c = *p;

        if (c == '\0' || (c == ',' && bracket_depth == 0)) {
            /* expression is [expr_start, p) */
            size_t expr_len = (size_t)(p - expr_start);

            if (expr_len == 0) {
                /* disallow empty expressions inside a list, e.g., "a,,b" */
                free_outputs(array_of_hosts, count);
                return PRTE_ERR_BAD_PARAM;
            }

            /* parse expression: prefix[idlist]suffix, where [idlist] optional */
            const char *lb = memchr(expr_start, '[', expr_len);
            if (!lb) {
                /* No idlist: just a single hostname string */
                if (!valid_text_component(expr_start, expr_len)) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_BAD_PARAM;
                }
                if (count >= cap) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_OUT_OF_RESOURCE;
                }
                char *s = strndup(expr_start, expr_len);
                if (!s) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_OUT_OF_RESOURCE;
                }
                array_of_hosts[count++] = s;
            } else {
                /* Must have matching ']' after '[' within this expression */
                size_t prefix_len = (size_t)(lb - expr_start);
                const char *rb = memchr(lb + 1, ']', expr_len - prefix_len - 1);
                if (!rb) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_BAD_PARAM;
                }

                size_t idlist_len = (size_t)(rb - (lb + 1));
                const char *suffix = rb + 1;
                size_t suffix_len = (expr_start + expr_len) - suffix;

                /* Validate prefix and suffix (idlist validation happens in parser) */
                if (!valid_text_component(expr_start, prefix_len) ||
                    !valid_text_component(suffix, suffix_len)) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_BAD_PARAM;
                }

                if (expand_idlist_and_emit(lb + 1, idlist_len,
                                           array_of_hosts, cap, &count,
                                           expr_start, prefix_len,
                                           suffix, suffix_len) < 0) {
                    free_outputs(array_of_hosts, count);
                    return PRTE_ERR_BAD_PARAM;
                }
            }

            if (c == '\0')
                break;

            /* move to next expression */
            p++; /* skip comma */
            expr_start = p;
            continue;
        }

        /* Track bracket depth so we only split on top-level commas */
        if (c == '[') bracket_depth++;
        else if (c == ']') bracket_depth--;

        /* RFC29 disallows whitespace anywhere */
        if (isspace((unsigned char)c) || bracket_depth < 0 || bracket_depth > 1) {
            free_outputs(array_of_hosts, count);
            return PRTE_ERR_BAD_PARAM;
        }

        p++;
    }

    if (bracket_depth != 0) {
        free_outputs(array_of_hosts, count);
        return PRTE_ERR_BAD_PARAM;
    }

    if (count > (size_t)INT_MAX) {
        free_outputs(array_of_hosts, count);
        return PRTE_ERR_BAD_PARAM;
    }

    *num_hosts = (int)count;
    return PRTE_SUCCESS;
}

/*
 * Count number of hosts expanded by an RFC29 hostlist.
 * https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/spec_29.html
 *
 * num_hosts is OUT.
 * Returns PRTE_SUCCESS on success, otherwise PRTE eror code
 */
int prte_ras_flux_hostlist_count(const char *hostlist, int *num_hosts)
{
    if (!hostlist || !num_hosts) { 
        return PRTE_ERR_BAD_PARAM; 
    }

    /* Empty hostlist expression "" => empty list */
    if (*hostlist == '\0') {
        *num_hosts = 0;
        return PRTE_SUCCESS;
    }

    const char *p = hostlist;
    const char *expr_start = p;
    int bracket_depth = 0;
    uint64_t total_hosts = 0;

    while (1) {
        char c = *p;

        if (c == '\0' || (c == ',' && bracket_depth == 0)) {
            /* expression is [expr_start, p) */
            size_t expr_len = (size_t)(p - expr_start);
            if (expr_len == 0) { 
                return PRTE_ERR_BAD_PARAM; /* "a,,b" */
            }

            const char *lb = memchr(expr_start, '[', expr_len);
            if (!lb) {
                /* No idlist: exactly one host */
                if (!valid_text_component(expr_start, expr_len)) { 
                    return PRTE_ERR_BAD_PARAM; 
                }
                if (UINT64_MAX - total_hosts < 1) { 
                    return PRTE_ERR_BAD_PARAM;
                }
                total_hosts += 1;
            } else {
                /* Must have matching ']' after '[' within this expression */
                size_t prefix_len = (size_t)(lb - expr_start);
                const char *rb = memchr(lb + 1, ']', expr_len - prefix_len - 1);
                if (!rb) { 
                    return PRTE_ERR_BAD_PARAM;
                }

                size_t idlist_len = (size_t)(rb - (lb + 1));
                const char *suffix = rb + 1;
                size_t suffix_len = (expr_start + expr_len) - suffix;

                if (!valid_text_component(expr_start, prefix_len) ||
                    !valid_text_component(suffix, suffix_len)) {
                    return PRTE_ERR_BAD_PARAM;
                }

                uint64_t ids = 0;
                if (count_idlist(lb + 1, idlist_len, &ids) < 0) {
                    return PRTE_ERR_BAD_PARAM;
                }

                if (UINT64_MAX - total_hosts < ids) { 
                    return PRTE_ERR_BAD_PARAM;
                }
                total_hosts += ids;
            }

            if (c == '\0')
                break;

            /* next expression */
            p++; /* skip comma */
            expr_start = p;
            continue;
        }

        if (c == '[') bracket_depth++;
        else if (c == ']') bracket_depth--;

        /* RFC29 disallows whitespace; also prevent nesting and unmatched ] */
        if (isspace((unsigned char)c) || bracket_depth < 0 || bracket_depth > 1) {
            return PRTE_ERR_BAD_PARAM;
        }

        p++;
    }

    if (bracket_depth != 0) { 
        return PRTE_ERR_BAD_PARAM;
    }
    if (total_hosts > (uint64_t)INT_MAX) { 
        return PRTE_ERR_BAD_PARAM;
    }

    *num_hosts = (int)total_hosts;
    return PRTE_SUCCESS; 
}


/*
 * Count the number of integer ids represented by an idset string.
 *
 * Examples:
 *   "0-4,8"      -> 6
 *   "0,1,5,10"   -> 4
 *   "[1-3,5-6]"  -> 5
 *
 * Returns:
 *   >= 0  : count of ids
 *   -1    : error
 *
 *   https://flux-framework.readthedocs.io/projects/flux-rfc/en/latest/spec_22.html
 */
ssize_t prte_ras_flux_idset_count(const char *s)
{
    if (!s) {
        return -1;
    }

    const char *p = s;

    /* Optional surrounding brackets */
    int bracketed = 0;
    if (*p == '[') {
        bracketed = 1;
        p++;
    }

    /* Allow empty set: "" or "[]" -> 0 */
    if (*p == '\0' || (bracketed && *p == ']')) {
        if (bracketed) {
            if (*p != ']') { 
                return -1; 
            }
            p++;
        }
        if (*p != '\0') { 
            return -1; 
        }
        return 0;
    }

    uint64_t last = 0;
    int have_last = 0;
    uint64_t total = 0;

    while (*p) {
        char *end = NULL;
        errno = 0;
        unsigned long long a_ull = strtoull(p, &end, 10);
        if (end == p) { 
            return -1;          /* no digits */
        }
        if (errno == ERANGE) {
            return -1;
        }
        if (a_ull > UINT64_MAX) { 
            return -1;
        }
        uint64_t a = (uint64_t)a_ull;

        p = end;

        uint64_t b = a;
        if (*p == '-') {
            p++;
            errno = 0;
            unsigned long long b_ull = strtoull(p, &end, 10);
            if (end == p) { 
               return -1; 
            }
            if (errno == ERANGE) {
                return -1;
            }
            if (b_ull > UINT64_MAX) { 
                return -1; 
            }
            b = (uint64_t)b_ull;
            p = end;

            if (b < a) { 
                return -1;           /* bad range */
            }
        }

        /* Enforce ascending + unique across items (per spec) */
        if (have_last) {
            if (a <= last) { 
                return -1; 
            }
        }
        have_last = 1;
        last = b;

        /* Add inclusive range length, checking overflow */
        uint64_t len = (b - a) + 1;
        if (UINT64_MAX - total < len) { 
            return -1; 
        }
        total += len;

        if (*p == ',') {
            p++;
            if (*p == '\0') { errno = EINVAL; return -1; }     /* trailing comma */
            continue;
        }

        if (bracketed) {
            if (*p == ']') {
                p++;
                if (*p != '\0') { errno = EINVAL; return -1; } /* junk after ] */
                break;
            }
        }

        if (*p == '\0') break;

        /* Any other character is invalid under the RFC character set */
        return -1;
    }

    if (total > (uint64_t)SSIZE_MAX) {
        return -1;
    }
    return (ssize_t)total;
}

