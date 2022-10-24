#ifndef GVU_IMGUI_WIDGETS_H
#define GVU_IMGUI_WIDGETS_H

#include "../Core/Managers/SubBufferManager.h"
#include <imgui.h>

namespace gul
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

}

#endif
