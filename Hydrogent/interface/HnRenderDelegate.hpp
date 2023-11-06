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

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <atomic>
#include <mutex>

#include "pxr/imaging/hd/renderDelegate.h"

#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "../../../DiligentCore/Graphics/GraphicsTools/interface/RenderStateCache.h"
#include "../../../DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../../PBR/interface/USD_Renderer.hpp"

#include "HnTextureRegistry.hpp"

namespace Diligent
{

namespace USD
{

class HnMaterial;
class HnMesh;

/// USD render delegate implementation in Hydrogent.
class HnRenderDelegate final : public pxr::HdRenderDelegate
{
public:
    struct CreateInfo
    {
        IRenderDevice*     pDevice           = nullptr;
        IDeviceContext*    pContext          = nullptr;
        IRenderStateCache* pRenderStateCache = nullptr;
        IBuffer*           pCameraAttribs    = nullptr;
        IBuffer*           pLightAttribs     = nullptr;
    };
    static std::unique_ptr<HnRenderDelegate> Create(const CreateInfo& CI);

    HnRenderDelegate(const CreateInfo& CI);

    virtual ~HnRenderDelegate() override final;

    // Returns a list of typeId's of all supported Rprims by this render
    // delegate.
    virtual const pxr::TfTokenVector& GetSupportedRprimTypes() const override final;

    // Returns a list of typeId's of all supported Sprims by this render
    // delegate.
    virtual const pxr::TfTokenVector& GetSupportedSprimTypes() const override final;

    // Returns a list of typeId's of all supported Bprims by this render
    // delegate.
    virtual const pxr::TfTokenVector& GetSupportedBprimTypes() const override final;

    // Returns a shared ptr to the resource registry of the current render
    // delegate.
    virtual pxr::HdResourceRegistrySharedPtr GetResourceRegistry() const override final;


    //
    // Renderpass Factory
    //

    // Request to create a new renderpass.
    // \param Index      - the render index to bind to the new renderpass.
    // \param Collection - the rprim collection to bind to the new renderpass.
    // \return A shared pointer to the new renderpass or empty on error.
    //
    virtual pxr::HdRenderPassSharedPtr CreateRenderPass(pxr::HdRenderIndex*           Index,
                                                        const pxr::HdRprimCollection& Collection) override final;


    //
    // Instancer Factory
    //

    // Request to create a new instancer.
    // \param  Id - The unique identifier of this instancer.
    // \return A pointer to the new instancer or nullptr on error.
    virtual pxr::HdInstancer* CreateInstancer(pxr::HdSceneDelegate* Delegate,
                                              const pxr::SdfPath&   Id) override final;

    virtual void DestroyInstancer(pxr::HdInstancer* instancer) override final;

    ///////////////////////////////////////////////////
    //
    // Prim Factories
    //
    ///////////////////////////////////////////////////


    // Request to Allocate and Construct a new Rprim.
    // \param TypeId  - the type identifier of the prim to allocate
    // \param RPrimId - a unique identifier for the prim
    // \return A pointer to the new prim or nullptr on error.
    virtual pxr::HdRprim* CreateRprim(const pxr::TfToken& TypeId,
                                      const pxr::SdfPath& RPrimId) override final;

    // Request to Destruct and deallocate the prim.
    virtual void DestroyRprim(pxr::HdRprim* rPrim) override final;

    // Request to Allocate and Construct a new Sprim.
    // \param TypeId  - the type identifier of the prim to allocate
    // \param SPrimId - a unique identifier for the prim
    // \return A pointer to the new prim or nullptr on error.
    virtual pxr::HdSprim* CreateSprim(const pxr::TfToken& TypeId,
                                      const pxr::SdfPath& SPrimId) override final;

    // Request to Allocate and Construct an Sprim to use as a standin, if there
    // if an error with another another Sprim of the same type.  For example,
    // if another prim references a non-exisiting Sprim, the fallback could
    // be used.
    //
    // \param TypeId  - the type identifier of the prim to allocate
    // \return A pointer to the new prim or nullptr on error.
    virtual pxr::HdSprim* CreateFallbackSprim(const pxr::TfToken& TypeId) override final;

    // Request to Destruct and deallocate the prim.
    virtual void DestroySprim(pxr::HdSprim* sprim) override final;

    // Request to Allocate and Construct a new Bprim.
    // \param TypeId  - the type identifier of the prim to allocate
    // \param BPrimId - a unique identifier for the prim
    // \return A pointer to the new prim or nullptr on error.
    virtual pxr::HdBprim* CreateBprim(const pxr::TfToken& TypeId,
                                      const pxr::SdfPath& BPrimId) override final;


    // Request to Allocate and Construct a Bprim to use as a standin, if there
    // if an error with another another Bprim of the same type.  For example,
    // if another prim references a non-exisiting Bprim, the fallback could
    // be used.
    //
    // \param TypeId - the type identifier of the prim to allocate
    // \return A pointer to the new prim or nullptr on error.
    virtual pxr::HdBprim* CreateFallbackBprim(const pxr::TfToken& TypeId) override final;

    // Request to Destruct and deallocate the prim.
    virtual void DestroyBprim(pxr::HdBprim* BPrim) override final;

    //
    // Sync, Execute & Dispatch Hooks
    //

    // Notification point from the Engine to the delegate.
    // This notification occurs after all Sync's have completed and
    // before task execution.
    //
    // This notification gives the Render Delegate a chance to
    // update and move memory that the render may need.
    //
    // For example, the render delegate might fill primvar buffers or texture
    // memory.
    virtual void CommitResources(pxr::HdChangeTracker* Tracker) override final;

    HnTextureRegistry& GetTextureRegistry() { return m_TextureRegistry; }

    const pxr::SdfPath* GetRPrimId(Uint32 UID) const;

    std::shared_ptr<USD_Renderer> GetUSDRenderer() { return m_USDRenderer; }

    IRenderDevice*     GetDevice() const { return m_pDevice; }
    IDeviceContext*    GetDeviceContext() const { return m_pContext; }
    IRenderStateCache* GetRenderStateCache() const { return m_pRenderStateCache; }
    IBuffer*           GetCameraAttribsCB() const { return m_CameraAttribsCB; }
    IBuffer*           GetLightAttribsCB() const { return m_LightAttribsCB; }

private:
    static const pxr::TfTokenVector SupportedRPrimTypes;
    static const pxr::TfTokenVector SupportedSPrimTypes;
    static const pxr::TfTokenVector SupportedBPrimTypes;

    RefCntAutoPtr<IRenderDevice>     m_pDevice;
    RefCntAutoPtr<IDeviceContext>    m_pContext;
    RefCntAutoPtr<IRenderStateCache> m_pRenderStateCache;

    RefCntAutoPtr<IBuffer>        m_CameraAttribsCB;
    RefCntAutoPtr<IBuffer>        m_LightAttribsCB;
    std::shared_ptr<USD_Renderer> m_USDRenderer;

    HnTextureRegistry m_TextureRegistry;

    std::atomic<Uint32>                      m_RPrimNextUID{1};
    mutable std::mutex                       m_RPrimUIDToSdfPathMtx;
    std::unordered_map<Uint32, pxr::SdfPath> m_RPrimUIDToSdfPath;

    std::mutex                  m_MeshesMtx;
    std::unordered_set<HnMesh*> m_Meshes;

    std::mutex                      m_MaterialsMtx;
    std::unordered_set<HnMaterial*> m_Materials;
};

} // namespace USD

} // namespace Diligent