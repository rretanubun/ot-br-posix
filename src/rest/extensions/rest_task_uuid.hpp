#ifndef REST_TASK_UUID_HPP_
#define REST_TASK_UUID_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define UUID_LEN 16
#define UUID_STR_LEN 37

typedef union uuid
{
    struct
    {
        uint32_t time_low;
        uint16_t time_mid;
        uint16_t time_hi_and_version;
        uint8_t  clock_seq_hi_and_reserved;
        uint8_t  clock_seq_low;
        uint8_t  node[6];
    };
    uint8_t buf[UUID_LEN];
} uuid_t;

/**
 * @fn      uuid_generate_random
 *
 * @brief   The uuid_generate_random function creates a new universally unique
 *          identifier (UUID) version 4 as described by RFC4122 section 4.2
 *
 * @param   *out Pointer to a uuid_t to populate with the generated UUID
 */
void uuid_generate_random(uuid_t *buf);

/**
 * @fn     uuid_unparse
 *
 * @brief  Convert a supplied UUID to its string format
 *
 * @param  uuid The uuid to convert
 * @param  out Pointer to the buffer to place the string
 */
int uuid_unparse(uuid_t uuid, char *out);

/**
 * @fn    uuid_parse
 *
 * @brief Convert a supplied UUID in string to uuid_t format
 *
 * @param in The string to convert
 * @param uuid Pointer to the uuid to store the result in
 * @return
 */
int uuid_parse(char *in, uuid_t *uuid);

/**
 * @fn    uuid_equals
 *
 * @brief Check if the two provided UUIDs are equal
 *
 * @param uuid1
 * @param uuid2
 * @return
 */
int uuid_equals(uuid_t uuid1, uuid_t uuid2);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif // REST_TASK_UUID_HPP_
