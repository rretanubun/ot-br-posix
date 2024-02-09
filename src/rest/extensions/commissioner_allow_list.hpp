#ifndef EXTENSIONS_COMMISSIONER_ALLOW_LIST_HPP_
#define EXTENSIONS_COMMISSIONER_ALLOW_LIST_HPP_

#include "linked_list.hpp"
#include "rest/extensions/parse_cmdline.hpp"
#include "rest/extensions/pthread_lock.hpp"
#include "rest/extensions/rest_server_common.hpp"
#include "rest/extensions/rest_task_uuid.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

#include "openthread/commissioner.h"

#define JSON_TYPE "type"
#define JSON_ATTRIBUTES "attributes"
#define JSON_HASACTIVATIONKEY "hasActivationKey"
#define JSON_EUI "eui"
#define JSON_PSKD "pskd"
#define JSON_TIMEOUT "timeout"
#define JSON_UUID "uuid"
#define JSON_ALLOW_STATE "state"

#define JSON_ALLOW_LIST_TYPE "addThreadDeviceTask"

/**
 * This class implements an allow list entry. It maintains the device's ID,
 * eui64, join timeout, and Joiner PSKd
 *
 */
class AllowListEntry : public allow_list::LinkedListEntry<AllowListEntry>
{
    friend class allow_list::LinkedList<AllowListEntry>;
    ;

public:
    enum AllowListEntryState : uint8_t
    {
        kAllowListEntryNew,
        kAllowListEntryActiveJoiner,
        kAllowListEntryJoined,
        kAllowListEntryJoinFailed
    };

    /**
     * This constructor creates an AllowListEntry.
     */
    AllowListEntry(otExtAddress aEui64, uuid_t uuid, uint32_t aTimeout, char *aPskd)
    {
        meui64   = aEui64;
        muuid    = uuid;
        mTimeout = aTimeout;
        mPSKd    = aPskd;
        mstate   = kAllowListEntryNew;
        mNext    = nullptr;
    }

    AllowListEntry(otExtAddress aEui64, uuid_t uuid, uint32_t aTimeout, AllowListEntryState state, char *aPskd)
    {
        meui64   = aEui64;
        muuid    = uuid;
        mTimeout = aTimeout;
        mPSKd    = aPskd;
        mstate   = state;
        mNext    = nullptr;
    }

    /**
     * Update Entry State
     */
    void update_state(AllowListEntryState new_state) { mstate = new_state; }

    /**
     *
     * @return
     */
    bool is_joined(void) const { return (kAllowListEntryJoined == mstate); }
    bool is_failed(void) const { return (kAllowListEntryJoinFailed == mstate); }

    /**
     * @brief JSON-ify Allow List Entry using the specified entryType
     *
     * @param entryType The string to use for "type" attribute
     *                  (e.g. "addThreadDeviceTask" for actions or "device" for reportng)
     * @return cJSON* The created JSON object
     */
    cJSON *Allow_list_entry_as_CJSON(const char *entryType);

    // Members
    otExtAddress        meui64;
    uuid_t              muuid;
    uint32_t            mTimeout;
    char               *mPSKd;
    AllowListEntryState mstate;
    AllowListEntry     *mNext;
};

/**
 * @brief Find an allow List entry via the device's eui64 address
 *
 * @param[in] aEui64 eui64 address of the device to find
 *
 * @return pointer to the desired entry
 *         NULL if the entry could be found
 */
AllowListEntry *entryEui64Find(const otExtAddress *aEui64);

/**
 * @brief Check if the provided address is not null
 *
 * @param[in] aEui64 The EUI64 address to check.
 *
 * @return true if address is NULL, false otherwise
 */
bool eui64IsNull(const otExtAddress aEui64);

/**
 * @brief Add a device to the allow list and the On-Mesh commissioner
 *
 * @param[in]  aEui64 eui64 Address of the devive to add
 * @param[in]  aTimeout timeout to use when adding a commissioner joiner for the device, after which a Joiner is
 * automatically removed, in seconds.
 * @param[in]  aPskd Pskd to use when joining the device
 * @param[in]  aInstance Openthread instance
 *
 * @note If provided with a NULL eui64, this function will return immediately, unless
 * OPENTHREAD_COMMISSIONER_ALLOW_ANY_JOINER is defined.
 *
 * @return OT_ERROR_NONE          Successfully added the Joiner.
 *         OT_ERROR_NO_BUFS       No buffers available to add the Joiner.
 *         OT_ERROR_INVALID_ARGS  @p aPskd is invalid or @p aEui64 is NULL and OPENTHREAD_COMMISSIONER_ALLOW_ANY_JOINER
 * is not set. OT_ERROR_INVALID_STATE On-Mesh commissioner is not active.
 *
 */
otError allowListCommissionerJoinerAdd(otExtAddress aEui64, uint32_t aTimeout, char *aPskd, otInstance *aInstance);

/**
 * @brief Remove a single entry from On-Mesh commissioner joiner table
 *
 * @param[in] aEui64 The EUI64 address of the device to be removed from the commissioner joiner table.
 * @param[in] aInstance Openthread instance
 *
 * @retval OT_ERROR_NONE          Successfully removed the Joiner.
 *         OT_ERROR_NOT_FOUND     Joiner specified by @p aEui64 was not found.
 *         OT_ERROR_INVALID_STATE On-Mesh commissioner is not active.
 */
otError allowListCommissionerJoinerRemove(otExtAddress aEui64, otInstance *aInstance);

/**
 * @brief Remove a single entry from the allow list
 *
 * @param[in] aEui64 The EUI64 address of the device to be removed from the allow list.
 *
 * @return OT_ERROR_NONE       The entry was successfully removed from the list.
 *         OT_ERROR_NOT_FOUND  Could not find the entry in the list.
 */
otError allowListEntryErase(otExtAddress aEui64);

/**
 * @brief  Add a new device (entry) to the allow List internal linked list
 *
 * @param[in]  aEui64 eui64 Address of the devive to add
 * @param[in]  aTimeout timeout to use when adding a commissioner joiner for the device, after which a Joiner is
 * automatically removed, in seconds.
 * @param[in]  aPskd Pskd to use when joinig the device
 */
void allowListAddDevice(otExtAddress aEui64, uint32_t aTimeout, char *aPskd);

/**
 * @brief    Print Allow List Entries available in memory to console
 */
void allow_list_print_all_entries_to_console(void);

/**
 * @brief Create unwrapped JSON response for all allow list entry in RAM
 *
 * @param input_object The input object to place the response into
 * @return int  Zero when allow list is empty (input_object unmodified)
 *              N>0  when N-entres are added
 *               <0  negative error code otherwise
 *
 */
int allowListJsonifyAll(cJSON *input_object);

/**
 * @brief Erase ALL allowlist entries
 *
 */
void allowListEraseAll(void);

/**
 * @brief  Start the On-Mesh commissioner functionality
 * @param[in]  aInstance Openthread instance
 *
 * @return OT_ERROR_NONE           Successfully started the commissioner service.
 *         OT_ERROR_ALREADY        Commissioner is already started.
 *         OT_ERROR_INVALID_STATE  Device is not currently attached to a network.
 */
otError allowListCommissionerStart(otInstance *aInstance);

/**
 * @brief This function posts to the OpenThread queue a task to stop the commissioner
 *
 * @return OT_ERROR_NONE
 */
otError allowListCommissionerStopPost(void);

/**
 * @brief This function returns the number of active joiners in the allow list
 *
 * @return The number of active joiners in the allow list.
 */
uint8_t allowListActiveJoinersCountGet(void);

otError allowListEntryJoinStatusGet(const otExtAddress *eui64);

#ifdef __cplusplus
}
#endif

#endif /* EXTENSIONS_COMMISSIONER_ALLOW_LIST_HPP_ */
