#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <vector>
#include <optional>
#include <set>

#include <cstdint>
#include <limits>
#include <algorithm>

int main() {
    glfwInit();

    //tell glfw we dont want anything to do with opengl
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

    uint32_t extension_count = 0;
    const char** extensions;

    extensions = glfwGetRequiredInstanceExtensions(&extension_count);

    //we'll create vulkan instance
    VkInstance instance;

    //first create the app info
    VkApplicationInfo app_info{};

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};

    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;

    create_info.enabledLayerCount = 0; //leave empty for now

    //attempt to create the instance
    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a vulkan instance!");
    }

    //the surface must be made after the instance

    VkSurfaceKHR surface; //items from the queue will be delivered here

    VkResult surface_result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

    if(surface_result != VK_SUCCESS) {
        throw std::runtime_error("Couldn't create a window surface");
    }

    //now we need to select a graphics card in our system
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    uint32_t device_count = 0;
    
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr); //get how many devices are available

    if(device_count == 0) {
        throw std::runtime_error("No GPU available with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(device_count);

    //array to hold all the device handles
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    //a struct of each queue we are going to use and the index it is at
    struct QueueFamilies {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;
    };

    QueueFamilies indices;

    //array of extensions required for the swap chain to function
    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    SwapChainSupportDetails swap_chain_details;

    for(const auto& device : devices) { //get a pointer to each device
        int i = 0;

        //look for a queue that supports graphics commands on that device
        uint32_t queue_family_count = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);

        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());  

        QueueFamilies device_indices;

        for(const auto& queue_family : queue_families) {
            //determine which queue family index supports graphics
            if(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                device_indices.graphics_family = i;
            }

            //determine which queue family index supports presenting
            VkBool32 present_support = false;

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

            if(present_support) {
                device_indices.present_family = i;
            }

            i++;
        }

        //check swapchaining capability
        bool extensions_support = false;

        uint32_t device_extension_count;

        //get the amount of extensions on this device
        vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extentions(device_extension_count);

        //now we have an array of the extensions available on the device
        vkEnumerateDeviceExtensionProperties(device, nullptr, &device_extension_count, available_extentions.data());
        
        //required extensions for swap chaining
        std::set<std::string> required_extensions(device_extensions.begin(), device_extensions.end());

        for(const auto& extension : available_extentions) {
            required_extensions.erase(extension.extensionName); //remove any we have
        }

        extensions_support = required_extensions.empty(); //if there are no required extensions left, we are good to continue

        //lastly we need to check if the swapchain is even compatible with our surface
        bool swapchain_surface_check = false;

        SwapChainSupportDetails current_details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &current_details.capabilities);

        //query the formats the surface supports
        uint32_t format_count; 

        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

        if(format_count != 0) {
            current_details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, current_details.formats.data());
        }

        //query the supported presentation modes
        uint32_t present_mode_count;

        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        
        if(present_mode_count != 0) {
            current_details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, current_details.present_modes.data());
        }
        
        //general check info for if the device is suitable
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;

        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);

        if(extensions_support) {
            //if the extensions are supported, then check if the surface is compatable with the swapchain
            swapchain_surface_check = (!current_details.formats.empty() && !current_details.present_modes.empty());

            if(swapchain_surface_check) {
                if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && device_features.geometryShader) {
                    //this device has indices for both presenting and graphics
                    if(device_indices.graphics_family.has_value() && device_indices.present_family.has_value()) { 
                        physical_device = device; //set our physical device to this one
                        indices = device_indices; //set our family queue indices to the indices of the current device  
                        swap_chain_details = current_details;
                        break;
                    }
                }
            }
        } 
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    std::cout << "Successfully initialized device: " << properties.deviceName << std::endl;

    //if we finish the loop and still haven't found a device, throw an error
    if(physical_device == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    //we need to create a logical device to actually use the GPUs hardware
    VkDevice device;

    VkDeviceQueueCreateInfo queue_create_info{}; //information about the queues in the device we need to make

    //for simplicity i am just going to assume the graphics queue does presenting as well
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = indices.graphics_family.value();
    queue_create_info.queueCount = 1;

    float queue_priority = 1.0f;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features{};
    VkDeviceCreateInfo device_create_info{};

    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    device_create_info.pQueueCreateInfos = &queue_create_info; //how many queues we make and what theyre for
    device_create_info.queueCreateInfoCount = 1;

    device_create_info.pEnabledFeatures = &device_features; //what parts of the gpu to enable

    //tell the logical device to enable swap chaining
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    if(vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Couldn't create a logical device");
    }  

    VkQueue graphics_queue;

    vkGetDeviceQueue(device, indices.graphics_family.value(), 0, &graphics_queue);

    //will implement later
    // VkQueue present_queue;

    // vkGetDeviceQueue(device, indices.present_family.value(), 0, &present_queue);

    //summary
    //Physical device GPU
    //Logical device Connection to the GPU. Manages memory, turns on features
    //Queue family A group of queues that only do one thing
    //Queue The queue where work gets done, drop stuff here for the GPU to execute
    //Surface items are delivered to the surface from the queue

    //even if swap chain conditions are all met we need to determine the best settings
    VkSurfaceFormatKHR surface_format = swap_chain_details.formats[0];
    
    for(const auto& available_format : swap_chain_details.formats) { //loop through available formats
        if(available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = available_format;
            break;
        }
    }

    //get the presentation mode
    VkPresentModeKHR present_mode = swap_chain_details.present_modes[0];

    for(const auto& available_present_mode : swap_chain_details.present_modes) {
        if(available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = available_present_mode;
            break;
        }
    }

    //swap extent: the resolution of the swap chain images
    VkExtent2D extent;

        if(swap_chain_details.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            //if width is not the max possible value, the OS has decided how big the drawing surface will be, just use that.
            extent = swap_chain_details.capabilities.currentExtent;
        }
    else {
        //we need to determine it on our own
        int width, height;

        glfwGetFramebufferSize(window, &width, &height); //gives us the size of the window in pixels

        VkExtent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        //clamp it so the resolution of the extent stays where the gpu can actually handle
        actual_extent.width = std::clamp(actual_extent.width, swap_chain_details.capabilities.minImageExtent.width, swap_chain_details.capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, swap_chain_details.capabilities.minImageExtent.height, swap_chain_details.capabilities.maxImageExtent.height);

        extent = actual_extent;
    }

    //now we can create the swap chain

    //how many images we want in the swap chain
    uint32_t image_count = swap_chain_details.capabilities.minImageCount + 1;

    //we ensure we dont ask the gpu for more images than it is capable of handling
    if(swap_chain_details.capabilities.maxImageCount > 0 && image_count > swap_chain_details.capabilities.maxImageCount) {
        image_count = swap_chain_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{};

    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface;

    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};

    if(indices.graphics_family != indices.present_family) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT; //images can be used across multiple queue families
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //an image is owned by one queue family at a time
    }

    swapchain_create_info.preTransform = swap_chain_details.capabilities.currentTransform; //we dont want any transformations on the image

    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //ignore the alpha

    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE; //we dont care about pixels which are obscured

    //its possible that the swapchain becomes invalid while the program, is running so we need a backup, ill do this later
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    //now that we have the settings, make the swapchain
    VkSwapchainKHR swapchain;

    if(vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Could not create Vulkan swapchain");
    }

    //now that its made lets get the handles of the VkImages inside
    std::vector<VkImage> swapchain_images;

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);

    swapchain_images.resize(image_count);

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

    //to use any VkImage, we need an image view. this object describes how to access the VkImage and what part etc to use. we need one for each image
    std::vector<VkImageView> swapchain_imageviews;

    swapchain_imageviews.resize(swapchain_images.size());

    for(size_t i = 0; i < swapchain_images.size(); ++i) {
        VkImageViewCreateInfo imageview_create_info{};

        imageview_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageview_create_info.image = swapchain_images[i]; //tell the view which image it is going to access

        imageview_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; //treat image as 1d, 2d, 3d, or a cube map etc.
        imageview_create_info.format = surface_format.format; 

        //stick to default mapping for RGBA
        imageview_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageview_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageview_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageview_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        imageview_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageview_create_info.subresourceRange.baseMipLevel = 0;
        imageview_create_info.subresourceRange.levelCount = 1;
        imageview_create_info.subresourceRange.baseArrayLayer = 0;
        imageview_create_info.subresourceRange.layerCount = 1;

        //now we can create the view
        if(vkCreateImageView(device, &imageview_create_info, nullptr, &swapchain_imageviews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views");
        }
    }

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    //destroy all the image views we created
    for(auto image_view : swapchain_imageviews) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}