
#include "rest_task_uuid.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void uuid_generate_random(uuid_t *buf)
{
    // Initialize random number generator
    srand((unsigned)time(NULL));

    // Fill the buffer with random bytes
    for (int i = 0; i < UUID_LEN; i++)
    {
        ((unsigned char *)buf)[i] = rand() % 256; // Generate a random byte
    }

    // Mark off appropriate bits as per RFC4122 sction 4.4
    buf->clock_seq_hi_and_reserved = (buf->clock_seq_hi_and_reserved & 0x3F) | 0x80;
    buf->time_hi_and_version       = (buf->time_hi_and_version & 0x0FFF) | 0x4000;
}

int uuid_unparse(uuid_t uuid, char *out)
{
    return snprintf(out, UUID_STR_LEN, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", uuid.time_low, uuid.time_mid,
                    uuid.time_hi_and_version, uuid.clock_seq_hi_and_reserved, uuid.clock_seq_low, uuid.node[0],
                    uuid.node[1], uuid.node[2], uuid.node[3], uuid.node[4], uuid.node[5]);
}

int uuid_parse(char *in, uuid_t *uuid)
{
    int temp[11] = { 0 };
    int r = 0;

    if ((UUID_STR_LEN - 1) != strlen(in))
    {
        return -1;
    }

    memset(uuid, 0, sizeof(*uuid));

    r = sscanf(in, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", temp + 0, temp + 1, temp + 2, temp + 3, temp + 4, temp + 5,
               temp + 6, temp + 7, temp + 8, temp + 9, temp + 10);

    if (11 != r)
    {
        return -1;
    }

    uuid->time_low                  = temp[0];
    uuid->time_mid                  = temp[1];
    uuid->time_hi_and_version       = temp[2];
    uuid->clock_seq_hi_and_reserved = temp[3];
    uuid->clock_seq_low             = temp[4];
    for (int i = 0; i < 6; i++)
    {
        uuid->node[i] = temp[5 + i];
    }
    return 0;
}

int uuid_equals(uuid_t uuid1, uuid_t uuid2)
{
    return uuid1.time_low == uuid2.time_low && uuid1.time_mid == uuid2.time_mid &&
           uuid1.time_hi_and_version == uuid2.time_hi_and_version &&
           uuid1.clock_seq_hi_and_reserved == uuid2.clock_seq_hi_and_reserved &&
           uuid1.clock_seq_low == uuid2.clock_seq_low && uuid1.node[0] == uuid2.node[0] &&
           uuid1.node[1] == uuid2.node[1] && uuid1.node[2] == uuid2.node[2] && uuid1.node[3] == uuid2.node[3] &&
           uuid1.node[4] == uuid2.node[4] && uuid1.node[5] == uuid2.node[5];
}

#ifdef __cplusplus
}
#endif
