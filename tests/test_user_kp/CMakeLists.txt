cmake_minimum_required(VERSION 2.6.4)

project(test_user_kp)

include(FindPkgConfig)

pkg_search_module(smartslog REQUIRED smartslog)

include_directories(${smartslog_INCLUDE_DIRS})

link_directories(${smartslog_LIBRARY_DIRS})

add_executable(test_user_kp 
    main.c)

target_link_libraries(test_user_kp commons ontology ${smartslog_LIBRARIES} ${CURL_LIBRARIES})
