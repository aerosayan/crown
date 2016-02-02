/*
 * Copyright (c) 2012-2016 Daniele Bartolini and individual contributors.
 * License: https://github.com/taylor001/crown/blob/master/LICENSE
 */

#include "compile_options.h"
#include "map.h"
#include "reader_writer.h"
#include "resource_manager.h"
#include "sjson.h"
#include "texture_resource.h"
#include "os.h"

#if CROWN_DEVELOPMENT
	#define TEXTUREC_NAME "texturec-development-"
#elif CROWN_DEBUG
	#define TEXTUREC_NAME "texturec-debug-"
#else
	#define TEXTUREC_NAME "texturec-release-"
#endif  // CROWN_DEBUG
#if CROWN_ARCH_32BIT
	#define TEXTUREC_BITS "32"
#elif CROWN_ARCH_64BIT
	#define TEXTUREC_BITS "64"
#endif // CROWN_ARCH_32BIT
#if CROWN_PLATFORM_LINUX
	#define TEXTUREC_PATH "./" TEXTUREC_NAME "" TEXTUREC_BITS
#elif CROWN_PLATFORM_WINDOWS
	#define TEXTUREC_PATH TEXTUREC_NAME "" TEXTUREC_BITS ".exe"
#else
	#define TEXTUREC_PATH ""
#endif // CROWN_PLATFORM_LINUX

namespace crown
{
namespace texture_resource
{
	void compile(const char* path, CompileOptions& opts)
	{
		Buffer buf = opts.read(path);

		TempAllocator4096 ta;
		JsonObject object(ta);
		sjson::parse(buf, object);

		DynamicString name(ta);
		sjson::parse_string(object["source"], name);

		const bool generate_mips = sjson::parse_bool(object["generate_mips"]);
		const bool is_normalmap  = sjson::parse_bool(object["is_normalmap"]);

		DynamicString texsrc(ta);
		DynamicString texout(ta);
		opts.get_absolute_path(name.c_str(), texsrc);
		opts.get_absolute_path("texture.ktx", texout);

		using namespace string_stream;
		StringStream args(ta);
		args << " -f " << texsrc.c_str();
		args << " -o " << texout.c_str();
		args << (generate_mips ? " -m " : "");
		args << (is_normalmap  ? " -n " : "");

		StringStream output(ta);
		int exitcode = os::execute_process(TEXTUREC_PATH, c_str(args), output);
		RESOURCE_COMPILER_ASSERT(exitcode == 0
			, opts
			, "Failed to compile texture:\n%s"
			, c_str(output)
			);

		Buffer blob = opts.read(texout.c_str());
		opts.delete_file(texout.c_str());

		// Write DDS
		opts.write(TEXTURE_VERSION);
		opts.write(array::size(blob));
		opts.write(blob);
	}

	void* load(File& file, Allocator& a)
	{
		const u32 file_size = file.size();
		file.skip(sizeof(TextureHeader));
		const bgfx::Memory* mem = bgfx::alloc(file_size);
		file.read(mem->data, file_size - sizeof(TextureHeader));

		TextureResource* teximg = (TextureResource*) a.allocate(sizeof(TextureResource));
		teximg->mem = mem;
		teximg->handle.idx = bgfx::invalidHandle;

		return teximg;
	}

	void online(StringId64 id, ResourceManager& rm)
	{
		TextureResource* teximg = (TextureResource*) rm.get(TEXTURE_TYPE, id);
		teximg->handle = bgfx::createTexture(teximg->mem);
	}

	void offline(StringId64 id, ResourceManager& rm)
	{
		TextureResource* teximg = (TextureResource*) rm.get(TEXTURE_TYPE, id);
		bgfx::destroyTexture(teximg->handle);
	}

	void unload(Allocator& a, void* resource)
	{
		a.deallocate(resource);
	}

} // namespace texture_resource
} // namespace crown