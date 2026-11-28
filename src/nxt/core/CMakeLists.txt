# src/nxt/core/CMakeLists.txt

# std::thread는 플랫폼별 스레드 라이브러리를 요구한다(MinGW의 -pthread 등).
find_package(Threads REQUIRED)

add_library(nxt_core STATIC
    diagnostics/assert.cpp
    diagnostics/log.cpp
    diagnostics/profiler.cpp
    jobs/std_thread_source.cpp
    jobs/job_counter.cpp
    jobs/worker.cpp
    jobs/worker_pool.cpp
)

target_sources(nxt_core
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${PROJECT_SOURCE_DIR}/src"
        FILES
            concurrency/spsc_queue.hpp
            handle/handle.hpp
            handle/handle_manager.hpp
            diagnostics/assert.hpp
            diagnostics/log.hpp
            diagnostics/profiler.hpp
            time/time.hpp
            time/timer.hpp
            memory/allocator.hpp
            memory/arena.hpp
            memory/pool.hpp
            jobs/thread_source.hpp
            jobs/std_thread_source.hpp
            jobs/job.hpp
            jobs/job_counter.hpp
            jobs/worker.hpp
            jobs/worker_pool.hpp
)

target_include_directories(nxt_core PUBLIC "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(nxt_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(nxt_core PRIVATE /W4)
else()
    target_compile_options(nxt_core PRIVATE -Wall -Wextra)
endif()
