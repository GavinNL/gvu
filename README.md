# GVU - Gavin's Vulkan Utilities

This is a set of utility classes/managers which can help organize your vulkan application

# Include Header Structure

* gvu
  * Core - Only requires vulkan headers and std
    * Cache 
    * Managers - 
  * Advanced - Advanced classes
  * Extensions - Requires additional libraries



# Caches

Holds reusable objects. It works by hashing the CreateInfo struct for the particular Vulkan object. Most of these caches will provide their own CreateInfo struct which is based off the standard Vulkan XXXXCreateInfo. Instead of using raw pointers, the provided CreateInfos will use vectors of objects. These CreateInfo structs are hashable. If two CreateInfo structs are provided and generate the same hash, it will return the previously created object instead of creating a new one.


 * SamplerCache
 * DescriptorSetLayoutCache
 * RenderpassCache
 * PipelineLayoutCache


Most caches work in a simlar fashion. Initialize it with the init() function, and then call the create() function with the appropriate CreateInfo struct. 

Each cache also defines their own CreateInfo struct which is based off the regular vulkan structs. Instead of having to manually set the pointers and manage arrays, these structs provide vectors for each of the member functions. The pointers are automatically set during creation time.

```cpp
gvu::DescriptorSetLayoutCache cache;
cache.init( device );

gvu::DescriptorSetLayoutCreateInfo ci;
ci.bindings.emplace_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});

auto layout = cache.createDescriptorSetLayout(ci);

auto layout2 = cache.createDescriptorSetLayout(ci);

assert(layout == layout2);

cache.destroy();

```

# Managers

Managers are utility classes which help you work with other objects. 
They provide convenience functions to either create/allocate/update the objects they manage

## CommandPoolManager

The *CommandPoolManager* is used to manage command pools and command buffers. 

**How it works**: 


## DescriptorPoolManager

Regular DescriptorPools allow you to allocate any kind of descriptor set as long as there are enough
descriptors of the specific type you are requesting.

The DescriptorPoolManager, is used to allocate DescriptorSets of a a SINGLE layout. 

**How it works**: After you initialize it with the given layout you want to allocate, the manger will allocate descriptor sets from a pool of a specific size. Once the pool has been exhausted, it will create a new pool.

When you are done with the descriptor set, you can `return` it to the pool. When all descriptor sets from a specific pool have been returned, it will reset the pool.


```cpp
// create a layout cache
gvu::DescriptorSetLayoutCache dlayoutCache;
dlayoutCache.init(window->getDevice());


gvu::DescriptorPoolManager poolManager;


gvu::DescriptorSetLayoutCreateInfo dci;
dci.bindings.emplace_back(VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
dci.bindings.emplace_back(VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr});
auto dLayout1 = dlayoutCache.create(dci);


poolManager.init(window->getDevice(), &dlayoutCache, dLayout1, 3);
```

You probably do not want to use the DescriptorPoolManager on its own. It is used within the `DescriptorSetManager` to manage all descriptor sets.

## DescriptorSetManager

**How it works**: This manger internally handles a DescriptorPoolManager for each layout you want to use. You can allocate descriptor sets by passing it in either a `DescriptorSetLayout` or a `DescriptorSetLayoutCreateInfo` struct.


# Extensions

## Memory Cache

The Memory Cache is a manager which should be used to allocate memory-backed objects (images/buffers). Under the hood, it uses [Vulkan Memory Allocator](https://gpuopen.com/vulkan-memory-allocator/) but provides a few additional features.

**How it Works**: You allocate images/buffers from the Memory Cache, then when you are finished with them. You can return it to the cache. The image memory is not freed right away. Instead, it is held until you request a new image with the same dimensions/format/usage. 

Objects returned by the Memory Cache as provided as `shared_ptrs`. These are reference counted. When the reference count goes to `1`, the object is considered unused and can be returned by a subsequent call to the memory cache's allocate() method.




```mermaid
graph TD
    CommandBufferManager --> |requires| ImageCache
    ImageArrayManager --> |requires| ImageCache
    ImageCache --> |requires| vmaMemoryAllocator
    RenderPassCache
    SamplerCache
    DescriptorSetLayoutCache
    PipelineLayoutCache --> |requires| DescriptorSetLayoutCache
    DescriptorPoolManager --> |requires| DescriptorSetLayoutCache
```
