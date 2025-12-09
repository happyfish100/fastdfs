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

# Upload buffer example
add_executable(upload_buffer_example upload_buffer_example.cpp)
target_link_libraries(upload_buffer_example fastdfs-client)

# Error handling example
add_executable(error_handling_example error_handling_example.cpp)
target_link_libraries(error_handling_example fastdfs-client)

# File info example
add_executable(file_info_example file_info_example.cpp)
target_link_libraries(file_info_example fastdfs-client)

# Partial download example
add_executable(partial_download_example partial_download_example.cpp)
target_link_libraries(partial_download_example fastdfs-client)

# Streaming example
add_executable(streaming_example streaming_example.cpp)
target_link_libraries(streaming_example fastdfs-client)

