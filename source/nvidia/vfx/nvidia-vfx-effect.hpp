// Copyright (c) 2020 Michael Fabian Dirks <info@xaymar.com>
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

#pragma once
#include "nvidia-vfx.hpp"
#include "nvidia/cuda/nvidia-cuda-obs.hpp"
#include "nvidia/cuda/nvidia-cuda-stream.hpp"
#include "nvidia/cuda/nvidia-cuda.hpp"
#include "nvidia/cv/nvidia-cv-image.hpp"
#include "nvidia/cv/nvidia-cv-texture.hpp"
#include "nvidia/cv/nvidia-cv.hpp"
#include "nvidia/vfx/nvidia-vfx.hpp"

#include "warning-disable.hpp"
#include <memory>
#include <string>
#include <string_view>
#include "warning-enable.hpp"

namespace streamfx::nvidia::vfx {
	using namespace ::streamfx::nvidia;

	class effect {
		protected:
		std::shared_ptr<cuda::obs> _nvcuda;
		std::shared_ptr<cv::cv>    _nvcvi;
		std::shared_ptr<vfx>       _nvvfx;
		std::shared_ptr<void>      _fx;
		std::string                _model_path;

		public:
		~effect();
		effect(effect_t name);

		::streamfx::nvidia::vfx::handle_t get()
		{
			return _fx.get();
		}

		public /* Int32 */:
		inline cv::result set(parameter_t param, uint32_t const value)
		{
			return _nvvfx->NvVFX_SetU32(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, uint32_t& value)
		{
			return _nvvfx->NvVFX_GetU32(_fx.get(), param, &value);
		};

		inline cv::result set(parameter_t param, int32_t const value)
		{
			return _nvvfx->NvVFX_SetS32(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, int32_t& value)
		{
			return _nvvfx->NvVFX_GetS32(_fx.get(), param, &value);
		};

		public /* Int64 */:
		inline cv::result set(parameter_t param, uint64_t const value)
		{
			return _nvvfx->NvVFX_SetU64(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, uint64_t& value)
		{
			return _nvvfx->NvVFX_GetU64(_fx.get(), param, &value);
		};

		public /* Float32 */:
		inline cv::result set(parameter_t param, float const value)
		{
			return _nvvfx->NvVFX_SetF32(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, float& value)
		{
			return _nvvfx->NvVFX_GetF32(_fx.get(), param, &value);
		};

		public /* Float64 */:
		inline cv::result set(parameter_t param, double const value)
		{
			return _nvvfx->NvVFX_SetF64(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, double& value)
		{
			return _nvvfx->NvVFX_GetF64(_fx.get(), param, &value);
		};

		public /* String */:
		inline cv::result set(parameter_t param, const char* const value)
		{
			return _nvvfx->NvVFX_SetString(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, const char*& value)
		{
			return _nvvfx->NvVFX_GetString(_fx.get(), param, &value);
		};

		inline cv::result set(parameter_t param, std::string_view const& value)
		{
			return _nvvfx->NvVFX_SetString(_fx.get(), param, value.data());
		};
		cv::result get(parameter_t param, std::string_view& value);

		inline cv::result set(parameter_t param, std::string const& value)
		{
			return _nvvfx->NvVFX_SetString(_fx.get(), param, value.c_str());
		};
		cv::result get(parameter_t param, std::string& value);

		public /* CUDA Stream */:
		inline cv::result set(parameter_t param, cuda::stream_t const& value)
		{
			return _nvvfx->NvVFX_SetCudaStream(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, cuda::stream_t& value)
		{
			return _nvvfx->NvVFX_GetCudaStream(_fx.get(), param, &value);
		};

		inline cv::result set(parameter_t param, std::shared_ptr<cuda::stream> const& value)
		{
			return _nvvfx->NvVFX_SetCudaStream(_fx.get(), param, value->get());
		};
		//cv::result get_stream(parameter_t param, std::shared_ptr<cuda::stream>& value);

		public /* CV Image */:
		inline cv::result set(parameter_t param, cv::image_t* value)
		{
			return _nvvfx->NvVFX_SetImage(_fx.get(), param, value);
		};
		inline cv::result get(parameter_t param, cv::image_t* value)
		{
			return _nvvfx->NvVFX_GetImage(_fx.get(), param, value);
		};

		inline cv::result set(parameter_t param, std::shared_ptr<cv::image> const& value)
		{
			return _nvvfx->NvVFX_SetImage(_fx.get(), param, value->get_image());
		};
		inline cv::result get(parameter_t param, std::shared_ptr<cv::image>& value)
		{
			return _nvvfx->NvVFX_GetImage(_fx.get(), param, value->get_image());
		};

		public /* CV Texture */:
		inline cv::result set(parameter_t param, std::shared_ptr<cv::texture> const& value)
		{
			return _nvvfx->NvVFX_SetImage(_fx.get(), param, value->get_image());
		};
		//cv::result get(parameter_t param, std::shared_ptr<cv::texture>& value);

		public /* Objects */:
		inline cv::result set_object(parameter_t param, void* const value)
		{
			return _nvvfx->NvVFX_SetObject(_fx.get(), param, value);
		};
		inline cv::result get_object(parameter_t param, void*& value)
		{
			return _nvvfx->NvVFX_GetObject(_fx.get(), param, &value);
		};

		public /* Control */:
		inline cv::result load()
		{
			return _nvvfx->NvVFX_Load(_fx.get());
		};

		inline cv::result run(bool async = false)
		{
			return _nvvfx->NvVFX_Run(_fx.get(), async ? 1 : 0);
		};
	};
} // namespace streamfx::nvidia::vfx
