project(tasksys-test CXX)

file(GLOB SOURCES *.cxx)
add_executable(tasksys-test ${SOURCES})
target_link_libraries(tasksys-test PUBLIC tasksys)
target_compile_features(tasksys-test PUBLIC cxx_std_26)
