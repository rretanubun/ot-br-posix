#ifndef REST_TASK_QUEUE_HPP_
#define REST_TASK_QUEUE_HPP_

#include "rest_server_common.hpp"
#include "rest_task_handler.hpp"
#include "rest/extensions/pthread_lock.hpp"
#include "utils/thread_helper.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_QUEUE_MAX 10

/**
 * @brief Specifies the function signature that the task jsonifier function
 *        must adhere to.
 *
 * A task jsonifier is responsible for taking a `task_node_t` pointer
 * and creating a JSON representation which can be returned to a
 * client.
 *
 * @param task_node the node which is to be jsonified
 * @return a cJSON pointer representing the task node.
 */
typedef cJSON *(*task_jsonifier)(task_node_t *task_node);

/**
 * @brief Specifies the function signature that the task validator function
 *        must adhere to.
 *
 * A task validator is responsible for taking the input cJSON from a user and
 * ensuring all the various fields and structures meet the requirements set out
 * in the API schema.
 *
 * See the `validate_task` function below for more info.
 *
 * @param task the JSON structure to return
 * @return the value must be one of ACTIONS_TASK_VALID ACTIONS_TASK_INVALID, or
 *         ACTIONS_TASK_NOT_IMPLEMENTED.
 */
typedef uint8_t (*task_validator)(cJSON *task);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task processor is responsible for starting the execution of a task. Once
 * the execution has started, the evaluation function is called regularly for
 * updates.
 *
 * @param task the task which is to be executed.
 * @param aInstance openthread instance
 * @return the status of the task, which should ACTIONS_RESULT_SUCCESS,
 *         ACTIONS_RESULT_FAILURE, ACTIONS_RESULT_RETRY, or
 *         ACTIONS_RESULT_PENDING.
 */
typedef rest_actions_task_result_t (*task_processor)(task_node_t *task_node, otInstance *aInstance);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task evaluator is responsible for continued execution and processing of
 * a task. This is responsible for monitoring the execution of a task and
 * reporting when the execution has finished (either successfully or in
 * failure).
 *
 * @param task the task which is being evaluated.
 * @return the status of the task, which should be ACTIONS_RESULT_SUCCESS,
 *         ACTIONS_RESULT_FAILURE, ACTIONS_RESULT_PENDING, or
 *         ACTIONS_RESULT_NO_CHANGE_REQUIRED.
 */
typedef rest_actions_task_result_t (*task_evaluator)(task_node_t *task_node);

/**
 * @brief Specifies the function signature that the task processor function
 *        must adhere to.
 *
 * A task cleaner is responsible for releasing any resources that the task
 * is holding so that it can be removed from the queue.
 *
 * @param task the task which is being cleaned
 * @param aInstance openthread instance
 * @return the status of the cleaning operation, which should be
 *         ACTIONS_RESULT_SUCCESS or ACTIONS_RESULT_FAILURE.
 */
typedef rest_actions_task_result_t (*task_cleaner)(task_node_t *task_node, otInstance *aInstance);

/**
 * @brief Validate the REST POST Action Task with the given JSON array
 *
 * @param task Pointer to cJSON task to be validated
 * @return ACTIONS_TASK_VALID if the task is valid,
 *         ACTIONS_TASK_INVALID if the task is invalid,
 *         ACTIONS_TASK_NOT_IMPLEMENTED if the task has not been implemented
 */
uint8_t      validate_task(cJSON *task);

/**
 * @brief Generates the new task object (task_node) of type 'task_node_t'.  
 *        Initializes task_queue with the newly created task object which will
 *        be proccessed on different thread.
 *
 * @param task A pointer to JSON array item.
 * @param uuid_t *task_id A reference to get the task_id
 * @return true     Task queued
*  @return false    Not able to queue task
 */
bool         queue_task(cJSON *task, uuid_t *task_id);
cJSON       *task_to_json(task_node_t *task_node);
task_node_t *task_node_find_by_id(uuid_t uuid);

/**
 * @brief When called, I generate a CJSON object for the task metadata
 * as specified in the siemens_target_rest_specification.openapi.yaml
 *
 * sample output:
 *
 * meta:
 *    collection:
 *        offset: 0 // based on the args passed to this function
 *        limit:  4 // based on the args passed to this function
 *        total:  4 // determined by the total number of tasks in the queue
 *
 * @param aOffset the value to use for meta.collection.offset
 * @param aLimit  the value to use for meta.collection.limit
 *
 * @return cJSON* a populated meta.collection json object, NULL on error
 */
cJSON *jsonCreateTaskMetaCollection(uint32_t aOffset, uint32_t aLimit);

uint8_t can_remove_task_max();

void rest_task_queue_task_init(otInstance *aInstance);

/**
 * @brief Looks up the type id for a given task name and updates the `type_id`
 *        argument if found.
 *
 * @param task_name the task name to look up the id for
 * @param type_id [out] a pointer to place the result into
 * @return true if found, false otherwise.
 */
bool task_type_id_from_name(const char *task_name, rest_actions_task_t *type_id);

/**
 * @brief Create the semaphore used to protect a task node against being accessed simultaneously
 *        by different tasks.
 *        Must be called before locks are used.
 *
 * @return 0:         Success
 *         non-zero:  Failed
 *
 */
uint8_t taskNodeLockInit(void);

/**
 * @brief Delete and nullify a task node lock
 */
uint8_t taskNodeLockDeinit(void);

/**
 * @brief Acquire lock before proceeding to access/modify a task node
 *        Must be used whenever task_node_t* aNode is used
 *
 * @param[in] LockType lock_type Select the lock type [blocking,non-blocking,timed]
 * @param[in] long timeout_sec   timeout value in ms
 *
 * @return 0:         Success
 *         non-zero:  Failed
 *
 */
uint8_t taskNodeLockAcquire(LockType lock_type, long timeout_ms);

/**
 * @brief Release lock and make available so that other tasks can use the task node
 *
 * @return 0:         Success
 *        non-zero:  Failed
 */
uint8_t taskNodeLockRelease(void);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif
