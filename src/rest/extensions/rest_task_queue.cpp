
#include "rest_task_queue.hpp"
#include "rest_task_add_thread_device.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <cJSON.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <openthread/logging.h>

#define EVALUATE_INTERVAL 10

task_node_t    *task_queue     = NULL;
uint8_t         task_queue_len = 0;
pthread_mutex_t sTaskNodeLock;

typedef struct
{
    rest_actions_task_t type_id;
    const char        **type_name;
    task_jsonifier      jsonify;
    task_validator      validate;
    task_processor      process;
    task_evaluator      evaluate;
    task_cleaner        clean;
} task_handlers_t;

static task_handlers_t *task_handler_by_task_type_id(rest_actions_task_t type_id);

/**
 * This list contains the handlers for each type of task, it must define the
 * tasks in the same order as the defined id from `rest_actions_task_t`. It must
 * also define all of the tasks (though ACTIONS_TASKS_SIZE must not have an
 * associated task as it's a counter).
 *
 * If these contraints are not met, it will assert during startup.
 */
static task_handlers_t handlers[] = {

    {
        .type_id   = ADD_THREAD_DEVICE_TASK,
        .type_name = &taskNameAddThreadDevice,
        .jsonify   = jsonify_add_thread_device_task,
        .validate  = validate_add_thread_device_task,
        .process   = process_add_thread_device_task,
        .evaluate  = evaluate_add_thread_device_task,
        .clean     = clean_add_thread_device_task,
    },
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/**
 * @brief Finds a task_handlers_t struct for a specific type id if it exists.
 *
 * @return a task_handlers_t pointer for the specified type id, or NULL if one
 *  could not be found.
 */
static task_handlers_t *task_handler_by_task_type_id(rest_actions_task_t type_id)
{
    if (type_id < ACTIONS_TASKS_SIZE)
    {
        return &handlers[type_id];
    }
    return NULL;
}

cJSON *task_to_json(task_node_t *aTaskNode)
{
    if (NULL == aTaskNode || NULL == aTaskNode->task)
    {
        return NULL;
    }
    // TODO-SPAR11: Add Lock
    task_handlers_t *handlers = task_handler_by_task_type_id(aTaskNode->type);
    if (NULL == handlers || NULL == handlers->jsonify)
    {
        return NULL;
    }
    return handlers->jsonify(aTaskNode);
}

task_node_t *task_node_find_by_id(uuid_t uuid)
{
    task_node_t *head = task_queue;
    while (NULL != head)
    {
        if (uuid_equals(uuid, head->id))
        {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

uint8_t can_remove_task_max()
{
    uint8_t      can_remove = 0;
    task_node_t *head       = task_queue;
    while (NULL != head)
    {
        if (can_remove_task(head))
        {
            can_remove++;
        }
        head = head->next;
    }
    return can_remove;
}

static bool remove_oldest_non_running_task()
{
    int             timestamp = (int)time(NULL);
    struct timespec ts;
    ts.tv_sec  = 0;         // Seconds
    ts.tv_nsec = 10000000L; // Nanoseconds (10 millisecond)
    task_node_t *head;
    head                          = task_queue;
    task_node_t *task_node_delete = NULL;

    while (NULL != head)
    {
        // Find the oldest task by finding the smallest timestamp
        if (timestamp > head->created && can_remove_task(head))
        {
            timestamp        = head->created;
            task_node_delete = head;
        }
        head = head->next;
    }

    if (NULL != task_node_delete)
    {
        // we don't call task_update_status as the task should delete shortly
        // after this
        task_node_delete->status     = ACTIONS_TASK_STATUS_STOPPED;
        task_node_delete->deleteTask = true;
        nanosleep(&ts, NULL); // 10 millisecond delay
        return true;
    }

    return false;
}

uint8_t validate_task(cJSON *task)
{
    if (NULL == task)
    {
        return ACTIONS_TASK_INVALID;
    }
    otLogWarnPlat("Validating task: %s", cJSON_PrintUnformatted(task));

    cJSON *task_type = cJSON_GetObjectItemCaseSensitive(task, "type");
    if (NULL == task_type || !cJSON_IsString(task_type))
    {
        otLogWarnPlat("%s:%d task missing type field", __FILE__, __LINE__);
        return ACTIONS_TASK_INVALID;
    }
    cJSON *task_attributes = cJSON_GetObjectItemCaseSensitive(task, "attributes");
    if (NULL == task_attributes || !cJSON_IsObject(task_attributes))
    {
        otLogWarnPlat("%s:%d task missing attributes field", __FILE__, __LINE__);
        return ACTIONS_TASK_INVALID;
    }

    cJSON *timeout = cJSON_GetObjectItemCaseSensitive(task_attributes, "timeout");
    if (NULL != timeout && !cJSON_IsNumber(timeout))
    {
        otLogWarnPlat("%s:%d task has invalid timeout field", __FILE__, __LINE__);
        return ACTIONS_TASK_INVALID;
    }

    rest_actions_task_t task_type_id = ACTIONS_TASKS_SIZE;
    if (task_type_id_from_name(task_type->valuestring, &task_type_id))
    {
        if (task_type_id >= ACTIONS_TASKS_SIZE || NULL == handlers[task_type_id].validate)
        {
            otLogWarnPlat("Could not find a validate handler for %d", task_type_id);
            return ACTIONS_TASK_INVALID;
        }
        return handlers[task_type_id].validate(task_attributes);
    }

    return ACTIONS_TASK_INVALID;
}

bool queue_task(cJSON *task, uuid_t *task_id)
{
    otbrLogWarning("Queueing task: %s", cJSON_PrintUnformatted(task));
    if (TASK_QUEUE_MAX <= task_queue_len)
    {
        if (!remove_oldest_non_running_task())
        {
            // Note: This case should not be possible as we already check to see if we exceed queue max before getting
            // to this queue fcn
            otLogWarnPlat(
                "Maximum number of tasks hit, and no completed task available for removal, not queueing task");
            return false;
        }
    }
    // Generate the task object, and copy the ID to the output
    task_node_t *task_node = task_node_new(task);
    memcpy(task_id, &(task_node->id), sizeof(uuid_t));

    if (NULL == task_queue)
    {
        task_queue     = task_node;
        task_queue_len = 1;
    }
    else
    {
        task_node_t *head = task_queue;
        while (NULL != head->next)
        {
            head = head->next;
        }
        head->next      = task_node;
        task_node->prev = head;
        task_queue_len++;
    }
    return true;
}

static void process_task(task_node_t *task_node, otInstance *aInstance)
{
    if (NULL == task_node)
    {
        return;
    }
    if (ACTIONS_TASK_STATUS_PENDING != task_node->status)
    {
        return;
    }

    task_update_status(task_node, ACTIONS_TASK_STATUS_ACTIVE);

    task_handlers_t *handlers = task_handler_by_task_type_id(task_node->type);
    if (NULL == handlers || NULL == handlers->process)
    {
        otLogWarnPlat("Could not find a process handler for %d", task_node->type);
        return;
    }

    rest_actions_task_result_t processed = handlers->process(task_node, aInstance);
    if (ACTIONS_RESULT_FAILURE == processed)
    {
        task_update_status(task_node, ACTIONS_TASK_STATUS_FAILED);
    }
    else if (ACTIONS_RESULT_RETRY == processed)
    {
        task_update_status(task_node, ACTIONS_TASK_STATUS_PENDING);
    }
}

static void evaluate_task(task_node_t *task_node)
{
    if (NULL == task_node)
    {
        return;
    }
    if (ACTIONS_TASK_STATUS_ACTIVE != task_node->status)
    {
        return;
    }

    // TODO-SPAR11
    //  if (EVALUATE_INTERVAL > ((int)time(NULL) - task_node->last_evaluated)) {
    //      otbrLogWarning("ERROR In evaluate_task:EVALUATE_INTERVAL ");
    //      return;
    //  }

    task_handlers_t *handlers = task_handler_by_task_type_id(task_node->type);
    if (NULL == handlers || NULL == handlers->evaluate)
    {
        otbrLogWarning("Could not find an evaluate handler for %d", task_node->type);
        return;
    }

    rest_actions_task_result_t result = handlers->evaluate(task_node);
    if (ACTIONS_RESULT_SUCCESS == result)
    {
        task_update_status(task_node, ACTIONS_TASK_STATUS_COMPLETED);
    }
    else if (ACTIONS_RESULT_FAILURE == result)
    {
        task_update_status(task_node, ACTIONS_TASK_STATUS_FAILED);
    }

    // else ACTIONS_RESULT_PENDING, but we don't need to change task status as it will just re-evaluate later
    task_node->last_evaluated = (int)time(NULL);
}

cJSON *jsonCreateTaskMetaCollection(uint32_t aOffset, uint32_t aLimit)
{
    cJSON *meta            = cJSON_CreateObject();
    cJSON *meta_collection = cJSON_CreateObject();
    // Abort if we are unable to create the necessary JSON objects
    if (NULL == meta || NULL == meta_collection)
    {
        return NULL;
    }

    (void)cJSON_AddNumberToObject(meta_collection, "offset", aOffset);
    (void)cJSON_AddNumberToObject(meta_collection, "limit", aLimit);
    (void)cJSON_AddNumberToObject(meta_collection, "total", task_queue_len);
    (void)cJSON_AddItemToObject(meta, "collection", meta_collection);
    return meta;
}

uint8_t taskNodeLockAcquire(LockType lock_type, long timeout_ms)
{
    struct timespec timeout = { 0, 0 };
    switch (lock_type)
    {
    case LOCK_TYPE_BLOCKING:
        // Blocking lock
        return pthread_mutex_lock(&sTaskNodeLock);
        break;

    case LOCK_TYPE_NONBLOCKING:
        // Non-blocking lock
        return pthread_mutex_trylock(&sTaskNodeLock);
        break;

    case LOCK_TYPE_TIMED:
        // Timed lock
        clock_gettime(CLOCK_REALTIME, &timeout); // Get current time
        timeout.tv_nsec += (timeout_ms * 1000);
        // Ensure that tv_nsec doesn't exceed 1 second (1e9 nanoseconds).
        if (timeout.tv_nsec >= 1000000000)
        {
            timeout.tv_sec += timeout.tv_nsec / 1000000000;
            timeout.tv_nsec %= 1000000000;
        }
        return pthread_mutex_timedlock(&sTaskNodeLock, &timeout);
        break;

    default:
        return -1;
    }

    return -1;
}

uint8_t taskNodeLockRelease(void)
{
    uint8_t lock_status;
    lock_status = pthread_mutex_unlock(&sTaskNodeLock);
    return lock_status;
}

uint8_t taskNodeLockInit(void)
{
    uint8_t lock_status;
    lock_status = pthread_mutex_init(&sTaskNodeLock, NULL);
    return lock_status;
}

uint8_t taskNodeLockDeinit(void)
{
    uint8_t lock_status;
    lock_status = pthread_mutex_destroy(&sTaskNodeLock);
    return lock_status;
}

/**
 * @brief The main function that iterates through the task_queue and process each task
 *        High level processing steps:
 *
 *        1. Delete any tasks that are marked for deletion
 *        2. Process any PENDING or ACTIVE tasks
 *           3.1 If task is timed out, the task is marked STOPPED (and deleted)
 *           3.2 If task is PENDING, call its process() function to make it ACTIVE
 *           3.3 If task is ACTIVE, call its evaluate() function to see if it is PENDING|SUCCESS|FAILED
 *
 */
static void *rest_task_queue_task(void *threadArg)
{
    otInstance     *instance = (otInstance *)threadArg;
    task_node_t    *head     = task_queue;
    struct timespec ts;
    ts.tv_sec  = 0;        // Seconds
    ts.tv_nsec = 1000000L; // Nanoseconds (1 millisecond)

    while (1)
    {
        // TODO-SPAR11
        // 1. Acquire task node lock before processing any task
        // 2. Release task node lock before leaving the iterator loop
        // 3. Add thread exit logic
        //  taskNodeLockAcquire(LOCK_TYPE_BLOCKING,0);
        //  if (TASK_SHUTDOWN_NOTIFICATION_INDEX == 0)
        //  {
        //      taskNodeLockRelease();
        //      break;
        //  }
        //  taskNodeLockRelease();

        if (NULL == head)
        {
            // Hit end of queue, wrap around to the start
            // Continue in case the queue is empty so we catch that here
            head = task_queue;
            nanosleep(&ts, NULL); // 1 millisecond delay
            continue;
        }

        // Is this task marked for deletion?
        if (head->deleteTask)
        {
            if (ACTIONS_TASK_STATUS_STOPPED != head->status)
            {
                // we don't call task_update_status as we're going to be
                // deleting this a few lines below.
                head->status = ACTIONS_TASK_STATUS_STOPPED;
            }

            task_handlers_t *handlers = task_handler_by_task_type_id(head->type);
            if (NULL == handlers || NULL == handlers->clean)
            {
                otLogWarnPlat("Could not find a clean handler for %d, assuming no clean needed", head->type);
            }
            else
            {
                // calls the clean function defined in handlers[]
                handlers->clean(head, instance);
            }

            task_node_t *next = head->next;
            if (NULL == head->prev)
            {
                // If prev is empty, then we are the start of the list
                task_queue = next;
                if (NULL != next)
                {
                    next->prev = NULL;
                }
            }
            else
            {
                head->prev->next = next;
                if (NULL != next)
                {
                    next->prev = head->prev;
                }
            }
            {
                // Delete the cJSON task as well as the task_node
                otbrLogInfo("Deleting task id %s", head->id_str);
                cJSON_Delete(head->task);
                head->task = NULL;
                free(head);
                if (task_queue_len > 0)
                {
                    task_queue_len--;
                }
            }

            head = next;
            continue;
        }

        // Is this task PENDING or ACTIVE?
        if (ACTIONS_TASK_STATUS_PENDING == head->status || ACTIONS_TASK_STATUS_ACTIVE == head->status)
        {
            // Check if task has timed out if se we need to clean it and  mark it as stopped
            // We do not delete the task because the GET handler want to keep tabs on what is happening to the tasks.
            int current_time = (int)time(NULL);
            if (head->timeout >= 0 && head->timeout < current_time)
            {
                /* Mark tasks that have timed-out without failing/being completed as "Stopped" and stop evaluating */
                otbrLogWarning("task timed out %s", cJSON_PrintUnformatted(head->task));
                task_handlers_t *handlers = task_handler_by_task_type_id(head->type);
                if (NULL == handlers || NULL == handlers->clean)
                {
                    otLogWarnPlat("Could not find a clean handler for %d, assuming no clean needed", head->type);
                }
                else
                {
                    handlers->clean(head, instance);
                }

                task_update_status(head, ACTIONS_TASK_STATUS_STOPPED);
            }
            // If task has not timed out, carry on with its processing
            else
            {
                // If ACTIONS_TASK_STATUS_PENDING, run its process() function to see if we can make it active
                if (ACTIONS_TASK_STATUS_PENDING == head->status)
                {
                    process_task(head, instance);
                }
                // Else If ACTIONS_TASK_STATUS_ACTIVE, run its evaluate, to see if it is completed/failed
                else if (ACTIONS_TASK_STATUS_ACTIVE == head->status)
                {
                    evaluate_task(head);
                }
            }
        }
        // Get ready to process the next task in the queue
        head = head->next;
    }
    otbrLogWarning("EXITING rest_task_queue_task");
    pthread_exit(NULL);
    return NULL;
}

void rest_task_queue_task_init(otInstance *aInstance)
{
    int   ret = -1;
    pthread_t task_queue_handle;
    // Initialize the pthread mutex
    ret = taskNodeLockInit();
    if (ret != 0)
    {
        otLogWarnPlat("Error in taskNodeLockInit");
    }
    // Initialize the pthread mutex
    ret = openthread_lock_init();
    if (ret != 0)
    {
        otLogWarnPlat("Error in openthread_lock_init");
    }
    // As noted above, the handler list needs to have an entry for each
    // task type defined in `rest_actions_task_t
    assert(ARRAY_SIZE(handlers) > 0);
    assert(ARRAY_SIZE(handlers) == ACTIONS_TASKS_SIZE);

    // To optimize during runtime, we want to ensure that the list is ordered
    // and contains each of the entries. This allows us to just index in via
    // the task type id rather than having to iterate.
    //
    // This check iterates over the list an ensures that each entry has a
    // type_id which is exactly 1 greater than the previous entry.
    rest_actions_task_t previous_id = handlers[0].type_id;
    for (size_t idx = 1; idx < ARRAY_SIZE(handlers); idx++)
    {
        assert(previous_id + 1 == handlers[idx].type_id);
        previous_id = handlers[idx].type_id;
    }

    // This creates a thread that will process,evaluate and clean the task from task queue.
    ret = pthread_create(&task_queue_handle, NULL, rest_task_queue_task, (void *)aInstance);
    if (ret != 0)
    {
        otLogCritPlat("Cannot create rest_task_queue_task");
    }
}

bool task_type_id_from_name(const char *task_name, rest_actions_task_t *type_id)
{
    if (NULL == task_name || NULL == type_id)
    {
        return false;
    }

    for (size_t idx = 0; idx < ACTIONS_TASKS_SIZE; idx++)
    {
        size_t name_length = strlen(*handlers[idx].type_name);
        if (0 == strncmp(task_name, *handlers[idx].type_name, name_length))
        {
            *type_id = handlers[idx].type_id;
            return true;
        }
    }
    return false;
}

#ifdef __cplusplus
}
#endif
