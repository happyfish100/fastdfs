cmake_minimum_required(VERSION 3.12)

# Basic usage example
add_executable(basic_usage basic_usage.cpp)
target_link_libraries(basic_usage fastdfs-client)

# Metadata example
add_executable(metadata_example metadata_example.cpp)
target_link_libraries(metadata_example fastdfs-client)

# Appender example
add_executable(appender_example appender_example.cpp)
target_link_libraries(appender_example fastdfs-client)

# Batch operations example
add_executable(batch_operations_example batch_operations_example.cpp)
target_link_libraries(batch_operations_example fastdfs-client)

