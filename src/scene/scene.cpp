#include "scene.h"
#include "mesh.h"
#include "material.h"
#include "materialparameter.h"
#include "../engine/pixelformat.h"
#include "../engine/logging.h"
#include "../render/rendersystem.h"
#include "../render/pixelbuffer.h"
#include "../render/shaderprogram.h"
#include "foundation/math.h"
#include "foundation/color.h"
#include "foundation/exception.h"
#include <queue>

GF_NAMESPACE_BEGIN

SceneAppContext sceneAppContext;

void RenderWorld::addEntity(const std::shared_ptr<RenderEntity>& entity)
{
    const auto notFound = std::cend(entities_);
    const auto r = std::equal_range(std::cbegin(entities_), std::cend(entities_), entity, entities_.comp());
    if (r.first == r.second)
    {
        entities_.insert(entity);
    }
}

void RenderWorld::removeEntity(const std::shared_ptr<RenderEntity>& entity)
{
    const auto r = std::equal_range(std::cbegin(entities_), std::cend(entities_), entity, entities_.comp());
    entities_.erase(r.first, r.second);
}

void RenderWorld::clearEntities()
{
    entities_.clear();
}

void RenderWorld::draw(const RenderCamera& camera)
{
    auto& graphics = sceneAppContext.graphicsCommandBuilder();
    auto& backBuffer = sceneAppContext.backBuffer();
    auto& depthTarget = sceneAppContext.depthTarget();

    graphics.clearDepthTarget(depthTarget, 1);

    const auto view_T = camera.view.transpose();
    const auto proj_T = camera.proj.transpose();

    for (const auto& entity : entities_)
    {
        const auto mesh = entity->mesh;
        if (!mesh.useable())
        {
            continue;
        }

        const auto vertexData = mesh->vertexData();
        const auto world_T = entity->worldMatrix.transpose();
        for (auto subMeshes = mesh->subMeshes(); subMeshes.first != subMeshes.second; ++subMeshes.first)
        {
            auto& subMesh = *subMeshes.first;
            if (!subMesh.material.useable())
            {
                continue;
            }

            subMesh.material->directNumeric(ShaderType::vertex, SystemMatParam::WORLD, &world_T, sizeof(Matrix44));
            subMesh.material->directNumeric(ShaderType::vertex, SystemMatParam::VIEW, &view_T, sizeof(Matrix44));
            subMesh.material->directNumeric(ShaderType::vertex, SystemMatParam::PROJ, &proj_T, sizeof(Matrix44));

            auto drawCall = subMesh.material->drawCallSource();
            drawCall.setViewport(camera.viewport);
            drawCall.setRenderTarget(backBuffer);
            drawCall.setDepthTarget(depthTarget);

            if (subMesh.indexed)
            {
                drawCall.setVertexIndexed(*vertexData, 0, subMesh.offset, subMesh.count, 0, 1);
            }
            else
            {
                drawCall.setVertex(*vertexData, subMesh.offset, subMesh.count, 0, 1);
            }

            try
            {
                graphics.triggerDrawCall(drawCall);
            }
            catch (const Direct3DException& e)
            {
                GF_LOG_WARN("Failed to trigger draw call. {}", e.msg());
            }
        }
    }
}

void SceneAppContext::startup()
{
    try
    {
        auto& graphicsExe = renderSystem.commandExecuter(GpuCommandType::graphics);
        auto& copyExe = renderSystem.commandExecuter(GpuCommandType::copy);
        const auto numFrameBuffers = renderSystem.numBackBuffers();
        const auto frameIndex = renderSystem.currentFrameIndex();

        frameResources_.resize(numFrameBuffers);
        for (size_t i = 0; i < numFrameBuffers; ++i)
        {
            frameResources_[i].graphicsCommands = graphicsExe.createCommands();
            frameResources_[i].fenceValue_ = 0;
        }
        copyCommands_ = copyExe.createCommands();

        graphicsCommandBuilder_ = graphicsExe.createBuilder();
        copyCommandBuilder_ = copyExe.createBuilder();

        graphicsCommandBuilder_->record(*frameResources_[frameIndex].graphicsCommands);
        copyCommandBuilder_->record(*copyCommands_);
    }
    catch (const Direct3DError& e)
    {
        GF_LOG_ERROR("SceneManager initialization error. {}", e.msg());
        throw;
    }
    catch (const Direct3DException& e)
    {
        GF_LOG_ERROR("SceneManager initialization error. {}", e.msg());
        throw;
    }
    
    const auto fullySize = renderSystem.fullyViewport();
    PixelBufferSetup depthSetup = {};
    depthSetup.width = static_cast<size_t>(fullySize.width + EPSILON);
    depthSetup.height = static_cast<size_t>(fullySize.height + EPSILON);
    depthSetup.baseFormat = PixelFormat::R32;
    depthSetup.dtvFormat = PixelFormat::D32_float;
    depthSetup.arrayLength = 1;
    depthSetup.mipLevels = 1;
    depthSetup.flags = PBF_AllowDepthTarget | PBF_DenyShaderResource;
    depthSetup.state = PixelBufferState::depthWrite;
    depthTarget_ = std::make_unique<PixelBuffer>(depthSetup, 1.f);

    graphicsCommandBuilder_->transition(backBuffer(), PixelBufferState::present, PixelBufferState::renderTarget);
    graphicsCommandBuilder_->clearRenderTarget(backBuffer(), { 0, 0, 0, 1 });

    GF_LOG_INFO("SceneManager initialized.");
}

void SceneAppContext::shutdown()
{
    depthTarget_.reset();
    copyCommandBuilder_.reset();
    copyCommands_.reset();
    graphicsCommandBuilder_.reset();
    frameResources_.clear();

    GF_LOG_INFO("SceneManager shutdown.");
}

GpuCommandBuilder& SceneAppContext::graphicsCommandBuilder()
{
    return *graphicsCommandBuilder_;
}

GpuCommandBuilder& SceneAppContext::copyCommandBuilder()
{
    return *copyCommandBuilder_;
}

PixelBuffer& SceneAppContext::depthTarget()
{
    return *depthTarget_;
}

PixelBuffer& SceneAppContext::backBuffer()
{
    return renderSystem.backBuffer(renderSystem.currentFrameIndex());
}

void SceneAppContext::executeCommandsAndPresent()
{
    graphicsCommandBuilder_->transition(backBuffer(), PixelBufferState::renderTarget, PixelBufferState::present);

    const auto frameIndex = renderSystem.currentFrameIndex();

    auto& graphicsExe = renderSystem.commandExecuter(GpuCommandType::graphics);
    auto& copyExe = renderSystem.commandExecuter(GpuCommandType::copy);

    graphicsCommandBuilder_->close();
    copyCommandBuilder_->close();

    const auto copyCompleted = copyExe.execute(*copyCommandBuilder_);
    copyExe.waitForFenceCompletion(copyCompleted);

    frameResources_[frameIndex].fenceValue_ = graphicsExe.execute(*graphicsCommandBuilder_);

    renderSystem.present();

    const auto newFrameIndex = renderSystem.currentFrameIndex();
    graphicsExe.waitForFenceCompletion(frameResources_[newFrameIndex].fenceValue_);

    frameResources_[newFrameIndex].graphicsCommands->reset();
    graphicsCommandBuilder_->record(*frameResources_[newFrameIndex].graphicsCommands);

    copyCommands_->reset();
    copyCommandBuilder_->record(*copyCommands_);

    graphicsCommandBuilder_->transition(backBuffer(), PixelBufferState::present, PixelBufferState::renderTarget);
    graphicsCommandBuilder_->clearRenderTarget(backBuffer(), { 0, 0, 0, 1 });
}

GF_NAMESPACE_END