#include "rest_server_common.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <ctype.h>
#include <openthread/logging.h>

static int hex_char_to_int(char c)
{
    if (('A' <= c) && (c <= 'F'))
    {
        return (uint8_t)c - (uint8_t)'A' + 10;
    }
    if (('a' <= c) && (c <= 'f'))
    {
        return (uint8_t)c - (uint8_t)'a' + 10;
    }
    if (('0' <= c) && (c <= '9'))
    {
        return (uint8_t)c - (uint8_t)'0';
    }
    return -1;
}

uint8_t joiner_verify_pskd(char *pskd)
{
    int len = strlen(pskd);
    if (OT_PSKD_LENGTH_MIN > len)
    {
        otLogWarnPlat("PSKd %s has incorrect length %d", pskd, len);
        return OT_JOINFAILED_LENGTH;
    }
    if (OT_PSKD_LENGTH_MAX < len)
    {
        otLogWarnPlat("PSKd %s has incorrect length %d", pskd, len);
        return OT_JOINFAILED_LENGTH;
    }
    for (int i = 0; i < len; i++)
    {
        if (!isalnum(pskd[i]))
        {
            otLogWarnPlat("PSKd %s has incorrect format and is not alphanumeric", pskd);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
        if (islower(pskd[i]))
        {
            otLogWarnPlat("PSKd %s has incorrect format and is not all uppercase", pskd);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
        if ('I' == pskd[i] || 'O' == pskd[i] || 'Q' == pskd[i] || 'Z' == pskd[i])
        {
            otLogWarnPlat("PSKd %s has incorrect format and contains illegal character %c", pskd, pskd[i]);
            return OT_JOINFAILED_PSKD_FORMAT;
        }
    }
    return WPANSTATUS_OK;
}

otError str_to_m8(uint8_t *m8, const char *str, uint8_t size)
{
    if (size * 2 > strlen(str))
    {
        return OT_ERROR_FAILED;
    }

    for (int i = 0; i < size; i++)
    {
        int hex_int_1 = hex_char_to_int(str[i * 2]);
        int hex_int_2 = hex_char_to_int(str[i * 2 + 1]);
        if (-1 == hex_int_1 || -1 == hex_int_2)
        {
            return OT_ERROR_FAILED;
        }
        m8[i] = (uint8_t)(hex_int_1 * 16 + hex_int_2);
    }

    return OT_ERROR_NONE;
}

bool is_hex_string(char *str)
{
    int offset = 0;
    if ('x' == str[1])
    {
        if ('0' != str[0])
        {
            return false;
        }
        offset = 2;
    }
    for (size_t i = offset; i < strlen(str); i++)
    {
        if (!isxdigit(str[i]))
        {
            return false;
        }
    }
    return true;
}

#ifdef __cplusplus
}
#endif
