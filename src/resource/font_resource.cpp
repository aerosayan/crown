/*
 * Copyright (c) 2012-2020 Daniele Bartolini and individual contributors.
 * License: https://github.com/dbartolini/crown/blob/master/LICENSE
 */

#include "config.h"
#include "core/containers/array.inl"
#include "core/filesystem/file.h"
#include "core/filesystem/filesystem.h"
#include "core/json/json_object.inl"
#include "core/json/sjson.h"
#include "core/memory/allocator.h"
#include "core/memory/temp_allocator.inl"
#include "core/strings/string.inl"
#include "resource/compile_options.h"
#include "resource/font_resource.h"
#include "resource/types.h"
#include <algorithm>

namespace crown
{
namespace font_resource
{
	const GlyphData* glyph(const FontResource* fr, CodePoint cp)
	{
		CE_ASSERT(cp < fr->num_glyphs, "Index out of bounds");

		const CodePoint* pts  = (CodePoint*)&fr[1];
		const GlyphData* data = (GlyphData*)(pts + fr->num_glyphs);

		// FIXME: Can do binary search
		for (u32 i = 0; i < fr->num_glyphs; ++i)
		{
			if (pts[i] == cp)
				return &data[i];
		}

		CE_FATAL("Glyph not found");
		return NULL;
	}

} // namespace font_resource

#if CROWN_CAN_COMPILE
namespace font_resource_internal
{
	struct GlyphInfo
	{
		CodePoint cp;
		GlyphData gd;

		bool operator<(const GlyphInfo& a) const
		{
			return cp < a.cp;
		}
	};

	s32 compile(CompileOptions& opts)
	{
		Buffer buf = opts.read();

		TempAllocator4096 ta;
		JsonObject obj(ta);
		JsonArray glyphs_json(ta);

		sjson::parse(obj, buf);
		sjson::parse_array(glyphs_json, obj["glyphs"]);

		const u32 texture_size = sjson::parse_int(obj["size"]);
		const u32 font_size    = sjson::parse_int(obj["font_size"]);
		const u32 num_glyphs   = array::size(glyphs_json);

		DATA_COMPILER_ASSERT(font_size > 0
			, opts
			, "Font size must be > 0"
			);

		Array<GlyphInfo> glyphs(default_allocator());
		array::resize(glyphs, num_glyphs);
		for (u32 i = 0; i < num_glyphs; ++i)
		{
			TempAllocator512 ta;
			JsonObject obj(ta);
			sjson::parse(obj, glyphs_json[i]);

			glyphs[i].cp           = sjson::parse_int  (obj["id"]);
			glyphs[i].gd.x         = sjson::parse_float(obj["x"]);
			glyphs[i].gd.y         = sjson::parse_float(obj["y"]);
			glyphs[i].gd.width     = sjson::parse_float(obj["width"]);
			glyphs[i].gd.height    = sjson::parse_float(obj["height"]);
			glyphs[i].gd.x_offset  = sjson::parse_float(obj["x_offset"]);
			glyphs[i].gd.y_offset  = sjson::parse_float(obj["y_offset"]);
			glyphs[i].gd.x_advance = sjson::parse_float(obj["x_advance"]);
		}
		std::sort(array::begin(glyphs), array::end(glyphs));

		opts.write(RESOURCE_HEADER(RESOURCE_VERSION_FONT));
		opts.write(texture_size);
		opts.write(font_size);
		opts.write(num_glyphs);

		for (u32 i = 0; i < num_glyphs; ++i)
			opts.write(glyphs[i].cp);

		for (u32 i = 0; i < num_glyphs; ++i)
		{
			opts.write(glyphs[i].gd.x);
			opts.write(glyphs[i].gd.y);
			opts.write(glyphs[i].gd.width);
			opts.write(glyphs[i].gd.height);
			opts.write(glyphs[i].gd.x_offset);
			opts.write(glyphs[i].gd.y_offset);
			opts.write(glyphs[i].gd.x_advance);
		}

		return 0;
	}

} // namespace font_resource_internal
#endif // CROWN_CAN_COMPILE

} // namespace crown
