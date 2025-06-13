project(tasksys-test CXX)

find_package(TBB REQUIRED)

file(GLOB SOURCES *.cxx)
file(GLOB HEADERS *.h)

add_executable(tasksys-test ${SOURCES} ${HEADERS})
target_link_libraries(tasksys-test PUBLIC tasksys TBB::tbb)
target_compile_features(tasksys-test PUBLIC cxx_std_26)
