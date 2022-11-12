#ifndef GVU_IMGUI_WIDGETS_H
#define GVU_IMGUI_WIDGETS_H

#include "../Core/Managers/SubBufferManager.h"
#include "../Advanced/VulkanApplicationContext.h"

#include <imgui.h>

namespace gvu
{

/**
 * @brief drawAllocation
 * @param M
 *
 * Draw all the allocations in the sub buffer manager
 * Also provides a button to free the unused allocations
 */
inline void drawAllocation(gvu::SubBufferManager & M, std::string const & name, uint32_t bytesPerPixel)
{

    auto & A = M.allocations();
    ImGui::Begin(name.c_str());

    if(ImGui::Button("Free"))
    {
        M.mergeFreeAllocations();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0,0});

    const uint32_t allocatedColors[] = {0xFF999999,0xFFBBBBBB,0xFFDDDDDD};
    const uint32_t freeColors[] = {0xFF222222,0xFF333333};
    uint32_t i=0;
    uint32_t j=0;

    for(auto &a : A)
    {
        auto remainingSpace = ImGui::GetContentRegionAvail().x;
        auto s = a->allocationSize() / bytesPerPixel;
        ImGui::PushStyleColor(ImGuiCol_Button, a.use_count() == 1 ? freeColors[(++j)%2]: allocatedColors[(++i)%3]);

        ImGui::PushID(&a);
        while(remainingSpace < s)
        {
            ImGui::Button("", { remainingSpace, 0});
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Size: %lu  Use Count: %lu", a->allocationSize(), a.use_count());
            }
            s -= remainingSpace;
            remainingSpace = ImGui::GetContentRegionAvail().x;
        }
        ImGui::Button("", { static_cast<float>(s), 0});
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Size: %lu  Use Count: %lu", a->allocationSize(), a.use_count());
        }
        ImGui::PopID();
        ImGui::PopStyleColor(1);

        if( &a != &A.back() ) ImGui::SameLine();
    }
    ImGui::PopStyleVar(1);
    ImGui::End();
}

inline void drawCubeFaces(gvu::TextureHandle h, uint32_t mip, float w = ImGui::GetContentRegionAvail().x)
{
    w /= 4.0f;
    ImVec2 imgSize = {w,w};

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImGui::Dummy(imgSize);ImGui::SameLine();
    ImGui::Image(h->getSingleImageSet(2, mip), imgSize); ImGui::SameLine();
    ImGui::Dummy(imgSize);ImGui::SameLine();
    ImGui::Dummy(imgSize);


    ImGui::Image(h->getSingleImageSet(1, mip), imgSize); ImGui::SameLine();
    ImGui::Image(h->getSingleImageSet(4, mip), imgSize); ImGui::SameLine();
    ImGui::Image(h->getSingleImageSet(0, mip), imgSize); ImGui::SameLine();
    ImGui::Image(h->getSingleImageSet(5, mip), imgSize);

    ImGui::Dummy(imgSize);ImGui::SameLine();
    ImGui::Image(h->getSingleImageSet(3, mip), imgSize); ImGui::SameLine();
    ImGui::Dummy(imgSize);ImGui::SameLine();
    ImGui::Dummy(imgSize);

    ImGui::PopStyleVar(1);
}

inline void drawContextInfo(std::shared_ptr<VulkanApplicationContext> m_context)
{
    ImGui::Text("Allocated Buffers       : %d", static_cast<int>(m_context->memoryCache.getAllocatedBufferCount()));
    ImGui::Text("Allocated Textures      : %d", static_cast<int>(m_context->memoryCache.getAllocatedTextureCount()));
    ImGui::Text("Command Pools           : %d", static_cast<int>(m_context->commandPoolManager.getCommandPoolCount()));
    ImGui::Text("Active Command Buffers  : %d", static_cast<int>(m_context->commandPoolManager.getActiveCommandBufferCount()));
    ImGui::Text("Returned Command Buffers: %d", static_cast<int>(m_context->commandPoolManager.getReturnedCommandBufferCount()));
    ImGui::Text("Allocated Textures      : %d", static_cast<int>(m_context->memoryCache.getAllocatedTextureCount()));
    ImGui::Text("Descriptor Set Layouts  : %d", static_cast<int>(m_context->descriptorSetLayoutCache.cacheSize()));
    ImGui::Text("RenderPasses            : %d", static_cast<int>(m_context->renderPassCache.cacheSize()));
    ImGui::Text("Samplers                : %d", static_cast<int>(m_context->samplerCache.cacheSize()));
    ImGui::Text("Descriptor Pools        : %d", static_cast<int>(m_context->descriptorSetAllocator.descriptorPoolCount()));
    ImGui::Text("Allocated Sets          : %d", static_cast<int>(m_context->descriptorSetAllocator.descriptorSetCount()));
}

}

#endif
