# Inegration of AddThreadDeviceTask into ot-br-posix project

**Description**
In this project, We have extended the `REST` API functionality to add a new thread device using `POST` request.
It adds the following endpoints and methods:
`/api/actions` [POST] (`addThreadDeviceTask`)
The `rest/extensions` folder implements `api/action` type `addThreadDeviceTask`. A `POST` request on `api/action` is handled by its registered endpoint handler `ApiActionHandler()`, based on the method  `[POST | GET | DELETE | PUT]` it calls its respective function.
For e.g.: A `POST` request on `api/actions` with the type `addThreadDeviceTask`, is handled by `RestActionPostHandler()`.

Endpoint registration is handled by the `Resource` class from the `rest` module.

Handling of api/action:

Request & Response: After receiving the request, `RestActionPostHandler()` parses the request body using the `Request` class of the `rest` module.
We validate the received data before we attempt to perform processing. Once `JSON` data is extracted it is added into a newly created instance `task_node` of `struct type task_node_t` for further processing.
(`task_node` contains all the information about the task like its `status, uuid, task type, creation time, timeout`)
The response is sent back using the `Response` class of the `rest` module.

Processing of request: A `request` is processed on a separate thread (`rest_task_queue_task`). A POSIX thread (pthread) is created during the `rest_web_server`  initialization in `rest/rest_web_server.cpp`. This thread is responsible for Processing, evaluating, and cleaning of the `task_node`.
When the `task_node` is created, it is marked as `ACTIONS_TASK_STATUS_PENDING`  which is then processed by thread function `rest_task_queue_task`.

## Files Description

-`rest_task_queue.cpp`,`rest_task_queue.hpp`
Implements APIs related to thread creation and task handling.

-`rest_task_add_thread_device.cpp`, `rest_task_add_thread_device.hpp`
Implements the APIs that validate, process, evaluate, jsonify and clean the task.

-`rest_task_handler.cpp`, `rest_task_handler.hpp`
Implements additional functionailty like `task_node` creation, update task status and convert `task_node` to `JSON` format.
> **_NOTE:_** TODO: Move this into `rest_task_queue` files.

-`rest_task_uuid.cpp`,`rest_task_uuid.hpp`
Implements functionality to handle UUID (generation,parse,unparse) used for `task_node`.

-`rest_server_common.cpp`,`rest_server_common.hpp`
Implements APIs used for conversions of data from one form to another.

-`commissioner_allow_list.cpp`,`commissioner_allow_list.hpp`
Implements functionality related to commisioner.

-`pthread_lock.cpp`,`pthread_lock.hpp`
Implements functionality to handle pthreads API synchronization.
    
-`parse_cmdline.cpp`,`parse_cmdline.hpp`
This file includes definitions for command line parser.

-`error.cpp`,`error.hpp`
This file implements the error code functions used by OpenThread core modules.
> **_NOTE:_** TODO: Local copy of file included to simplify building code in isolation, should be removed before mainlining back to community


