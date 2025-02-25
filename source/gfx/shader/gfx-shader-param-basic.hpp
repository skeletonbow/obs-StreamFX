// Modern effects for a modern Streamer
// Copyright (C) 2019 Michael Fabian Dirks
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#pragma once
#include "common.hpp"
#include "gfx-shader-param.hpp"
#include "obs/gs/gs-effect-parameter.hpp"

#include "warning-disable.hpp"
#include <vector>
#include "warning-enable.hpp"

namespace streamfx::gfx {
	namespace shader {
		enum class basic_field_type {
			Input,
			Slider,
			Enum,
		};

		basic_field_type get_field_type_from_string(std::string_view v);

		struct basic_data {
			union {
				int32_t  i32;
				uint32_t ui32;
				float_t  f32;
			};
		};

		struct basic_enum_data {
			std::string name;
			basic_data  data;
		};

		class basic_parameter : public parameter {
			// Descriptor
			basic_field_type         _field_type;
			std::string              _suffix;
			std::vector<std::string> _keys;
			std::vector<std::string> _names;

			protected:
			// Limits
			std::vector<basic_data> _min;
			std::vector<basic_data> _max;
			std::vector<basic_data> _step;
			std::vector<basic_data> _scale;

			// Enumeration Information
			std::list<basic_enum_data> _values;

			public:
			basic_parameter(streamfx::gfx::shader::shader* parent, streamfx::obs::gs::effect_parameter param,
							std::string prefix);
			virtual ~basic_parameter();

			virtual void load_parameter_data(streamfx::obs::gs::effect_parameter parameter, basic_data& data);

			public:
			inline basic_field_type field_type()
			{
				return _field_type;
			}

			inline std::string_view suffix()
			{
				return _suffix;
			}

			inline std::string_view key_at(std::size_t idx)
			{
				if (idx >= get_size())
					throw std::out_of_range("Index out of range.");
				return _keys[idx];
			}

			inline std::string_view name_at(std::size_t idx)
			{
				if (idx >= get_size())
					throw std::out_of_range("Index out of range.");
				return _names[idx];
			}
		};

		struct bool_parameter : public basic_parameter {
			// std::vector<bool> doesn't allow .data()
			std::vector<int32_t> _data;

			public:
			bool_parameter(streamfx::gfx::shader::shader* parent, streamfx::obs::gs::effect_parameter param,
						   std::string prefix);
			virtual ~bool_parameter();

			void defaults(obs_data_t* settings) override;

			void properties(obs_properties_t* props, obs_data_t* settings) override;

			void update(obs_data_t* settings) override;

			void assign() override;
		};

		struct float_parameter : public basic_parameter {
			std::vector<basic_data> _data;

			public:
			float_parameter(streamfx::gfx::shader::shader* parent, streamfx::obs::gs::effect_parameter param,
							std::string prefix);
			virtual ~float_parameter();

			void defaults(obs_data_t* settings) override;

			void properties(obs_properties_t* props, obs_data_t* settings) override;

			void update(obs_data_t* settings) override;

			void assign() override;
		};

		struct int_parameter : public basic_parameter {
			std::vector<basic_data> _data;

			public:
			int_parameter(streamfx::gfx::shader::shader* parent, streamfx::obs::gs::effect_parameter param,
						  std::string prefix);
			virtual ~int_parameter();

			void defaults(obs_data_t* settings) override;

			void properties(obs_properties_t* props, obs_data_t* settings) override;

			void update(obs_data_t* settings) override;

			void assign() override;
		};

	} // namespace shader
} // namespace streamfx::gfx
