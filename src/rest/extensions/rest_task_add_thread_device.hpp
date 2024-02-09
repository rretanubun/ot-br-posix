#ifndef REST_TASK_ADD_THREAD_DEVICE_HPP_
#define REST_TASK_ADD_THREAD_DEVICE_HPP_

#include "rest_server_common.hpp"
#include "rest_task_handler.hpp"
#include "rest_task_queue.hpp"

struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <string.h>

extern const char         *taskNameAddThreadDevice;
cJSON                     *jsonify_add_thread_device_task(task_node_t *task_node);
uint8_t                    validate_add_thread_device_task(cJSON *task);
rest_actions_task_result_t process_add_thread_device_task(task_node_t *task_node, otInstance *aInstance);
rest_actions_task_result_t evaluate_add_thread_device_task(task_node_t *task_node);
rest_actions_task_result_t clean_add_thread_device_task(task_node_t *task_node, otInstance *aInstance);

#ifdef __cplusplus
} // end of extern "C"
#endif

#endif // REST_TASK_ADD_THREAD_DEVICE_HPP_
