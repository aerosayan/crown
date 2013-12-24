/*
Copyright (c) 2013 Daniele Bartolini, Michele Rossi
Copyright (c) 2012 Daniele Bartolini, Simone Boscaratto

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstring>
#include <inttypes.h>

#include "Types.h"
#include "Allocator.h"
#include "File.h"
#include "OS.h"
#include "StringUtils.h"
#include "List.h"
#include "Bundle.h"
#include "Device.h"
#include "ResourceManager.h"
#include "RendererTypes.h"
#include "Renderer.h"
#include "TextureResource.h"

#define MAX_SPRITE_ANIM_FRAMES 60
#define SPRITE_FRAME_SIZE 4 * 4 * 4

namespace crown
{

const uint32_t SPRITE_VERSION = 1;

//-----------------------------------------------------------------------------
struct SpriteHeader
{
	VertexBufferId 	vb;
	IndexBufferId 	ib;
	uint32_t 		num_frames;
	uint32_t		frame_names_offset;
	uint32_t		frame_vertices_offset;
};

//-----------------------------------------------------------------------------
struct SpriteResource
{
	//-----------------------------------------------------------------------------
	static void* load(Allocator& allocator, Bundle& bundle, ResourceId id)
	{
		File* file = bundle.open(id);
		const size_t file_size = file->size();

		void* res = allocator.allocate(file_size);
		file->read(res, file_size);

		bundle.close(file);

		return res;
	}

	//-----------------------------------------------------------------------------
	static void online(void* resource)
	{
		SpriteResource* sr = (SpriteResource*)resource;
		SpriteHeader* h = (SpriteHeader*) sr;

		static uint16_t t_indices[] = {0, 1, 2, 0, 2, 3};

		h->vb = device()->renderer()->create_vertex_buffer(4, VertexFormat::P2_T2, sr->frame(0));
		h->ib = device()->renderer()->create_index_buffer(6, t_indices);
	}

	//-----------------------------------------------------------------------------
	static void unload(Allocator& allocator, void* resource)
	{
		CE_ASSERT_NOT_NULL(resource);
		allocator.deallocate(resource);
	}

	//-----------------------------------------------------------------------------
	static void offline(void* resource)
	{
		CE_ASSERT_NOT_NULL(resource);

		SpriteResource* sr = (SpriteResource*) resource;
		SpriteHeader* h = (SpriteHeader*) sr;

		device()->renderer()->destroy_vertex_buffer(h->vb);
		device()->renderer()->destroy_index_buffer(h->ib);
	}

	//-----------------------------------------------------------------------------
	uint32_t num_frames() const
	{
		return ((SpriteHeader*) this)->num_frames;
	}

	//-----------------------------------------------------------------------------
	const float* animation() const
	{
		SpriteHeader* h = (SpriteHeader*) this;
		return (float*) (((char*) this) + h->frame_vertices_offset);
	}

	//-----------------------------------------------------------------------------
	const float* frame(uint32_t index) const
	{
		SpriteHeader* h = (SpriteHeader*) this;
		return (float*) (((char*) this) + h->frame_vertices_offset + index * sizeof(float) * 4 * 4);
	}

	//-----------------------------------------------------------------------------
	VertexBufferId vertex_buffer() const
	{
		return ((SpriteHeader*) this)->vb;
	}

	//-----------------------------------------------------------------------------
	IndexBufferId index_buffer() const
	{
		return ((SpriteHeader*) this)->ib;
	}

private:

	// Disable construction
	SpriteResource();
};

} // namespace crown