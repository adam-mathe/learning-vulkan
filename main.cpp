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

#include <fstream>

//load shaders
static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary); //read as binary from the end of the file

    if(!file.is_open()) {
        throw std::runtime_error("Failed to open file");
    }

    size_t file_size = (size_t) file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
}

VkShaderModule makeShaderModule(VkDevice& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo create_info;

    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;

    if(vkCreateShaderModule(device, &create_info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Could not create shader module");
    }

    return module;
}

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

    //now that we have shaders in the spv type, we need to load them
    auto vert_shader_code = readFile("vert.spv");
    auto frag_shader_code = readFile("frag.spv");

    //to pass this shader code to vulkan, we need to wrap it in a module
    VkShaderModule vert_shader_module = makeShaderModule(device, vert_shader_code);
    VkShaderModule frag_shader_module = makeShaderModule(device, frag_shader_code);

    //now to use these shaders we have to assign them to a certain stage of the rendering pipeline
    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};

    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};

    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    //put these stages into an array
    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    //dynamic states allow us to change specific settings without rebuilding the ENTIRE graphics pipeline
    std::vector<VkDynamicState> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};

    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();
    //now we can change that data at drawing time later

    //for everything else in the pipeline we will use fixed settings
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};

    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;
    //our vertices are coming from the shader, not the cpu

    //now we need to tell the pipeline how to treat the vertices
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};

    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; //every 3 vertices is 1 triangle
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{};

    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = nullptr; //using dynamic state
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = nullptr; //using dynamic state

    //Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE; //we want to draw
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE; //solid, line, or point
    rasterizer.lineWidth = 1.0f; //only matters if drawing lines
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; //if facing away from camera, dont draw
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    //not needed for a triangle
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    //multisampling or antialisaing. not going to do this for the first triangle
    VkPipelineMultisampleStateCreateInfo multisampling{};

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE; //disable it
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    //color blending, i will skip this for now
    VkPipelineColorBlendAttachmentState color_blend_attachment{};

    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};

    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    //now make the pipeline layout
    VkPipelineLayout pipeline_layout{};

    VkPipelineLayoutCreateInfo pipeline_layout_info{};

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //nothing for now

    if(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    //tell the gpu what we are drawing and how to handle the memory
    VkAttachmentDescription color_attachment{};

    color_attachment.format = surface_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    //a render pass can have multiple sub passes
    VkAttachmentReference color_attachment_ref{};

    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    //now we can make the render pass
    VkRenderPass render_pass;
    
    VkRenderPassCreateInfo render_pass_info{};

    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    //make the render pass
    if(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    //finally we can make the pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;

    //all the settings we defined
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = nullptr;

    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;

    VkPipeline graphics_pipeline;

    //make the pipeline
    if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Could not create the graphics pipeline");
    }

    //we have set up our render pass to expect a single format buffer, we need to set it up
    //the frame buffer links the render pass to the swap chain images

    std::vector<VkFramebuffer> swapchain_frame_buffers;

    swapchain_frame_buffers.resize(swapchain_imageviews.size());

    for(size_t i = 0; i < swapchain_imageviews.size(); ++i) {
        VkImageView attachments[] = {
            swapchain_imageviews[i]
        };

        VkFramebufferCreateInfo framebuffer_info{};

        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = extent.width;
        framebuffer_info.height = extent.height;
        framebuffer_info.layers = 1;


        if(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapchain_frame_buffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Could not create frame buffer");
        }
    }

    //we need a command pool for command buffers
    VkCommandPool command_pool;

    VkCommandPoolCreateInfo pool_info{};

    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = indices.graphics_family.value();

    if(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the command pool");
    }
    
    //now make the command buffer
    VkCommandBuffer command_buffer;

    VkCommandBufferAllocateInfo alloc_info{};

    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    //now we need to record the command buffer, we'll do it once here because the triangle is not moving
    VkCommandBufferBeginInfo begin_info{};

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    //Sync objects. make the cpu wait for the gpu to finish
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence inflight_fence;

    VkSemaphoreCreateInfo semaphore_info{};

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};

    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if(vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphore) != VK_SUCCESS || vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphore) != VK_SUCCESS | vkCreateFence(device, &fence_info, nullptr, &inflight_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphores");
    }



    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        //draw the frame
        vkWaitForFences(device, 1, &inflight_fence, VK_TRUE, UINT64_MAX);

        vkResetFences(device, 1, &inflight_fence);

        //now we need to acquire an image from the swap chain
        uint32_t image_index;

        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);

        vkResetCommandBuffer(command_buffer, 0);

        //record command buffer
        //in this command buffer we are going to tell the gpu to:
        //start the render pass, bind the pipeline, draw, and then end the render pass
        if(vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo render_pass_begin_info{};

        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = swapchain_frame_buffers[image_index]; //change the 0 later
        render_pass_begin_info.renderArea.offset = {0,0};
        render_pass_begin_info.renderArea.extent = extent;

        VkClearValue clear_color = {{0.2f, 0.2f, 0.2f, 1.0f}};

        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        //bind the graphics pipeline
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

        //we left viewport and scissor dynamic so:
        VkViewport viewport{};

        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) extent.width;
        viewport.height = (float) extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};

        //we want the scissor rectangle to cover the entire framebuffer
        scissor.offset = {0,0};
        scissor.extent = extent;

        vkCmdSetScissor(command_buffer, 0, 1, &scissor);   

        vkCmdDraw(command_buffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(command_buffer);

        if(vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }

        //now we need to submit the command buffer
        VkSubmitInfo submit_info{};

        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = { image_available_semaphore };
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        VkSemaphore signal_semaphores[] = { render_finished_semaphore };

        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if(vkQueueSubmit(graphics_queue, 1, &submit_info, inflight_fence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer");
        }

        VkSubpassDependency dependency{};

        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;

        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        //submit the result back to the swapchain
        VkPresentInfoKHR present_info{};

        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &image_index;

        present_info.pResults = nullptr;

        vkQueuePresentKHR(graphics_queue, &present_info);
    }

    //destroy semaphores

    vkDestroyCommandPool(device, command_pool, nullptr);

    //destroy the frame buffers we created for each image view
    for(auto framebuffer : swapchain_frame_buffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    //destroy all the image views we created
    for(auto image_view : swapchain_imageviews) {
        vkDestroyImageView(device, image_view, nullptr);
    }

    vkDestroyPipeline(device, graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyShaderModule(device, vert_shader_module, nullptr);
    vkDestroyShaderModule(device, frag_shader_module, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}