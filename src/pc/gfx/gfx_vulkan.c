#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "gfx_cc.h"
#include "gfx_rendering_api.h"

#include "src/pc/controller/controller_keyboard.h"

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;
const char *validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static GLFWwindow *wnd;
static VkInstance instance;
static VkDebugUtilsMessengerEXT debugMessenger;
static VkSurfaceKHR surface;
static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
static VkDevice device;
static VkQueue graphicsQueue;
static VkQueue presentQueue;
static VkSwapchainKHR swapChain;
static struct {
    size_t length;
    VkImage *data;
} swapChainImages;
static VkFormat swapChainImageFormat;
static VkExtent2D swapChainExtent;
static struct {
    size_t length;
    VkImageView *data;
} swapChainImageViews;

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    struct {
        size_t length;
        VkSurfaceFormatKHR *data;
    } formats;
    struct {
        size_t length;
        VkPresentModeKHR *data;
    } presentModes;
};

static struct QueueFamilyIndices createQueueFamilyIndices(void) {
    struct QueueFamilyIndices queueFamilyIndices = { -1, -1 };
    return queueFamilyIndices;
}

static uint32_t numberOfQueues(struct QueueFamilyIndices *queueFamilyIndices) {
    return sizeof(queueFamilyIndices) / sizeof(uint32_t);
}

static bool isCompleted(struct QueueFamilyIndices *queueFamilyIndices) {
    return queueFamilyIndices->graphicsFamily != -1 && queueFamilyIndices->presentFamily != 1;
}
static uint32_t min(uint32_t x, uint32_t y) {
    return y ^ ((x ^ y) & -(x < y));
}

/*Function to find maximum of x and y*/
static uint32_t max(uint32_t x, uint32_t y) {
    return x ^ ((x ^ y) & -(x < y));
}
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                             const VkAllocationCallbacks *pAllocator,
                                             VkDebugUtilsMessengerEXT *pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance,
                                                                   "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks *pAllocator) {
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance,
                                                                    "vkDestroyDebugUtilsMessengerEXT");
    if (func != NULL) {
        func(instance, debugMessenger, pAllocator);
    }
}

static void cleanup() {
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
    }
    for (int i = 0; i < swapChainImageViews.length; i++) {
        vkDestroyImageView(device, swapChainImageViews.data[i], NULL);
    }
    vkDestroySwapchainKHR(device, swapChain, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(wnd);
    glfwTerminate();
}

static const char **getRequiredExtensions(uint32_t *totalExtensionCount) {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    *totalExtensionCount = glfwExtensionCount;
    if (enableValidationLayers)
        (*totalExtensionCount)++;

    char **totalExtensions = malloc(sizeof(char *) * (*totalExtensionCount));
    for (int i = 0; i < glfwExtensionCount; i++) {
        totalExtensions[i] = glfwExtensions[i];
    }
    if (enableValidationLayers)
        totalExtensions[glfwExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    return totalExtensions;
}
static bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);

    VkLayerProperties availableLayers[layerCount];
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

    for (int i = 0; i < sizeof(validationLayers) / sizeof(char *); i++) {
        bool layerFound = false;

        for (int j = 0; j < sizeof(availableLayers) / sizeof(VkLayerProperties); j++) {
            if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
    printf("validation layer: %s", pCallbackData->pMessage);

    return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo) {
    createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                  | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                              | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo->pfnUserCallback = debugCallback;
}

static void setupDebugMessenger() {
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    populateDebugMessengerCreateInfo(&createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, NULL, &debugMessenger) != VK_SUCCESS) {
        printf("failed to set up debug messenger!");
        exit(-1);
    }
}

static void createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        printf("validation layers requested, but not available!");
        exit(-1);
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Super Mario 64 PC Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t extensionCount = 0;
    getRequiredExtensions(&extensionCount);
    const char **extensions = getRequiredExtensions(&extensionCount);
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = sizeof(validationLayers) / sizeof(char *);
        createInfo.ppEnabledLayerNames = validationLayers;

        populateDebugMessengerCreateInfo(&debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = NULL;
    }

    if (vkCreateInstance(&createInfo, NULL, &instance) != VK_SUCCESS) {
        printf("failed to create instance!");
        exit(-1);
    }
}
static void createSurface() {
    if (glfwCreateWindowSurface(instance, wnd, NULL, &surface) != VK_SUCCESS) {
        printf("failed to create window surface!");
        exit(-1);
    }
}
static struct QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    struct QueueFamilyIndices queueFamilyIndices = createQueueFamilyIndices();

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

    VkQueueFamilyProperties queueFamilies[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

    for (int i = 0; i < sizeof(queueFamilies); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIndices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            queueFamilyIndices.presentFamily = i;
        }

        if (isCompleted(&queueFamilyIndices)) {
            break;
        }
    }

    return queueFamilyIndices;
}
static struct SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    struct SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
    if (formatCount != 0) {
        details.formats.length = formatCount;
        details.formats.data = (VkSurfaceFormatKHR *) malloc(formatCount * sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data);
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);

    if (presentModeCount != 0) {
        details.presentModes.length = presentModeCount;
        details.presentModes.data =
            (VkPresentModeKHR *) malloc(presentModeCount * sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                  details.presentModes.data);
    }

    return details;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR *availableFormats,
                                                  uint32_t forrmatsCount) {
    for (int i = 0; i < forrmatsCount; i++) {
        if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB
            && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormats[i];
        }
    }
    return availableFormats[0];
}
static VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR *availablePresentModes,
                                              uint32_t presentModesCount) {
    for (int i = 0; i < presentModesCount; i++) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentModes[i];
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}
static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = { WIDTH, HEIGHT };

        actualExtent.width = max(capabilities.minImageExtent.width,
                                 min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = max(capabilities.minImageExtent.height,
                                  min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}
static void createImageViews() {
    swapChainImageViews.length = swapChainImages.length;

    swapChainImageViews.data = (VkImageView *) malloc(sizeof(VkImageView) * swapChainImages.length);
    for (size_t i = 0; i < swapChainImages.length; i++) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages.data[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &createInfo, NULL, &swapChainImageViews.data[i]) != VK_SUCCESS) {
            printf("failed to create image views!");
            exit(-1);
        }
    }
}

static void createSwapChain() {
    struct SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats.data, swapChainSupport.formats.length);
    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes.data, swapChainSupport.formats.length);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0
        && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    swapChainImageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    swapChainExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    struct QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;  // Optional
        createInfo.pQueueFamilyIndices = NULL; // Optional
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, NULL, &swapChain) != VK_SUCCESS) {
        printf("failed to create swap chain!");
        exit(-1);
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
    swapChainImages.length = imageCount;
    swapChainImages.data = (VkImage *) malloc(imageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data);
}
static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

    VkExtensionProperties availableExtensions[extensionCount];
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions);

    uint32_t requiredExtensionCount = sizeof(deviceExtensions) / sizeof(char *);

    for (int i = 0; i < sizeof(deviceExtensions) / sizeof(char); i++) {
        for (int j = 0; j < extensionCount; j++) {
            if (strcmp(deviceExtensions[i], availableExtensions[j].extensionName) == 0) {
                requiredExtensionCount--;
            }
        }

        return requiredExtensionCount == 0;
    }
}
static bool isDeviceSuitable(VkPhysicalDevice device) {
    struct QueueFamilyIndices queueFamilyIndices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        struct SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate =
            swapChainSupport.formats.length > 0 && swapChainSupport.presentModes.length > 0;
    }
    return isCompleted(&queueFamilyIndices) && checkDeviceExtensionSupport(device) && swapChainAdequate;
}

static void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

    if (deviceCount == 0) {
        printf("failed to find GPUs with Vulkan support!");
        exit(-1);
    }

    VkPhysicalDevice devices[deviceCount];
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices);

    for (int i = 0; i < deviceCount; i++) {
        if (isDeviceSuitable(devices[i])) {
            physicalDevice = devices[i];
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        printf("failed to find a suitable GPU!");
        exit(-1);
    }
}

static void createLogicalDevice() {
    struct QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    uint32_t uniqueQueueFamilies[] = { queueFamilyIndices.graphicsFamily,
                                       queueFamilyIndices.presentFamily };
    uint32_t uniqueQueueFamiliesCount = sizeof(uniqueQueueFamilies) / sizeof(uint32_t);
    VkDeviceQueueCreateInfo *queueCreateInfos =
        malloc(sizeof(VkDeviceQueueCreateInfo) * uniqueQueueFamiliesCount);

    float queuePriority = 1.0f;
    for (int i = 0; i < uniqueQueueFamiliesCount; i++) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = uniqueQueueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        memcpy(queueCreateInfos + i, &queueCreateInfo, sizeof(VkDeviceQueueCreateInfo));
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = uniqueQueueFamiliesCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = sizeof(deviceExtensions) / sizeof(char *);
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = sizeof(validationLayers) / sizeof(char *);
        createInfo.ppEnabledLayerNames = validationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, NULL, &device) != VK_SUCCESS) {
        printf("failed to create logical device!");
        exit(-1);
    }

    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.graphicsFamily, 0, &presentQueue);
}
void createGraphicsPipeline() {

}
static void gfx_glfw_init(void) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    wnd = glfwCreateWindow(800, 600, "Super Mario 64 PC port (Vulkan)", NULL, NULL);
}

static void gfx_glfw_main_loop(void (*run_one_game_iter)(void)) {
    double t;
    while (!glfwWindowShouldClose(wnd)) {
        t = glfwGetTime();
        run_one_game_iter();
        t = glfwGetTime() - t;

        if (t < 1. / 30.) {
            usleep(1000000. * (1. / 30. - t));
        }
    }
    cleanup();
    exit(0);
}

static void gfx_glfw_get_dimensions(uint32_t *width, uint32_t *height) {
    glfwGetWindowSize(wnd, width, height);
}

static void gfx_glfw_onkeydown(int scancode) {
    // keyboard_on_key_down(translate_scancode(scancode));
}

static void gfx_glfw_onkeyup(int scancode) {
    // keyboard_on_key_up(translate_scancode(scancode));
}

static void gfx_glfw_handle_events(void) {
    glfwPollEvents();
}

static bool gfx_glfw_start_frame(void) {
    return true;
}

static void gfx_glfw_swap_buffers_begin(void) {
    glfwSwapBuffers(wnd);
}

static void gfx_glfw_swap_buffers_end(void) {
}

static double gfx_glfw_get_time(void) {
    return 0.0;
}

struct ShaderProgram {
    uint32_t shader_id;
    uint32_t opengl_program_id;
    uint8_t num_inputs;
    bool used_textures[2];
    uint8_t num_floats;
    uint32_t attrib_locations[7];
    uint8_t attrib_sizes[7];
    uint8_t num_attribs;
};

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static uint32_t opengl_vbo;

static bool gfx_vulkan_z_is_from_0_to_1(void) {
    return false;
}

static void gfx_vulkan_vertex_array_set_attribs(struct ShaderProgram *prg) {
}

static void gfx_vulkan_unload_shader(struct ShaderProgram *old_prg) {
}

static void gfx_vulkan_load_shader(struct ShaderProgram *new_prg) {
}

static void append_str(char *buf, size_t *len, const char *str) {
}

static void append_line(char *buf, size_t *len, const char *str) {
}

static const char *shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha,
                                      bool inputs_have_alpha, bool hint_single_element) {
}

static void append_formula(char *buf, size_t *len, uint8_t c[2][4], bool do_single, bool do_multiply,
                           bool do_mix, bool with_alpha, bool only_alpha, bool opt_alpha) {
}

static struct ShaderProgram *gfx_vulkan_create_and_load_new_shader(uint32_t shader_id) {
}

static struct ShaderProgram *gfx_vulkan_lookup_shader(uint32_t shader_id) {
}

static void gfx_vulkan_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs,
                                       bool used_textures[2]) {
}
static uint32_t gfx_vulkan_new_texture(void) {
    uint32_t ret = 0;
    return ret;
}
static void gfx_vulkan_select_texture(int tile) {
}

static void gfx_vulkan_upload_texture(uint8_t *rgba32_buf, int width, int height) {
}

static void gfx_vulkan_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms,
                                              uint32_t cmt) {
}

static void gfx_vulkan_set_depth_test(bool depth_test) {
}

static void gfx_vulkan_set_depth_mask(bool z_upd) {
}

static void gfx_vulkan_set_zmode_decal(bool zmode_decal) {
}

static void gfx_vulkan_set_viewport(int x, int y, int width, int height) {
}

static void gfx_vulkan_set_scissor(int x, int y, int width, int height) {
}

static void gfx_vulkan_set_use_alpha(bool use_alpha) {
}

static void gfx_vulkan_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
}

static void gfx_vulkan_init(void) {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
}

static void gfx_vulkan_start_frame(void) {
}

struct GfxRenderingAPI gfx_vulkan_api = {
    gfx_vulkan_z_is_from_0_to_1, gfx_vulkan_unload_shader,
    gfx_vulkan_load_shader,      gfx_vulkan_create_and_load_new_shader,
    gfx_vulkan_lookup_shader,    gfx_vulkan_shader_get_info,
    gfx_vulkan_new_texture,      gfx_vulkan_select_texture,
    gfx_vulkan_upload_texture,   gfx_vulkan_set_sampler_parameters,
    gfx_vulkan_set_depth_test,   gfx_vulkan_set_depth_mask,
    gfx_vulkan_set_zmode_decal,  gfx_vulkan_set_viewport,
    gfx_vulkan_set_scissor,      gfx_vulkan_set_use_alpha,
    gfx_vulkan_draw_triangles,   gfx_vulkan_init,
    gfx_vulkan_start_frame
};

struct GfxWindowManagerAPI gfx_glfw = { gfx_glfw_init,
                                        gfx_glfw_main_loop,
                                        gfx_glfw_get_dimensions,
                                        gfx_glfw_handle_events,
                                        gfx_glfw_start_frame,
                                        gfx_glfw_swap_buffers_begin,
                                        gfx_glfw_swap_buffers_end,
                                        gfx_glfw_get_time };