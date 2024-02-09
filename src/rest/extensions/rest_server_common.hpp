#ifndef REST_SERVER_COMMON_HPP_
#define REST_SERVER_COMMON_HPP_

#include "utils/thread_helper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "openthread/error.h"

typedef enum
{
    LOCK_TYPE_BLOCKING,
    LOCK_TYPE_NONBLOCKING,
    LOCK_TYPE_TIMED
} LockType;

#define WPANSTATUS_OK 0
#define OT_NETWORKKEY_LENGTH 32
#define OT_PSKD_LENGTH_MIN 6
#define OT_PSKD_LENGTH_MAX 32
#define OT_JOINFAILED_LENGTH 16
#define OT_JOINFAILED_PSKD_FORMAT 17

uint8_t joiner_verify_pskd(char *pskd);

/**
 * @brief   str_to_m8, is designed to convert a string of hexadecimal characters
 *          into an array of bytes (uint8_t). It performs this conversion by processing
 *          each pair of hexadecimal characters in the input string, converting them
 *          into their corresponding byte value, and storing the result in the provided array.
 * @param   uint8_t *m8: A pointer to the array where the converted bytes will be stored.
 * @param   const char *str: A pointer to the input string containing hexadecimal characters.
 * @param   uint8_t size: The number of bytes that the m8 array can hold, which dictates how many characters from str
 * should be processed.
 * @return    The function returns an otError code, indicating the success or failure of the conversion process.
 */
otError str_to_m8(uint8_t *m8, const char *str, uint8_t size);

// TODO-SPAR11
/**
 * @brief [DEPRECATED] please use isValidPerRegex() for new work.
 * This function checks if the input string is hex or not
 * @param str Hex string to be checked
 * @return true if the string is HEX
 * @return false if the string is not HEX
 */
bool is_hex_string(char *str);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif
