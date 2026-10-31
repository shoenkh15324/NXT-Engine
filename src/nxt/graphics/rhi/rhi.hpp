#pragma once

// 렌더 하드웨어 인터페이스 추상화.
//
// 소비자(renderer · engine)가 가장 먼저 include하는 입구다. RHI 공개 헤더를 한
// 곳에 모아, 이 파일 하나로 공개 표면이 드러나게 한다. vk:: 타입은 여기까지 오지
// 않는다 — backends/vulkan/ 안에만 있다 (rhi.md §4).

#include <nxt/graphics/rhi/common/enums.hpp>
#include <nxt/graphics/rhi/common/handle.hpp>
#include <nxt/graphics/rhi/core/device.hpp>
#include <nxt/graphics/rhi/core/instance.hpp>
#include <nxt/graphics/rhi/frame/command_buffer.hpp>
#include <nxt/graphics/rhi/frame/fence.hpp>
#include <nxt/graphics/rhi/frame/resource_deletion.hpp>
#include <nxt/graphics/rhi/frame/semaphore.hpp>
#include <nxt/graphics/rhi/frame/swapchain.hpp>
#include <nxt/graphics/rhi/resource/image.hpp>
#include <nxt/graphics/rhi/resource/memory.hpp>
