// Copyright (c) 2017-2022 Michael Fabian Dirks <info@xaymar.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "gs-rendertarget.hpp"
#include "obs/gs/gs-helper.hpp"

#include "warning-disable.hpp"
#include <stdexcept>
#include "warning-enable.hpp"

streamfx::obs::gs::rendertarget::~rendertarget()
{
	auto gctx = streamfx::obs::gs::context();
	gs_texrender_destroy(_render_target);
}

streamfx::obs::gs::rendertarget::rendertarget(gs_color_format colorFormat, gs_zstencil_format zsFormat)
	: _color_format(colorFormat), _zstencil_format(zsFormat)
{
	_is_being_rendered = false;
	auto gctx          = streamfx::obs::gs::context();
	_render_target     = gs_texrender_create(colorFormat, zsFormat);
	if (!_render_target) {
		throw std::runtime_error("Failed to create render target.");
	}
}

streamfx::obs::gs::rendertarget_op streamfx::obs::gs::rendertarget::render(uint32_t width, uint32_t height)
{
	return {this, width, height};
}

streamfx::obs::gs::rendertarget_op streamfx::obs::gs::rendertarget::render(uint32_t width, uint32_t height,
																		   gs_color_space cs)
{
	return {this, width, height, cs};
}

gs_texture_t* streamfx::obs::gs::rendertarget::get_object()
{
	auto          gctx = streamfx::obs::gs::context();
	gs_texture_t* tex  = gs_texrender_get_texture(_render_target);
	return tex;
}

std::shared_ptr<streamfx::obs::gs::texture> streamfx::obs::gs::rendertarget::get_texture()
{
	return std::make_shared<streamfx::obs::gs::texture>(get_object(), false);
}

void streamfx::obs::gs::rendertarget::get_texture(streamfx::obs::gs::texture& tex)
{
	tex = streamfx::obs::gs::texture(get_object(), false);
}

void streamfx::obs::gs::rendertarget::get_texture(std::shared_ptr<streamfx::obs::gs::texture>& tex)
{
	tex = std::make_shared<streamfx::obs::gs::texture>(get_object(), false);
}

void streamfx::obs::gs::rendertarget::get_texture(std::unique_ptr<streamfx::obs::gs::texture>& tex)
{
	tex = std::make_unique<streamfx::obs::gs::texture>(get_object(), false);
}

gs_color_format streamfx::obs::gs::rendertarget::get_color_format()
{
	return _color_format;
}

gs_zstencil_format streamfx::obs::gs::rendertarget::get_zstencil_format()
{
	return _zstencil_format;
}

streamfx::obs::gs::rendertarget_op::rendertarget_op(streamfx::obs::gs::rendertarget* rt, uint32_t width,
													uint32_t height)
	: parent(rt)
{
	if (parent == nullptr)
		throw std::invalid_argument("rt");
	if (parent->_is_being_rendered)
		throw std::logic_error("Can't start rendering to the same render target twice.");

	auto gctx = streamfx::obs::gs::context();
	gs_texrender_reset(parent->_render_target);
	if (!gs_texrender_begin(parent->_render_target, width, height)) {
		throw std::runtime_error("Failed to begin rendering to render target.");
	}
	parent->_is_being_rendered = true;
}

streamfx::obs::gs::rendertarget_op::rendertarget_op(streamfx::obs::gs::rendertarget* rt, uint32_t width,
													uint32_t height, gs_color_space cs)
	: parent(rt)
{
	if (parent == nullptr)
		throw std::invalid_argument("rt");
	if (parent->_is_being_rendered)
		throw std::logic_error("Can't start rendering to the same render target twice.");

	auto gctx = streamfx::obs::gs::context();
	gs_texrender_reset(parent->_render_target);
	if (!gs_texrender_begin_with_color_space(parent->_render_target, width, height, cs)) {
		throw std::runtime_error("Failed to begin rendering to render target.");
	}
	parent->_is_being_rendered = true;
}

streamfx::obs::gs::rendertarget_op::rendertarget_op(streamfx::obs::gs::rendertarget_op&& r) noexcept
{
	this->parent = r.parent;
	r.parent     = nullptr;
}

streamfx::obs::gs::rendertarget_op::~rendertarget_op()
{
	if (parent == nullptr)
		return;

	auto gctx = streamfx::obs::gs::context();
	gs_texrender_end(parent->_render_target);
	parent->_is_being_rendered = false;
}
