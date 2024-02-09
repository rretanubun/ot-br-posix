#include "rest_task_handler.hpp"
#include "rest_task_queue.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <cJSON.h>
#include <stdlib.h>
#include <time.h>

static CJSON_PUBLIC(cJSON_bool)
    cJSON_AddOrReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    if (cJSON_HasObjectItem(object, string))
    {
        return cJSON_ReplaceItemInObjectCaseSensitive(object, string, newitem);
    }
    else
    {
        return cJSON_AddItemToObject(object, string, newitem);
    }
}

task_node_t *task_node_new(cJSON *task)
{
    task_node_t *task_node = (task_node_t *)calloc(1, sizeof(task_node_t)); // TODO-SPAR11:free memory
    assert(NULL != task_node);

    // Duplicate the client data associated with this task
    task_node->task = cJSON_Duplicate(task, cJSON_True);

    // Initialize the data for this new task to known defaults
    //
    task_node->prev       = NULL;  // Task queue management will update this
    task_node->next       = NULL;  // Task queue management will update this
    task_node->deleteTask = false; // New tasks are not marked for deletion

    // Populate UUID
    uuid_generate_random(&task_node->id);
    uuid_unparse(task_node->id, task_node->id_str);
    otbrLogWarning("creating new task with id %s", task_node->id_str);
    cJSON_AddStringToObject(task_node->task, "id", task_node->id_str);

    // Populated task type by name matching
    cJSON *task_type = cJSON_GetObjectItemCaseSensitive(task_node->task, "type");
    task_type_id_from_name(task_type->valuestring, &task_node->type);

    // Populate task creation time
    int timestamp      = (int)time(NULL);
    task_node->created = timestamp;

    // Setup task timeout if provided
    cJSON *attributes = cJSON_GetObjectItemCaseSensitive(task_node->task, "attributes");
    cJSON *timeout    = cJSON_GetObjectItemCaseSensitive(attributes, "timeout");

    if (cJSON_IsNumber(timeout))
    {
        // Set Up Timeout
        task_node->timeout = timestamp + (int)(timeout->valueint);
    }
    else
    {
        task_node->timeout = ACTIONS_TASK_NO_TIMEOUT;
    }

    // @note: While we can call task_update_status() here, we maybe locking
    // using taskNodeLockAcquire needlessly just to initialize the status
    // of a new task to known defaults (i.e. pending)
    //
    // Setup task status to pending (both the enum and the string version)
    task_update_status(task_node, ACTIONS_TASK_STATUS_PENDING);
    if (NULL != attributes)
    {
        (void)cJSON_AddItemToObject(attributes, "status",
                                    cJSON_CreateString(rest_actions_task_status_s[ACTIONS_TASK_STATUS_PENDING]));
    }
    // Return the prepared task node
    return task_node;
}

void task_update_status(task_node_t *aTaskNode, rest_actions_task_status_t status)
{
    assert(NULL != aTaskNode);

    taskNodeLockAcquire(LOCK_TYPE_BLOCKING, 0);
    aTaskNode->status = status;
    taskNodeLockRelease();
}

bool can_remove_task(task_node_t *aTaskNode)
{
    assert(NULL != aTaskNode);

    return (ACTIONS_TASK_STATUS_COMPLETED == aTaskNode->status || ACTIONS_TASK_STATUS_STOPPED == aTaskNode->status ||
            ACTIONS_TASK_STATUS_FAILED == aTaskNode->status);
}

cJSON *task_node_to_json(task_node_t *task_node)
{
    if (NULL == task_node)
    {
        return NULL;
    }
    cJSON *task_json  = cJSON_Duplicate(task_node->task, cJSON_True);
    cJSON *task_attrs = cJSON_GetObjectItemCaseSensitive(task_json, "attributes");
    cJSON_AddOrReplaceItemInObjectCaseSensitive(task_attrs, "status",
                                                cJSON_CreateString(rest_actions_task_status_s[task_node->status]));
    return task_json;
}

#ifdef __cplusplus
}
#endif
