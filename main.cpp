#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <vector>
#include <optional>
#include <set>

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

        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        //query the formats the surface supports
        uint32_t format_count; 

        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);

        if(format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
        }

        //query the supported presentation modes
        uint32_t present_mode_count;

        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        
        if(present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
        }
        
        //general check info for if the device is suitable
        VkPhysicalDeviceProperties device_properties;
        VkPhysicalDeviceFeatures device_features;

        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceFeatures(device, &device_features);

        if(extensions_support) {
            //if the extensions are supported, then check if the surface is compatable with the swapchain
            swapchain_surface_check = (!details.formats.empty() && !details.present_modes.empty());

            if(swapchain_surface_check) {
                if(device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && device_features.geometryShader) {
                    //this device has indices for both presenting and graphics
                    if(device_indices.graphics_family.has_value() && device_indices.present_family.has_value()) { 
                        physical_device = device; //set our physical device to this one
                        indices = device_indices; //set our family queue indices to the indices of the current device
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

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}