#include "debugdraw.h"
#include "../scene/scene.h"
#include "../scene/material.h"
#include "../render/pixelbuffer.h"
#include "../render/vertexdata.h"
#include "../render/gpucommand.h"
#include "../render/drawcall.h"
#include "../engine/pixelformat.h"
#include "../engine/logging.h"
#include "foundation/matrix44.h"

GF_NAMESPACE_BEGIN

namespace
{
    const auto DEBUG_MATERIAL = EnginePath("engine/debugdraw.material");
}

void DebugDraw::startup()
{
    material_ = resourceManager.template obtain<Material>(DEBUG_MATERIAL);
    material_->load();

    if (!material_->ready())
    {
        GF_LOG_WARN("Failed to enable DebugDrawManager. Resource loading failed.");
    }
    else
    {
        GF_LOG_INFO("DebugDrawManager initialized.");
    }
}

void DebugDraw::shutdown()
{
    material_->unload();
    positions_.clear();
    colors_.clear();
    vertex_.reset();
    GF_LOG_INFO("DebugDrawManager shutdown.");
}

void DebugDraw::drawDebugs(const RenderCamera& camera)
{
    if (positions_.empty() || !material_.useable())
    {
        return;
    }

    vertex_ = std::make_shared<VertexData>();
    vertex_->setTopology(PrimitiveTopology::lines);
    vertex_->setVertices(Semantics::POSITION, 0, positions_.data(),  positions_.size() * sizeof(Vector3), PixelFormat::RGB32_float);
    vertex_->setVertices(Semantics::COLOR, 0, colors_.data(), colors_.size() * sizeof(Color), PixelFormat::RGBA32_float);

    auto& copy = sceneAppContext.copyCommandBuilder();
    auto& graphics = sceneAppContext.graphicsCommandBuilder();
    auto& backBuffer = sceneAppContext.backBuffer();

    copy.uploadVertices(*vertex_);
    graphics.drawableState(*vertex_);

    material_->setFloat4x4("ViewProj", (camera.view * camera.proj).transpose());

    auto drawCall = material_->drawCallSource();
    drawCall.setRenderTarget(backBuffer);
    drawCall.setVertex(*vertex_, 0, positions_.size(), 0, 1);
    drawCall.setViewport(camera.viewport);

    graphics.triggerDrawCall(drawCall);

    positions_.clear();
    colors_.clear();
}

void DebugDraw::line(const Vector3& from, const Vector3& to, const Color& color)
{
    positions_.emplace_back(from);
    positions_.emplace_back(to);
    colors_.emplace_back(color);
    colors_.emplace_back(color);
}

DebugDraw debugDraw;

GF_NAMESPACE_END