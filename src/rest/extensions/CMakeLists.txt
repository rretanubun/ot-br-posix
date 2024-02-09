#
# MMB Research Inc (c) 2023. All rights reserved.
#

add_library(otbr-rest-extension
    rest_task_add_thread_device.cpp
    rest_task_handler.cpp
    rest_task_queue.cpp
    rest_task_uuid.cpp
    rest_server_common.cpp
    commissioner_allow_list.cpp
    pthread_lock.cpp
    parse_cmdline.cpp
    error.cpp
)

target_link_libraries(otbr-rest-extension
    PUBLIC
        http_parser
    PRIVATE
        cjson
        otbr-config
        otbr-utils
        openthread-ftd
        openthread-posix
)
