// https://github.com/danilw/nanovg_vulkan

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#ifndef DEMO_ANTIALIAS
#define DEMO_ANTIALIAS 1
#endif
#ifndef DEMO_STENCIL_STROKES
#define DEMO_STENCIL_STROKES 1
#endif
#ifndef DEMO_VULKAN_VALIDATON_LAYER
#define DEMO_VULKAN_VALIDATON_LAYER 0
#endif

#define NANOVG_VULKAN_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_vk.h"

#include "demo.h"
#include "perf.h"

#include "vulkan_util.h"


void errorcb(int error, const char *desc) { printf("GLFW error %d: %s\n", error, desc); }

int blowup = 0;
int screenshot = 0;
int premult = 0;
bool resize_event = false;

static void key(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    blowup = !blowup;
  if (key == GLFW_KEY_S && action == GLFW_PRESS)
    screenshot = 1;
  if (key == GLFW_KEY_P && action == GLFW_PRESS)
    premult = !premult;
}

void prepareFrame(VkDevice device, VkCommandBuffer cmd_buffer, FrameBuffers *fb) {
  VkResult res;

  // Get the index of the next available swapchain image:
  res = vkAcquireNextImageKHR(device, fb->swap_chain, UINT64_MAX, fb->present_complete_semaphore[fb->current_frame],
                              VK_NULL_HANDLE, &fb->current_buffer);

  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_event = true;
    res = 0;
    return;
  }

  const VkCommandBufferBeginInfo cmd_buf_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkBeginCommandBuffer(cmd_buffer, &cmd_buf_info);

  VkRect2D render_area = {{0, 0}, {fb->buffer_size.width, fb->buffer_size.height}};

  // Dynamic rendering: transition the swapchain image UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL (its
  // contents are cleared) and the depth image UNDEFINED -> DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
  VkImageMemoryBarrier2 barriers[2] = {{0}, {0}};
  barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].image = fb->swap_chain_buffers[fb->current_buffer].image;
  barriers[0].subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  barriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[1].image = fb->depth.image;
  barriers[1].subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};

  VkDependencyInfo dep_info = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dep_info.imageMemoryBarrierCount = 2;
  dep_info.pImageMemoryBarriers = barriers;
  vkCmdPipelineBarrier2(cmd_buffer, &dep_info);

  VkRenderingAttachmentInfo color_attachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  color_attachment.imageView = fb->swap_chain_buffers[fb->current_buffer].view;
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color.float32[0] = 0.3f;
  color_attachment.clearValue.color.float32[1] = 0.3f;
  color_attachment.clearValue.color.float32[2] = 0.32f;
  color_attachment.clearValue.color.float32[3] = 1.0f;

  VkRenderingAttachmentInfo depth_attachment = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  depth_attachment.imageView = fb->depth.view;
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.clearValue.depthStencil.depth = 1.0f;
  depth_attachment.clearValue.depthStencil.stencil = 0;

  VkRenderingInfo rendering_info = {VK_STRUCTURE_TYPE_RENDERING_INFO};
  rendering_info.renderArea = render_area;
  rendering_info.layerCount = 1;
  rendering_info.colorAttachmentCount = 1;
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.pDepthAttachment = &depth_attachment;
  rendering_info.pStencilAttachment = &depth_attachment;
  vkCmdBeginRendering(cmd_buffer, &rendering_info);

  VkViewport viewport;
  viewport.width = (float) fb->buffer_size.width;
  viewport.height = (float) fb->buffer_size.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  viewport.x = (float) render_area.offset.x;
  viewport.y = (float) render_area.offset.y;
  vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

  vkCmdSetScissor(cmd_buffer, 0, 1, &render_area);
}

void submitFrame(VkDevice device, VkQueue graphicsQueue, VkQueue presentQueue, VkCommandBuffer cmd_buffer,
                 FrameBuffers *fb) {
  VkResult res;

  vkCmdEndRendering(cmd_buffer);

  VkImageMemoryBarrier image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = fb->swap_chain_buffers[fb->current_buffer].image,
    .subresourceRange =
      {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
  };
  vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       0, 0, NULL, 0, NULL, 1, &image_barrier);

  vkEndCommandBuffer(cmd_buffer);

  VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &fb->present_complete_semaphore[fb->current_frame];
  submit_info.pWaitDstStageMask = &pipe_stage_flags;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &fb->render_complete_semaphore[fb->current_frame];

  vkQueueSubmit(presentQueue, 1, &submit_info, fb->flight_fence[fb->current_frame]);

  /* Now present the image in the window */

  VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.pNext = NULL;
  present.swapchainCount = 1;
  present.pSwapchains = &fb->swap_chain;
  present.pImageIndices = &fb->current_buffer;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &fb->render_complete_semaphore[fb->current_frame];

  res = vkQueuePresentKHR(presentQueue, &present);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    vkQueueWaitIdle(graphicsQueue);
    vkQueueWaitIdle(presentQueue);
    resize_event = true;
    res = 0;
    return;
  }

  fb->current_frame = (fb->current_frame + 1) % fb->swapchain_image_count;
  fb->num_swaps++;

  if (fb->num_swaps >= fb->swapchain_image_count) {
    vkWaitForFences(device, 1, &fb->flight_fence[fb->current_frame], true, UINT64_MAX);
    vkResetFences(device, 1, &fb->flight_fence[fb->current_frame]);
  }
}

int main() {
  GLFWwindow *window;

  if (!glfwInit()) {
    printf("Failed to init GLFW.");
    return -1;
  }

  if (!glfwVulkanSupported()) {
    printf("vulkan dose not supported\n");
    return 1;
  }

  glfwSetErrorCallback(errorcb);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  window = glfwCreateWindow(1000, 600, "NanoVG Example Vulkan", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }

  glfwSetKeyCallback(window, key);

  glfwSetTime(0);

  bool enableValidationLayer = false;
#if DEMO_VULKAN_VALIDATON_LAYER
  enableValidationLayer = true;
#endif

  VkInstance instance = createVkInstance(enableValidationLayer);

  VkResult res;
  VkSurfaceKHR surface;
  res = glfwCreateWindowSurface(instance, window, 0, &surface);
  if (VK_SUCCESS != res) {
    printf("glfwCreateWindowSurface failed\n");
    exit(-1);
  }

  uint32_t gpu_count = 0;

  res = vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
  if (VK_SUCCESS != res && res != VK_INCOMPLETE) {
    printf("vkEnumeratePhysicalDevices failed %d \n", res);
    exit(-1);
  }
  if (gpu_count < 1) {
    printf("No Vulkan device found.\n");
    exit(-1);
  }

  VkPhysicalDevice gpu[32];
  res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpu);
  if (res != VK_SUCCESS && res != VK_INCOMPLETE) {
    printf("vkEnumeratePhysicalDevices failed %d \n", res);
    exit(-1);
  }

  uint32_t idx = 0;
  bool use_idx = false;
  bool discrete_idx = false;
  for (uint32_t i = 0; i < gpu_count && (!discrete_idx); i++) {
    uint32_t qfc = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu[i], &qfc, NULL);
    if (qfc < 1)
      continue;

    VkQueueFamilyProperties *queue_family_properties;
    queue_family_properties = malloc(qfc * sizeof(VkQueueFamilyProperties));

    vkGetPhysicalDeviceQueueFamilyProperties(gpu[i], &qfc, queue_family_properties);

    for (uint32_t j = 0; j < qfc; j++) {
      VkBool32 supports_present;
      vkGetPhysicalDeviceSurfaceSupportKHR(gpu[i], j, surface, &supports_present);

      if ((queue_family_properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_present) {

        VkPhysicalDeviceProperties pr;
        vkGetPhysicalDeviceProperties(gpu[i], &pr);
        idx = i;
        use_idx = true;
        if (pr.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
          discrete_idx = true;
        }
        break;
      }
    }
    free(queue_family_properties);
  }
  if (!use_idx) {
    printf("Not found suitable queue which supports graphics.\n");
    exit(-1);
  }

  printf("Using GPU device %lu\n", (unsigned long) idx);

  VkNvgExt extQuery = {};
  VulkanDevice *device = createVulkanDevice(gpu[idx], surface, &extQuery);

  int winWidth, winHeight;
  glfwGetWindowSize(window, &winWidth, &winHeight);

  VkQueue executionQueue;
  VkQueue presentQueue;
  vkGetDeviceQueue(device->device, device->graphicsQueueFamilyIndex, 0, &executionQueue);
  vkGetDeviceQueue(device->device, device->graphicsQueueFamilyIndex, 0, &presentQueue);

  FrameBuffers fb = createFrameBuffers(device, surface, executionQueue, winWidth, winHeight, 0);

  VkCommandBuffer *cmd_buffer = createCmdBuffer(device->device, device->commandPool, fb.swapchain_image_count);

  VKNVGCreateInfo create_info = {0};
  create_info.device = device->device;
  create_info.gpu = device->gpu;
  create_info.cmdBuffer = cmd_buffer;
  create_info.swapchainImageCount = fb.swapchain_image_count;
  create_info.currentFrame = &fb.current_frame;
  create_info.colorFormat = fb.format;
  create_info.depthStencilFormat = fb.depth.format;

  /**
   * Either explicitly set the following to false or query your hardware and enable these items as necessary.
   * See usage inside `createVulkanDevice` for more info.
   * Utilises the capabilities of your hardware based on enabled extensions, either implicit (API version) or explicit
   */
  create_info.ext.dynamicState = extQuery.dynamicState;
  create_info.ext.colorBlendEquation = extQuery.colorBlendEquation;
  create_info.ext.colorWriteMask = extQuery.colorWriteMask;

  int flags = 0;
#ifndef NDEBUG
  flags |= NVG_DEBUG; // unused in nanovg_vk
#endif
#if DEMO_ANTIALIAS
  flags |= NVG_ANTIALIAS;
#endif
#if DEMO_STENCIL_STROKES
  flags |= NVG_STENCIL_STROKES;
#endif

  NVGcontext *vg = nvgCreateVk(create_info, flags, executionQueue);

  DemoData data;
  PerfGraph fps; //, cpuGraph, gpuGraph;
  if (loadDemoData(vg, &data) == -1)
    return -1;

  initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
  // initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
  // initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");
  double prevt = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    float pxRatio;
    double mx, my, t, dt;

    int cwinWidth, cwinHeight;
    glfwGetWindowSize(window, &cwinWidth, &cwinHeight);
    if ((resize_event) || (winWidth != cwinWidth || winHeight != cwinHeight)) {
      winWidth = cwinWidth;
      winHeight = cwinHeight;
      destroyFrameBuffers(device, &fb, executionQueue);
      fb = createFrameBuffers(device, surface, executionQueue, winWidth, winHeight, 0);
      resize_event = false;
    } else {

      prepareFrame(device->device, cmd_buffer[fb.current_frame], &fb);
      if (resize_event)
        continue;
      t = glfwGetTime();
      dt = t - prevt;
      prevt = t;
      updateGraph(&fps, (float) dt);
      pxRatio = (float) fb.buffer_size.width / (float) winWidth;

      glfwGetCursorPos(window, &mx, &my);

      nvgBeginFrame(vg, (float) winWidth, (float) winHeight, pxRatio);
      renderDemo(vg, mx, my, (float) winWidth, (float) winHeight, t, blowup, &data);
      renderGraph(vg, 5, 5, &fps);

      nvgEndFrame(vg);

      submitFrame(device->device, executionQueue, presentQueue, cmd_buffer[fb.current_frame], &fb);
    }
    glfwPollEvents();
  }

  vkQueueWaitIdle(executionQueue);
  vkQueueWaitIdle(presentQueue);

  freeDemoData(vg, &data);
  nvgDeleteVk(vg);

  destroyFrameBuffers(device, &fb, executionQueue);

  destroyVulkanDevice(device);

  destroyDebugCallback(instance);

  vkDestroyInstance(instance, NULL);

  glfwDestroyWindow(window);

  free(cmd_buffer);

  printf("Average Frame Time: %.2f ms\n", getGraphAverage(&fps) * 1000.0f);
  // printf("          CPU Time: %.2f ms\n", getGraphAverage(&cpuGraph) * 1000.0f);
  // printf("          GPU Time: %.2f ms\n", getGraphAverage(&gpuGraph) * 1000.0f);

  glfwTerminate();
  return 0;
}
