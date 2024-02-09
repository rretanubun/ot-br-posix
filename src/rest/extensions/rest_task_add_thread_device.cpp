#include "rest_task_add_thread_device.hpp"
#include "rest/extensions/commissioner_allow_list.hpp"
#include "rest/extensions/pthread_lock.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <openthread/commissioner.h>
#include <openthread/logging.h>
#include <openthread/platform/radio.h>

#define COMMISSIONER_JOINER_ADD_TASK_SIZE 2048
#define COMMISSIONER_JOINER_ADD_TASK_PRIORITY 5
#define COMMISSIONER_RESULT_MAX_WAIT_MS 30000

/**
 * @brief This flag acts a a mutex to make sure that
 * process_add_thread_device_task() only process one thread device
 * addition at a time.
 *
 */
static bool sJoinerAddOngoing = false;

// @note: Accommodating customer naming convention without refactoring whole codebase
static const char *const ATTRIBUTE_PSKD = "joinCred";

typedef struct
{
    otExtAddress mEui64;
    char        *mPskd;
    uint32_t     mTimeout;
    task_node_t *mTaskNode;
} pending_joiner_t;

static pending_joiner_t sPendingJoiner;

const char *taskNameAddThreadDevice = "addThreadDeviceTask";

cJSON *jsonify_add_thread_device_task(task_node_t *task_node)
{
    cJSON *task_json        = task_node_to_json(task_node);
    cJSON *attributes       = cJSON_GetObjectItemCaseSensitive(task_json, "attributes");
    cJSON *hasActivationKey = cJSON_GetObjectItemCaseSensitive(attributes, "hasActivationKey");
    cJSON_DeleteItemFromObject(hasActivationKey, ATTRIBUTE_PSKD);
    return task_json;
}

uint8_t validate_add_thread_device_task(cJSON *task)
{
    otbrLogWarning("%s", __func__);

    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(task, "timeout");
    if (NULL == timeout || !cJSON_IsNumber(timeout))
    {
        otbrLogWarning("%s:%d %s missing timeout field\n%s", __FILE__, __LINE__, taskNameAddThreadDevice,
                       cJSON_Print(task));
        return ACTIONS_TASK_INVALID;
    }

    cJSON *activation_key = cJSON_GetObjectItemCaseSensitive(task, "hasActivationKey");
    if (NULL == activation_key || !cJSON_IsObject(activation_key))
    {
        otLogWarnPlat("%s:%d %s missing hasActivationKey field\n%s", __FILE__, __LINE__, taskNameAddThreadDevice,
                      cJSON_Print(task));
        return ACTIONS_TASK_INVALID;
    }

    cJSON *eui = cJSON_GetObjectItemCaseSensitive(activation_key, "eui");
    if (NULL == eui || !cJSON_IsString(eui) || 16 != strlen(eui->valuestring) || !is_hex_string(eui->valuestring))
    {
        otLogWarnPlat("%s:%d %s missing or bad value in eui field\n%s", __FILE__, __LINE__, taskNameAddThreadDevice,
                      cJSON_Print(task));
        return ACTIONS_TASK_INVALID;
    }

    cJSON *pskd = cJSON_GetObjectItemCaseSensitive(activation_key, ATTRIBUTE_PSKD);
    if (NULL == pskd || !cJSON_IsString(pskd) || WPANSTATUS_OK != joiner_verify_pskd(pskd->valuestring))
    {
        otLogWarnPlat("%s:%d %s missing or bad value in %s field\n%s", __FILE__, __LINE__, taskNameAddThreadDevice,
                      ATTRIBUTE_PSKD, cJSON_Print(task));
        return ACTIONS_TASK_INVALID;
    }

    return ACTIONS_TASK_VALID;
}

/**
 * @brief Pending commissioner start result, add a thread joiner.
 *
 * This task is created after an initial call to "otCommissionerStart", once created the task will wait until a
 * commissioner state is OT_COMMISSIONER_STATE_ACTIVE or timeout. During this time the task "status" will be set to
 * "ACTIONS_TASK_STATUS_PENDING" to avoid unwanted evaluation by the task queue, and "sJoinerAddOngoing" will be
 * maintained at "true", so that other pending joiner additions are prorated.
 *
 * Once a commissioner is active, this task is notified with the result, on success,
 * joiners will be added.
 *
 */
void *restTaskJoinerAddConditionalTask(void *threadArg)
{
    otInstance         *aInstance          = (otInstance *)threadArg;
    otError             error              = OT_ERROR_FAILED;
    otCommissionerState commissionerResult = OT_COMMISSIONER_STATE_DISABLED;
    struct timespec ts = {1, 0}, end = {0, 0}, now = {0, 0}; //{sec,ns}

    // If task status is ACTIVE, make it PENDING so to avoid needless evaluation
    // We will then wait for the ot-commissioner to start and handle switching
    // task status to ACTIVE/FAILED depending on the status it returns back.
    if (sPendingJoiner.mTaskNode->status == ACTIONS_TASK_STATUS_ACTIVE)
    {
        task_update_status(sPendingJoiner.mTaskNode, ACTIONS_TASK_STATUS_PENDING);
    }

    // Calculate end time (10 seconds from now)
    clock_gettime(CLOCK_REALTIME, &end);
    end.tv_sec += 10;

    while (1)
    {
        // one sec delay
        nanosleep(&ts, NULL);
        // checks the commissioner state
        openthread_lock_acquire(LOCK_TYPE_BLOCKING, 0);
        commissionerResult = otCommissionerGetState(aInstance);
        openthread_lock_release();

        // Check if commissionerResult is ACTIVE
        if (commissionerResult == OT_COMMISSIONER_STATE_ACTIVE)
        {
            break; // Exit loop if condition is met
        }
        // Check for 10-second timeout
        clock_gettime(CLOCK_REALTIME, &now);
        if ((now.tv_sec > end.tv_sec) || (now.tv_sec == end.tv_sec && now.tv_nsec > end.tv_nsec))
        {
            break; // Exit loop if 10 seconds have passed
        }
    }

    // Either signal is received, or the wait timed out...
    // in any case lets see if we can add ot-joiners

    // If the commissioner is still disabled at this time (not expected)
    // lets mark this task as failed, the next joiner in the list (if any)
    // will have to try to start the commissioner again.
    if (OT_COMMISSIONER_STATE_DISABLED == commissionerResult)
    {
        // Commissioner is not able to be started, mark this joiner as failed
        otLogWarnPlat("%s: commissioner not ready, joiner failed", __func__);
        task_update_status(sPendingJoiner.mTaskNode, ACTIONS_TASK_STATUS_FAILED);
        goto exit; // clean up and returns
    }

    // Getting here means the commissioner is active, we can add this joiner
    task_update_status(sPendingJoiner.mTaskNode, ACTIONS_TASK_STATUS_ACTIVE);
    error =
        allowListCommissionerJoinerAdd(sPendingJoiner.mEui64, sPendingJoiner.mTimeout, sPendingJoiner.mPskd, aInstance);
    if (OT_ERROR_NONE != error)
    {
        otLogWarnPlat("allowListCommissionerJoinerAddCpp ERR");
    }

exit:
    sJoinerAddOngoing = false; // Set this to false to let other joiners be processed
    pthread_exit(NULL);        // Do This LAST in this task
    return NULL;
}

rest_actions_task_result_t process_add_thread_device_task(task_node_t *task_node, otInstance *aInstance)
{
    otbrLogWarning("%s", __func__);
    int             err = -1;
    otCommissionerState otCommissionerState = OT_COMMISSIONER_STATE_DISABLED;
    pthread_t           sJoinerAddConditionalHandle;

    // Arg check before doing works
    if (NULL == task_node || NULL == task_node->task)
    {
        otbrLogWarning("%s: called with NULL args", __func__);
        return ACTIONS_RESULT_FAILURE;
    }

    // If we are in the middle of adding a device to the ot-commissioner, do not process another entry
    if (true == sJoinerAddOngoing)
    {
        otbrLogWarning("%s: ot-joiner add already ongoing, retry", __func__);
        // return ACTIONS_RESULT_RETRY; //TODO-SPAR11
        return ACTIONS_RESULT_FAILURE;
    }
    else
    {
        sJoinerAddOngoing = true;
    }

    cJSON *task            = task_node->task;
    cJSON *task_attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *activation_key  = cJSON_GetObjectItemCaseSensitive(task_attributes, "hasActivationKey");
    cJSON *eui             = cJSON_GetObjectItemCaseSensitive(activation_key, "eui");
    cJSON *pskd            = cJSON_GetObjectItemCaseSensitive(activation_key, ATTRIBUTE_PSKD);
    cJSON *timeout         = cJSON_GetObjectItemCaseSensitive(task_attributes, "timeout");

    otError      error = OT_ERROR_NONE;
    otExtAddress eui64 = {0};

    //  convert a string of hexadecimal characters (eui64) into an array of bytes (uint8_t)
    error = str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);
    if (OT_ERROR_NONE != error)
    {
        sJoinerAddOngoing = false;
        return ACTIONS_RESULT_FAILURE;
    }

    if (NULL == timeout || !cJSON_IsNumber(timeout))
    {
        otLogWarnPlat("Invalid timeout value");
        sJoinerAddOngoing = false;
        return ACTIONS_RESULT_FAILURE;
    }
    error = allowListCommissionerStart(aInstance);
    if (OT_ERROR_INVALID_STATE == error)
    {
        otLogWarnPlat("Failed to start the commissioner, error %d", error);
        sJoinerAddOngoing = false;
        return ACTIONS_RESULT_FAILURE;
    }
    // Get the commissioner state
    openthread_lock_acquire(LOCK_TYPE_BLOCKING, 0);
    otCommissionerState = otCommissionerGetState(aInstance);
    openthread_lock_release();

    // If the commissioner is already ACTIVE, we can add ot-joiners right away
    if (OT_COMMISSIONER_STATE_ACTIVE == otCommissionerState)
    {
        error = allowListCommissionerJoinerAdd(eui64, (uint32_t)timeout->valueint, pskd->valuestring, aInstance);
        if (OT_ERROR_NONE != error)
        {
            otLogWarnPlat("allowListCommissionerJoinerAddCpp ERR");
            sJoinerAddOngoing = false;
            return ACTIONS_RESULT_FAILURE;
        }
        sJoinerAddOngoing = false;
        return ACTIONS_RESULT_SUCCESS;
    }

    // Getting here means ot-commissioner is not ACTIVE yet, so we need to make a thread
    // to wait for the ot-commissioner to BE ACTIVE, so we can add the FIRST joiner
    // once ot-commissioner is ACTIVE
    sPendingJoiner.mEui64    = eui64;
    sPendingJoiner.mPskd     = pskd->valuestring;
    sPendingJoiner.mTimeout  = (uint32_t)timeout->valueint;
    sPendingJoiner.mTaskNode = task_node;
    // this creates a thread that waits and add the joiner once the commissioner is active
    err = pthread_create(&sJoinerAddConditionalHandle, NULL, restTaskJoinerAddConditionalTask, (void *)aInstance);

    if (err != 0)
    {
        otLogCritPlat("Cannot create restTaskJoinerAddConditionalTask");
        sJoinerAddOngoing = false;
        return ACTIONS_RESULT_FAILURE;
    }

    // @note: on this exit path we do not set sJoinerAddOngoing = false
    // since the restTaskJoinerAddConditionalTask is running and it will
    // set sJoinerAddOngoing = false when it is done.
    return ACTIONS_RESULT_SUCCESS;
}

rest_actions_task_result_t evaluate_add_thread_device_task(task_node_t *task_node)
{
    otbrLogWarning("%s", __func__);

    return ACTIONS_RESULT_SUCCESS;

    // @note: Evaluate functionality is by passed for now.
    // when we return ACTIONS_RESULT_SUCCESS, our caller will mark it as complete in our task_node.

    // TODO-SPAR11: Add evaluate functionality

    cJSON *task            = task_node->task;
    cJSON *task_attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *activation_key  = cJSON_GetObjectItemCaseSensitive(task_attributes, "hasActivationKey");
    cJSON *eui             = cJSON_GetObjectItemCaseSensitive(activation_key, "eui");

    otError error = OT_ERROR_NONE;

    otExtAddress eui64 = { 0 };
    error = str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);
    if (OT_ERROR_NONE != error)
    {
        return ACTIONS_RESULT_FAILURE;
    }

    const otExtAddress *addrPtr = &eui64;
    error                       = allowListEntryJoinStatusGet(addrPtr);
    if (OT_ERROR_FAILED == error)
    {
        return ACTIONS_RESULT_FAILURE;
    }
    if (OT_ERROR_NONE == error)
    {
        // This means entry is joined, it is now safe to delete
        // when we return ACTIONS_RESULT_SUCCESS, our caller will mark it as complete in our task_node.
        (void)allowListEntryErase(eui64);
        return ACTIONS_RESULT_SUCCESS;
    }

    // Don't need to check for OT_ERROR_PENDING as the task is currently pending anyway
    return ACTIONS_RESULT_PENDING;
}

rest_actions_task_result_t clean_add_thread_device_task(task_node_t *task_node, otInstance *aInstance)
{
    cJSON *task            = task_node->task;
    cJSON *task_attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    cJSON *activation_key  = cJSON_GetObjectItemCaseSensitive(task_attributes, "hasActivationKey");
    cJSON *eui             = cJSON_GetObjectItemCaseSensitive(activation_key, "eui");

    otError error = OT_ERROR_NONE;

    otExtAddress eui64 = { 0 };
    error = str_to_m8(eui64.m8, eui->valuestring, OT_EXT_ADDRESS_SIZE);
    if (OT_ERROR_NONE != error)
    {
        return ACTIONS_RESULT_FAILURE;
    }

    error = allowListCommissionerJoinerRemove(eui64, aInstance);
    allowListEntryErase(eui64);

    if (OT_ERROR_NONE == error)
    {
        return ACTIONS_RESULT_SUCCESS;
    }
    return ACTIONS_RESULT_FAILURE;
}

#ifdef __cplusplus
}
#endif
