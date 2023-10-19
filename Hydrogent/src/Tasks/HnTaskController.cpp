/*
 *  Copyright 2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "Tasks/HnTaskController.hpp"

#include <atomic>
#include <array>

#include "Tasks/HnRenderTask.hpp"
#include "Tasks/HnPostProcessTask.hpp"
#include "HnTokens.hpp"
#include "HashUtils.hpp"

namespace Diligent
{

namespace USD
{

namespace
{

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    HnTaskControllerTokens,

    (postProcessTask)

    (renderBufferDescriptor)
    (renderTags)
);
// clang-format on

static constexpr std::array<HnTaskController::TaskUID, 4> RenderTaskUIDs = {
    HnTaskController::TaskUID_RenderDefault,
    HnTaskController::TaskUID_RenderMasked,
    HnTaskController::TaskUID_RenderAdditive,
    HnTaskController::TaskUID_RenderTranslucent,
};

} // namespace

class HnTaskController::TaskParamsDelegate final : public pxr::HdSceneDelegate
{
public:
    TaskParamsDelegate(pxr::HdRenderIndex& Index,
                       const pxr::SdfPath& Id) :
        pxr::HdSceneDelegate{&Index, Id}
    {}

    ~TaskParamsDelegate() override final
    {
    }

    template <typename T>
    void SetParameter(const pxr::SdfPath& Id, const pxr::TfToken& ValueKey, T&& Value)
    {
        m_ParamsCache[{Id, ValueKey}] = std::forward<T>(Value);
    }

    template <typename T>
    T GetParameter(const pxr::SdfPath& Id, const TfToken& ValueKey) const
    {
        auto it = m_ParamsCache.find({Id, ValueKey});
        if (it == m_ParamsCache.end())
        {
            UNEXPECTED("Parameter ", ValueKey, " is not set for ", Id);
            return {};
        }

        VERIFY(it->second.IsHolding<T>(), "Unexpected parameter type");
        return it->second.Get<T>();
    }

    bool HasParameter(const pxr::SdfPath& Id, const TfToken& ValueKey) const
    {
        return m_ParamsCache.find({Id, ValueKey}) != m_ParamsCache.end();
    }

    pxr::VtValue Get(const pxr::SdfPath& Id, const TfToken& ValueKey) override final
    {
        auto it = m_ParamsCache.find({Id, ValueKey});
        return it != m_ParamsCache.end() ? it->second : pxr::VtValue{};
    }

    pxr::GfMatrix4d GetTransform(const pxr::SdfPath& Id) override final
    {
        auto it = m_ParamsCache.find({Id, pxr::HdTokens->transform});
        return it != m_ParamsCache.end() ? it->second.Get<pxr::GfMatrix4d>() : pxr::GfMatrix4d{1.0};
    }

    pxr::VtValue GetLightParamValue(const pxr::SdfPath& Id,
                                    const pxr::TfToken& ParamName) override
    {
        return Get(Id, ParamName);
    }

    bool IsEnabled(const TfToken& Option) const override final
    {
        return HdSceneDelegate::IsEnabled(Option);
    }

    pxr::HdRenderBufferDescriptor GetRenderBufferDescriptor(const pxr::SdfPath& Id) override final
    {
        return GetParameter<pxr::HdRenderBufferDescriptor>(Id, HnTaskControllerTokens->renderBufferDescriptor);
    }

    pxr::TfTokenVector GetTaskRenderTags(const pxr::SdfPath& TaskId) override final
    {
        if (HasParameter(TaskId, HnTaskControllerTokens->renderTags))
        {
            return GetParameter<pxr::TfTokenVector>(TaskId, HnTaskControllerTokens->renderTags);
        }
        return pxr::TfTokenVector{};
    }

private:
    struct ParamKey
    {
        const pxr::SdfPath Path;
        const pxr::TfToken ValueKey;
        const size_t       Hash;

        ParamKey(const pxr::SdfPath& _Path, const pxr::TfToken& _ValueKey) :
            Path{_Path},
            ValueKey{_ValueKey},
            Hash{ComputeHash(pxr::SdfPath::Hash{}(_Path), pxr::TfToken::HashFunctor{}(_ValueKey))}
        {}

        bool operator==(const ParamKey& rhs) const
        {
            return Hash && rhs.Hash && Path == rhs.Path && ValueKey == rhs.ValueKey;
        }

        struct Hasher
        {
            size_t operator()(const ParamKey& Key) const
            {
                return Key.Hash;
            }
        };
    };

    std::unordered_map<ParamKey, pxr::VtValue, ParamKey::Hasher> m_ParamsCache;
};


HnTaskController::HnTaskController(pxr::HdRenderIndex& RenderIndex,
                                   const pxr::SdfPath& ControllerId) :
    m_RenderIndex{RenderIndex},
    m_ControllerId{ControllerId},
    m_ParamsDelegate{std::make_unique<TaskParamsDelegate>(RenderIndex, ControllerId)}
{
    CreateRenderTask(HnMaterialTagTokens->defaultTag, TaskUID_RenderDefault);
    CreateRenderTask(HnMaterialTagTokens->masked, TaskUID_RenderMasked);
    CreateRenderTask(HnMaterialTagTokens->additive, TaskUID_RenderAdditive);
    CreateRenderTask(HnMaterialTagTokens->translucent, TaskUID_RenderTranslucent);

    CreatePostProcessTask();

    m_DefaultTaskOrder =
        {
            TaskUID_RenderDefault,
            TaskUID_RenderMasked,
            TaskUID_RenderAdditive,
            TaskUID_RenderTranslucent,
            TaskUID_PostProcess,
        };
}

HnTaskController::~HnTaskController()
{
    // Remove all tasks from the render index
    for (const auto& it : m_TaskIds)
    {
        m_RenderIndex.RemoveTask(it.second);
    }
    m_TaskIds.clear();
}

pxr::HdTaskSharedPtr HnTaskController::GetTask(TaskUID UID) const
{
    auto it = m_TaskIds.find(UID);
    return it == m_TaskIds.end() ?
        m_RenderIndex.GetTask(it->second) :
        nullptr;
}

void HnTaskController::RemoveTask(TaskUID UID)
{
    auto it = m_TaskIds.find(UID);
    if (it == m_TaskIds.end())
        return;

    m_RenderIndex.RemoveTask(it->second);
    m_TaskIds.erase(it);
}

pxr::SdfPath HnTaskController::GetRenderTaskId(const pxr::TfToken& MaterialTag) const
{
    std::string Id = std::string{"renderTask_"} + MaterialTag.GetString();
    std::replace(Id.begin(), Id.end(), ':', '_');
    return GetControllerId().AppendChild(TfToken{Id});
}

void HnTaskController::SetParameter(const pxr::SdfPath& Id, const TfToken& ValueKey, pxr::VtValue Value)
{
    m_ParamsDelegate->SetParameter(Id, ValueKey, std::move(Value));
}

void HnTaskController::CreateRenderTask(const pxr::TfToken& MaterialTag, TaskUID UID)
{
    const pxr::SdfPath RenderTaskId = GetRenderTaskId(MaterialTag);
    // Note that we pass the delegate to the scene index. This delegate will be passed
    // to the task's Sync() method.
    CreateTask<HnRenderTask>(RenderTaskId, UID);

    HnRenderTaskParams TaskParams;

    pxr::HdRprimCollection Collection{
        pxr::HdTokens->geometry,
        pxr::HdReprSelector{pxr::HdReprTokens->smoothHull},
        false, // forcedRepr
        MaterialTag,
    };
    Collection.SetRootPath(pxr::SdfPath::AbsoluteRootPath());

    pxr::TfTokenVector RenderTags = {pxr::HdRenderTagTokens->geometry};

    m_ParamsDelegate->SetParameter(RenderTaskId, pxr::HdTokens->params, TaskParams);
    m_ParamsDelegate->SetParameter(RenderTaskId, pxr::HdTokens->collection, Collection);
    m_ParamsDelegate->SetParameter(RenderTaskId, pxr::HdTokens->renderTags, RenderTags);
}

void HnTaskController::CreatePostProcessTask()
{
    const pxr::SdfPath PostProcessTaskId = GetControllerId().AppendChild(HnTaskControllerTokens->postProcessTask);
    CreateTask<HnPostProcessTask>(PostProcessTaskId, TaskUID_PostProcess);

    HnPostProcessTaskParams TaskParams;
    m_ParamsDelegate->SetParameter(PostProcessTaskId, pxr::HdTokens->params, TaskParams);
}

const pxr::HdTaskSharedPtrVector HnTaskController::GetTasks(const std::vector<TaskUID>* TaskOrder) const
{
    if (TaskOrder == nullptr)
        TaskOrder = &m_DefaultTaskOrder;

    pxr::HdTaskSharedPtrVector Tasks;
    for (auto UID : *TaskOrder)
    {
        auto it = m_TaskIds.find(UID);
        if (it == m_TaskIds.end())
            continue;

        Tasks.push_back(m_RenderIndex.GetTask(it->second));
    }

    return Tasks;
}

void HnTaskController::SetCollection(const pxr::HdRprimCollection& Collection)
{
    pxr::HdRprimCollection NewCollection = Collection;
    for (const auto UID : RenderTaskUIDs)
    {
        auto it = m_TaskIds.find(UID);
        if (it == m_TaskIds.end())
            continue;

        const auto& TaskId = it->second;

        pxr::HdRprimCollection OldCollection = m_ParamsDelegate->GetParameter<pxr::HdRprimCollection>(TaskId, pxr::HdTokens->collection);

        const pxr::TfToken& OldMaterialTag = OldCollection.GetMaterialTag();
        NewCollection.SetMaterialTag(OldMaterialTag);

        if (OldCollection == NewCollection)
            continue;

        m_ParamsDelegate->SetParameter(TaskId, pxr::HdTokens->collection, NewCollection);
        m_RenderIndex.GetChangeTracker().MarkTaskDirty(TaskId, pxr::HdChangeTracker::DirtyCollection);
    }
}

void HnTaskController::SetRenderParams(const HnRenderTaskParams& Params)
{
    for (const auto UID : RenderTaskUIDs)
    {
        auto it = m_TaskIds.find(UID);
        if (it == m_TaskIds.end())
            continue;

        const auto& TaskId = it->second;

        HnRenderTaskParams OldParams = m_ParamsDelegate->GetParameter<HnRenderTaskParams>(TaskId, pxr::HdTokens->params);
        if (OldParams == Params)
            continue;

        m_ParamsDelegate->SetParameter(TaskId, pxr::HdTokens->params, Params);
        m_RenderIndex.GetChangeTracker().MarkTaskDirty(TaskId, pxr::HdChangeTracker::DirtyParams);
    }
}

void HnTaskController::SetPostProcessParams(const HnPostProcessTaskParams& Params)
{
    const auto it = m_TaskIds.find(TaskUID_PostProcess);
    if (it == m_TaskIds.end())
        return;

    const auto& TaskId = it->second;

    HnPostProcessTaskParams OldParams = m_ParamsDelegate->GetParameter<HnPostProcessTaskParams>(TaskId, pxr::HdTokens->params);
    if (OldParams == Params)
        return;

    m_ParamsDelegate->SetParameter(TaskId, pxr::HdTokens->params, Params);
    m_RenderIndex.GetChangeTracker().MarkTaskDirty(TaskId, pxr::HdChangeTracker::DirtyParams);
}

void HnTaskController::SetRenderTags(const pxr::TfTokenVector& RenderTags)
{
    for (const auto UID : RenderTaskUIDs)
    {
        auto it = m_TaskIds.find(UID);
        if (it == m_TaskIds.end())
            continue;

        const auto& TaskId = it->second;

        pxr::TfTokenVector OldRenderTags = m_ParamsDelegate->GetParameter<pxr::TfTokenVector>(TaskId, pxr::HdTokens->renderTags);
        if (OldRenderTags == RenderTags)
            continue;

        m_ParamsDelegate->SetParameter(TaskId, pxr::HdTokens->renderTags, RenderTags);
        m_RenderIndex.GetChangeTracker().MarkTaskDirty(TaskId, pxr::HdChangeTracker::DirtyRenderTags);
    }
}

} // namespace USD

} // namespace Diligent
