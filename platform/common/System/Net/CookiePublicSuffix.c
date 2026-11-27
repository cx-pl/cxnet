#include "cxnet.internal.h"
#include "CookiePublicSuffixRules.h"
#include <string.h>

static void cxnet_cookie_suffix_lower_ascii(char *value) {
    while (*value != '\0') {
        if (*value >= 'A' && *value <= 'Z') *value = (char)(*value + ('a' - 'A'));
        value += 1;
    }
}

static cx_bool cxnet_cookie_suffix_rule_contains(
    const char *const *rules, size_t rule_count, const char *candidate) {
    size_t low = 0u;
    size_t high = rule_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2u;
        int comparison = strcmp(candidate, rules[middle]);
        if (comparison == 0) return CX_TRUE;
        if (comparison < 0) high = middle;
        else low = middle + 1u;
    }
    return CX_FALSE;
}

cx_bool cxnet_System_Net_CookieContainer_IsPublicSuffix(
    struct cxcore_System_String *domain) {
    char host[254];
    size_t length;
    size_t index;
    cx_bool numeric_address = CX_TRUE;
    char *first_dot;
    if (domain == NULL || domain->_data == NULL || domain->_length == 0u ||
        domain->_length > 253u ||
        memchr(domain->_data, '\0', domain->_length) != NULL) return CX_FALSE;
    length = (size_t)domain->_length;
    memcpy(host, domain->_data, length);
    host[length] = '\0';
    while (length > 0u && host[length - 1u] == '.') {
        host[length - 1u] = '\0';
        length -= 1u;
    }
    if (length == 0u || strchr(host, ':') != NULL) return CX_FALSE;
    for (index = 0u; index < length; index += 1u) {
        if (host[index] != '.' && (host[index] < '0' || host[index] > '9')) {
            numeric_address = CX_FALSE;
            break;
        }
    }
    if (numeric_address) return CX_FALSE;
    cxnet_cookie_suffix_lower_ascii(host);

    /* An exception rule makes the exception itself registrable. */
    if (cxnet_cookie_suffix_rule_contains(cxnet_psl_exception,
            CXNET_PSL_EXCEPTION_COUNT, host)) return CX_FALSE;
    if (cxnet_cookie_suffix_rule_contains(cxnet_psl_exact,
            CXNET_PSL_EXACT_COUNT, host)) return CX_TRUE;

    /* The PSL's implicit "*" rule makes every single label a suffix. */
    first_dot = strchr(host, '.');
    if (first_dot == NULL) return CX_TRUE;

    /* A wildcard such as *.ck matches exactly one label before its base. */
    return cxnet_cookie_suffix_rule_contains(cxnet_psl_wildcard,
        CXNET_PSL_WILDCARD_COUNT, first_dot + 1);
}
