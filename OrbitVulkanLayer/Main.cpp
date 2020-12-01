// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "LayerLogic.h"
#include "absl/base/casts.h"
#include "vulkan/vk_layer.h"
#include "vulkan/vulkan.h"

/*
 * The big picture:
 * This is the main entry point for Orbit's vulkan layer. The layer is structured as follows:
 * * All instrumented vulkan functions will hook into implementations found here
 *   (e.g. OrbitQueueSubmit)
 * * The actual logic of the layer is implemented in LayerLogic.h/.cpp. This has the following
 *    scheme: For each vk function, there is a PreCall*, Call*, and PostCall* function, where
 *    Call* will just forward the call to the "actual" vulkan function following the dispatch
 *    table (see DispatchTable).
 *  * There are the following helper classes to structure the actual layer logic:
 *     * CommandBufferManager.h: Which keeps track of command buffer allocations.
 *     * DispatchTable.h: Which provides virtual dispatch for the vulkan functions to be called.
 *     * QueryManager.h: Which keeps track of query pool slots e.g. used for timestamp queries
 *        and allows to assign those.
 *     * QueueManager keeps track of association of VkQueue(s) to devices.
 *
 *
 * For this free functions in this namespace:
 * As said, they act as entries to the layer.
 * OrbitGetDeviceProcAddr and OrbitGetInstanceProcAddr are the actual entry points, called by
 * the loader and potential other layers, and forward to all the functions that this layer
 * intercepts.
 *
 * The actual logic of the layer (and thus of each intercepted vulkan function) is implemented
 * in `LayerLogic`.
 *
 * Only the basic enumeration as well as the ProcAddr functions are implemented here.
 *
 */
namespace orbit_vulkan_layer {

#if defined(WIN32)
#define ORBIT_EXPORT extern "C" __declspec(dllexport) VK_LAYER_EXPORT
#else
#define ORBIT_EXPORT extern "C" VK_LAYER_EXPORT
#endif

// layer metadata
static constexpr const char* const kLayerName = "ORBIT_VK_LAYER";
static constexpr const char* const kLayerDescription =
    "Provides GPU insights for the Orbit Profiler";

static constexpr const uint32_t kLayerImplVersion = 1;
static constexpr const uint32_t kLayerSpecVersion = VK_API_VERSION_1_1;

static const std::array<VkExtensionProperties, 3> device_extensions = {
    VkExtensionProperties{.extensionName = VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
                          .specVersion = VK_EXT_DEBUG_MARKER_SPEC_VERSION},
    VkExtensionProperties{.extensionName = VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                          .specVersion = VK_EXT_DEBUG_UTILS_SPEC_VERSION},
    VkExtensionProperties{.extensionName = VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
                          .specVersion = VK_EXT_HOST_QUERY_RESET_SPEC_VERSION}};
static LayerLogic logic_;

// ----------------------------------------------------------------------------
// Layer bootstrapping code
// ----------------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL OrbitCreateInstance(const VkInstanceCreateInfo* create_info,
                                                   const VkAllocationCallbacks* allocator,
                                                   VkInstance* instance) {
  VkResult result = logic_.PreCallAndCallCreateInstance(create_info, allocator, instance);
  logic_.PostCallCreateInstance(create_info, allocator, instance);
  return result;
}

VKAPI_ATTR void VKAPI_CALL OrbitDestroyInstance(VkInstance instance,
                                                const VkAllocationCallbacks* allocator) {
  logic_.CallAndPostDestroyInstance(instance, allocator);
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitCreateDevice(VkPhysicalDevice physical_device,
                                                 const VkDeviceCreateInfo* create_info,
                                                 const VkAllocationCallbacks* allocator,
                                                 VkDevice* device) {
  VkResult result =
      logic_.PreCallAndCallCreateDevice(physical_device, create_info, allocator, device);
  logic_.PostCallCreateDevice(physical_device, create_info, allocator, device);
  return result;
}

VKAPI_ATTR void VKAPI_CALL OrbitDestroyDevice(VkDevice device,
                                              const VkAllocationCallbacks* allocator) {
  logic_.CallAndPostDestroyDevice(device, allocator);
}

// ----------------------------------------------------------------------------
// Core layer logic
// ----------------------------------------------------------------------------

VKAPI_ATTR VkResult VKAPI_CALL OrbitResetCommandPool(VkDevice device, VkCommandPool command_pool,
                                                     VkCommandPoolResetFlags flags) {
  try {
    VkResult result = logic_.CallResetCommandPool(device, command_pool, flags);
    logic_.PostCallResetCommandPool(device, command_pool, flags);
    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL
OrbitAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* allocate_info,
                            VkCommandBuffer* command_buffers) {
  try {
    VkResult result = logic_.CallAllocateCommandBuffers(device, allocate_info, command_buffers);
    logic_.PostCallAllocateCommandBuffers(device, allocate_info, command_buffers);
    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitFreeCommandBuffers(VkDevice device, VkCommandPool command_pool,
                                                   uint32_t command_buffer_count,
                                                   const VkCommandBuffer* command_buffers) {
  try {
    logic_.CallFreeCommandBuffers(device, command_pool, command_buffer_count, command_buffers);
    logic_.PostCallFreeCommandBuffers(device, command_pool, command_buffer_count, command_buffers);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitBeginCommandBuffer(VkCommandBuffer command_buffer,
                                                       const VkCommandBufferBeginInfo* begin_info) {
  try {
    VkResult result = logic_.CallBeginCommandBuffer(command_buffer, begin_info);
    logic_.PostCallBeginCommandBuffer(command_buffer, begin_info);

    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitEndCommandBuffer(VkCommandBuffer command_buffer) {
  try {
    logic_.PreCallEndCommandBuffer(command_buffer);
    VkResult result = logic_.CallEndCommandBuffer(command_buffer);
    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitResetCommandBuffer(VkCommandBuffer command_buffer,
                                                       VkCommandBufferResetFlags flags) {
  try {
    logic_.PreCallResetCommandBuffer(command_buffer, flags);
    VkResult result = logic_.CallResetCommandBuffer(command_buffer, flags);

    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitGetDeviceQueue(VkDevice device, uint32_t queue_family_index,
                                               uint32_t queue_index, VkQueue* pQueue) {
  try {
    logic_.CallGetDeviceQueue(device, queue_family_index, queue_index, pQueue);
    logic_.PostCallGetDeviceQueue(device, queue_family_index, queue_index, pQueue);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitGetDeviceQueue2(VkDevice device,
                                                const VkDeviceQueueInfo2* queue_info,
                                                VkQueue* queue) {
  try {
    logic_.CallGetDeviceQueue2(device, queue_info, queue);
    logic_.PostCallGetDeviceQueue2(device, queue_info, queue);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitQueueSubmit(VkQueue queue, uint32_t submit_count,
                                                const VkSubmitInfo* submits, VkFence fence) {
  try {
    std::optional<uint64_t> pre_submit_timestamp =
        logic_.PreCallQueueSubmit(queue, submit_count, submits, fence);
    VkResult result = logic_.CallQueueSubmit(queue, submit_count, submits, fence);
    CHECK(result == VK_SUCCESS);
    logic_.PostCallQueueSubmit(queue, submit_count, submits, fence, pre_submit_timestamp);
    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitQueuePresentKHR(VkQueue queue,
                                                    const VkPresentInfoKHR* present_info) {
  try {
    VkResult result = logic_.CallQueuePresentKHR(queue, present_info);
    logic_.PostCallQueuePresentKHR(queue, present_info);
    return result;
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitCmdBeginDebugUtilsLabelEXT(VkCommandBuffer command_buffer,
                                                           const VkDebugUtilsLabelEXT* label_info) {
  try {
    logic_.CallCmdBeginDebugUtilsLabelEXT(command_buffer, label_info);
    logic_.PostCallCmdBeginDebugUtilsLabelEXT(command_buffer, label_info);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitCmdEndDebugUtilsLabelEXT(VkCommandBuffer command_buffer) {
  try {
    logic_.PreCallCmdEndDebugUtilsLabelEXT(command_buffer);
    logic_.CallCmdEndDebugUtilsLabelEXT(command_buffer);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitCmdDebugMarkerBeginEXT(
    VkCommandBuffer command_buffer, const VkDebugMarkerMarkerInfoEXT* marker_info) {
  try {
    logic_.CallCmdDebugMarkerBeginEXT(command_buffer, marker_info);
    logic_.PostCallCmdDebugMarkerBeginEXT(command_buffer, marker_info);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

VKAPI_ATTR void VKAPI_CALL OrbitCmdDebugMarkerEndEXT(VkCommandBuffer command_buffer) {
  try {
    logic_.PreCallCmdDebugMarkerEndEXT(command_buffer);
    logic_.CallCmdDebugMarkerEndEXT(command_buffer);
  } catch (const std::exception& e) {
    CHECK(false);
  }
}

// ----------------------------------------------------------------------------
// Layer enumeration functions
// ----------------------------------------------------------------------------
VKAPI_ATTR VkResult VKAPI_CALL
OrbitEnumerateInstanceLayerProperties(uint32_t* property_count, VkLayerProperties* properties) {
  // Vulkan spec dictates that we are only supposed to enumerate ourself
  if (property_count != nullptr) {
    *property_count = 1;
  }
  if (properties != nullptr) {
    snprintf(properties->layerName, strlen(kLayerName), kLayerName);
    snprintf(properties->description, strlen(kLayerDescription), kLayerDescription);
    properties->implementationVersion = kLayerImplVersion;
    properties->specVersion = kLayerSpecVersion;
  }

  return VK_SUCCESS;
}

// Deprecated by Khronos, but we'll support it in case older applications still
// use it.
VKAPI_ATTR VkResult VKAPI_CALL OrbitEnumerateDeviceLayerProperties(
    VkPhysicalDevice /*physical_device*/, uint32_t* property_count, VkLayerProperties* properties) {
  // This function is supposed to return the same results as
  // EnumerateInstanceLayerProperties since device layers were deprecated.
  return OrbitEnumerateInstanceLayerProperties(property_count, properties);
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitEnumerateInstanceExtensionProperties(
    const char* layer_name, uint32_t* property_count, VkExtensionProperties* /*properties*/) {
  // Inform the client that we have no extension properties if this layer
  // specifically is being queried.
  if (layer_name != nullptr && strcmp(layer_name, kLayerName) == 0) {
    if (property_count != nullptr) {
      *property_count = 0;
    }
    return VK_SUCCESS;
  }

  CHECK(false);

  // Vulkan spec mandates returning this when this layer isn't being queried.
  return VK_ERROR_LAYER_NOT_PRESENT;
}

VKAPI_ATTR VkResult VKAPI_CALL OrbitEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physical_device, const char* layer_name, uint32_t* property_count,
    VkExtensionProperties* properties) {
  // If our layer is queried exclusively, we just return our extensions
  if (layer_name != nullptr && strcmp(layer_name, kLayerName) == 0) {
    // If properties == nullptr, only the number of extensions are queried.
    if (properties == nullptr) {
      *property_count = device_extensions.size();
      return VK_SUCCESS;
    }
    uint32_t num_extensions_to_copy = device_extensions.size();
    // In the case that less extensions are queried then the layer uses, we copy on this number
    // and return VK_INCOMPLETE, according to the specification.
    if (*property_count < num_extensions_to_copy) {
      num_extensions_to_copy = *property_count;
    }
    memcpy(properties, device_extensions.data(),
           num_extensions_to_copy * sizeof(VkExtensionProperties));
    *property_count = num_extensions_to_copy;

    if (num_extensions_to_copy < device_extensions.size()) {
      return VK_INCOMPLETE;
    }
    return VK_SUCCESS;
  }

  // If a different layer is queried exclusively, we forward the call.
  if (layer_name != nullptr) {
    return logic_.CallEnumerateDeviceExtensionProperties(physical_device, layer_name,
                                                         property_count, properties);
  }

  // This is a general query, so we need to append our extensions to the once down in the
  // callchain.
  uint32_t num_other_extensions;
  VkResult result = logic_.CallEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                                  &num_other_extensions, nullptr);
  if (result != VK_SUCCESS) {
    return result;
  }

  std::vector<VkExtensionProperties> extensions(num_other_extensions);
  result = logic_.CallEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                                         &num_other_extensions, extensions.data());
  if (result != VK_SUCCESS) {
    return result;
  }

  // Lets append all of our extensions, that are not yet listed.
  // Note, as this list of our extensions is very small, we are fine with O(N*M) runtime.
  for (const auto& extension : device_extensions) {
    bool is_unique = true;
    for (const auto& other_extension : extensions) {
      if (strcmp(extension.extensionName, other_extension.extensionName) == 0) {
        is_unique = false;
        break;
      }
    }
    if (is_unique) {
      extensions.push_back(extension);
    }
  }

  // As above, if properties is nullptr, only the number if extensions is queried.
  if (properties == nullptr) {
    *property_count = extensions.size();
    return VK_SUCCESS;
  }

  uint32_t num_extensions_to_copy = extensions.size();
  // In the case that less extensions are queried then the layer uses, we copy on this number and
  // return VK_INCOMPLETE, according to the specification.
  if (*property_count < num_extensions_to_copy) {
    num_extensions_to_copy = *property_count;
  }
  memcpy(properties, extensions.data(), num_extensions_to_copy * sizeof(VkExtensionProperties));
  *property_count = num_extensions_to_copy;

  if (num_extensions_to_copy < extensions.size()) {
    return VK_INCOMPLETE;
  }
  return VK_SUCCESS;
}

// ----------------------------------------------------------------------------
// GetProcAddr functions
// ----------------------------------------------------------------------------

#define ORBIT_GETPROCADDR(func)                              \
  if (strcmp(name, "vk" #func) == 0) {                       \
    return absl::bit_cast<PFN_vkVoidFunction>(&Orbit##func); \
  }

ORBIT_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OrbitGetDeviceProcAddr(VkDevice device,
                                                                             const char* name) {
  // Functions available through GetInstanceProcAddr and GetDeviceProcAddr
  ORBIT_GETPROCADDR(GetDeviceProcAddr)
  ORBIT_GETPROCADDR(EnumerateDeviceLayerProperties)
  ORBIT_GETPROCADDR(EnumerateDeviceExtensionProperties)
  ORBIT_GETPROCADDR(CreateDevice)
  ORBIT_GETPROCADDR(DestroyDevice)

  ORBIT_GETPROCADDR(ResetCommandPool)

  ORBIT_GETPROCADDR(AllocateCommandBuffers)
  ORBIT_GETPROCADDR(FreeCommandBuffers)

  ORBIT_GETPROCADDR(BeginCommandBuffer)
  ORBIT_GETPROCADDR(EndCommandBuffer)
  ORBIT_GETPROCADDR(ResetCommandBuffer)

  ORBIT_GETPROCADDR(QueueSubmit)
  ORBIT_GETPROCADDR(QueuePresentKHR)
  ORBIT_GETPROCADDR(GetDeviceQueue)
  ORBIT_GETPROCADDR(GetDeviceQueue2)

  ORBIT_GETPROCADDR(CmdBeginDebugUtilsLabelEXT)
  ORBIT_GETPROCADDR(CmdEndDebugUtilsLabelEXT)
  ORBIT_GETPROCADDR(CmdDebugMarkerBeginEXT)
  ORBIT_GETPROCADDR(CmdDebugMarkerEndEXT)
  LOG("Fallback");
  return logic_.CallGetDeviceProcAddr(device, name);
}

ORBIT_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL OrbitGetInstanceProcAddr(VkInstance instance,
                                                                               const char* name) {
  // Functions available only through GetInstanceProcAddr
  ORBIT_GETPROCADDR(GetInstanceProcAddr)
  ORBIT_GETPROCADDR(CreateInstance)
  ORBIT_GETPROCADDR(DestroyInstance)
  ORBIT_GETPROCADDR(EnumerateInstanceLayerProperties)
  ORBIT_GETPROCADDR(EnumerateInstanceExtensionProperties)

  // Functions available through GetInstanceProcAddr and GetDeviceProcAddr
  ORBIT_GETPROCADDR(GetDeviceProcAddr)
  ORBIT_GETPROCADDR(EnumerateDeviceLayerProperties)
  ORBIT_GETPROCADDR(EnumerateDeviceExtensionProperties)
  ORBIT_GETPROCADDR(CreateDevice)
  ORBIT_GETPROCADDR(DestroyDevice)

  ORBIT_GETPROCADDR(ResetCommandPool)

  ORBIT_GETPROCADDR(AllocateCommandBuffers)
  ORBIT_GETPROCADDR(FreeCommandBuffers)

  ORBIT_GETPROCADDR(BeginCommandBuffer)
  ORBIT_GETPROCADDR(EndCommandBuffer)
  ORBIT_GETPROCADDR(ResetCommandBuffer)

  ORBIT_GETPROCADDR(QueueSubmit)
  ORBIT_GETPROCADDR(QueuePresentKHR)
  ORBIT_GETPROCADDR(GetDeviceQueue)
  ORBIT_GETPROCADDR(GetDeviceQueue2)

  ORBIT_GETPROCADDR(CmdBeginDebugUtilsLabelEXT)
  ORBIT_GETPROCADDR(CmdEndDebugUtilsLabelEXT)
  ORBIT_GETPROCADDR(CmdDebugMarkerBeginEXT)
  ORBIT_GETPROCADDR(CmdDebugMarkerEndEXT)

  return logic_.CallGetInstanceProcAddr(instance, name);
}

#undef ORBIT_GETPROCADDR

#undef ORBIT_EXPORT

}  // namespace orbit_vulkan_layer