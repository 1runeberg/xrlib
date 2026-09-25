/* 
 * Copyright 2024-26 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <limits>
#include <span>
#include <chrono>
#include <atomic>
#include <future>
#include <thread>

#include <xrvk/gltf.hpp>
#include <xrvk/texture.hpp>

#include <filesystem>

namespace fs = std::filesystem;
namespace xrlib
{
	static double ElapsedMilliseconds( std::chrono::steady_clock::time_point start )
	{
		return std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - start ).count();
	}

	CGltf::CGltf( CSession *pSession )
		: CGltf( pSession, SGltfLoadOptions {} )
	{
	}

	CGltf::CGltf( CSession *pSession, SGltfLoadOptions options )
		: m_pSession( pSession )
		, m_options( options )
	{
		assert( pSession );
	}

	CGltf::~CGltf()
	{
	}

	static bool DecodeImage( SGltfImage &outImage, std::span< const std::byte > bytes )
	{
		if ( bytes.empty() || bytes.size() > static_cast< size_t >( ( std::numeric_limits< int >::max )() ) )
			return false;

		const auto *pBytes = reinterpret_cast< const stbi_uc * >( bytes.data() );
		const int nSize = static_cast< int >( bytes.size() );
		int nChannels = 0;
		stbi_uc *pPixels = nullptr;
		if ( stbi_is_16_bit_from_memory( pBytes, nSize ) )
		{
			pPixels = reinterpret_cast< stbi_uc * >( stbi_load_16_from_memory( pBytes, nSize, &outImage.width, &outImage.height, &nChannels, 4 ) );
			outImage.bits = 16;
		}
		if ( !pPixels )
		{
			pPixels = stbi_load_from_memory( pBytes, nSize, &outImage.width, &outImage.height, &nChannels, 4 );
			outImage.bits = 8;
		}
		if ( !pPixels )
		{
			LogError( XRLIB_NAME, "Failed to decode glTF image: %s", outImage.name.c_str() );
			return false;
		}

		std::unique_ptr< stbi_uc, decltype( &stbi_image_free ) > pixels( pPixels, stbi_image_free );
		const size_t unSize = static_cast< size_t >( outImage.width ) * outImage.height * outImage.component * ( outImage.bits / 8 );
		outImage.image.assign( pPixels, pPixels + unSize );
		return true;
	}

	// Prepared textures use a restricted KTX2 profile: uncompressed 2D RGBA8/16
	// Reject other profiles rather than interpreting compressed bytes as pixels
	static bool DecodeKtx2( SGltfImage &outImage, std::span< const std::byte > bytes )
	{
		constexpr uint8_t identifier[] = { 0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a };
		if ( bytes.size() < 104 || std::memcmp( bytes.data(), identifier, sizeof( identifier ) ) )
			return false;
		auto Read = [ & ]( size_t offset, size_t count = 4 )
		{
			uint64_t value = 0;
			for ( size_t i = 0; i < count; ++i )
				value |= uint64_t( std::to_integer< uint8_t >( bytes[ offset + i ] ) ) << ( i * 8 );
			return value;
		};
		auto InBounds = [ & ]( uint64_t offset, uint64_t size ) { return offset <= bytes.size() && size <= bytes.size() - offset; };
		const auto format = Read( 12 ), width = Read( 20 ), height = Read( 24 ), levels = Read( 40 );
		const bool wide = format == VK_FORMAT_R16G16B16A16_UNORM;
		if ( ( format != VK_FORMAT_R8G8B8A8_UNORM && format != VK_FORMAT_R8G8B8A8_SRGB && !wide ) || Read( 16 ) != ( wide ? 2 : 1 ) || !width || !height || width > INT_MAX || height > INT_MAX || Read( 28 ) || Read( 32 ) || Read( 36 ) != 1 ||
			 Read( 44 ) || !levels || levels > 32 || !InBounds( 80, levels * 24 ) || Read( 64, 8 ) || Read( 72, 8 ) )
			return false;
		uint64_t dimension = ( std::max )( width, height ), requiredLevels = 0;
		while ( dimension )
		{
			++requiredLevels;
			dimension >>= 1;
		}
		if ( levels != requiredLevels )
			return false;
		const auto dfd = Read( 48 ), dfdSize = Read( 52 ), kvd = Read( 56 ), kvdSize = Read( 60 );
		if ( dfd < 80 + levels * 24 || dfdSize < 28 || !InBounds( dfd, dfdSize ) || Read( dfd ) != dfdSize || ( kvdSize && !InBounds( kvd, kvdSize ) ) )
			return false;

		// The build tool emits top-left images; reject a cache with a different orientation
		for ( uint64_t entry = kvd; entry < kvd + kvdSize; )
		{
			if ( kvd + kvdSize - entry < 4 )
				return false;
			const auto length = Read( entry );
			entry += 4;
			if ( length > kvd + kvdSize - entry )
				return false;
			const auto *text = reinterpret_cast< const char * >( bytes.data() + entry );
			const auto *end = static_cast< const char * >( std::memchr( text, 0, length ) );
			if ( !end )
				return false;
			if ( std::string_view( text, end - text ) == "KTXorientation" && ( length < size_t( end - text ) + 3 || end[ 1 ] != 'r' || end[ 2 ] != 'd' ) )
				return false;
			entry += ( length + 3 ) & ~uint64_t( 3 );
		}
		uint64_t w = width, h = height;
		std::vector< std::pair< uint64_t, uint64_t > > ranges;
		for ( size_t level = 0; level < levels; ++level )
		{
			const auto offset = Read( 80 + level * 24, 8 ), size = Read( 88 + level * 24, 8 );
			if ( w > UINT64_MAX / h / ( wide ? 8 : 4 ) )
				return false;
			const uint64_t expected = w * h * ( wide ? 8 : 4 );
			if ( size != expected || Read( 96 + level * 24, 8 ) != expected || !InBounds( offset, size ) || offset < dfd + dfdSize || ( kvdSize && offset < kvd + kvdSize ) || offset % ( wide ? 8 : 4 ) )
				return false;
			for ( const auto &[ start, count ] : ranges )
				if ( offset < start + count && start < offset + size )
					return false;
			ranges.emplace_back( offset, size );
			outImage.mips.push_back( { outImage.image.size(), size } );
			const auto *pixels = reinterpret_cast< const uint8_t * >( bytes.data() + offset );
			outImage.image.insert( outImage.image.end(), pixels, pixels + size );
			if ( level + 1 < levels && w == 1 && h == 1 )
				return false;
			w = ( std::max )( uint64_t( 1 ), w / 2 );
			h = ( std::max )( uint64_t( 1 ), h / 2 );
		}
		if ( w != 1 || h != 1 )
			return false;
		outImage.width = static_cast< int >( width );
		outImage.height = static_cast< int >( height );
		outImage.bits = wide ? 16 : 8;
		outImage.component = 4;
		return true;
	}

	static bool LoadImages( SGltfModel &outModel, const fs::path &sDirectory, uint32_t unMaxWorkers, const std::string &sTextureDirectory )
	{
		outModel.images.resize( outModel.asset.images.size() );
		auto LoadImage = [ & ]( size_t i )
		{
			const auto &image = outModel.asset.images[ i ];
			auto &outImage = outModel.images[ i ];
			outImage.name = image.name;
			if ( !sTextureDirectory.empty() )
			{
				const auto file = fs::path( sTextureDirectory ) / ( "image-" + std::to_string( i ) + ".ktx2" );
#ifdef XR_USE_PLATFORM_ANDROID
				auto data = fastgltf::AndroidGltfDataBuffer::FromAsset( file );
#else
				auto data = fastgltf::GltfDataBuffer::FromPath( file );
#endif
				if ( data.error() != fastgltf::Error::None || !DecodeKtx2( outImage, static_cast< fastgltf::span< std::byte > >( data.get() ) ) )
				{
					LogError( XRLIB_NAME, "Invalid or missing prepared KTX2 image: %s", file.string().c_str() );
					return false;
				}
				return true;
			}
			bool bLoaded = std::visit(
				fastgltf::visitor {
					[ & ]( const fastgltf::sources::Array &source ) { return DecodeImage( outImage, { source.bytes.data(), source.bytes.size() } ); },
					[ & ]( const fastgltf::sources::BufferView &source )
					{
						const auto bytes = fastgltf::DefaultBufferDataAdapter {}( outModel.asset, source.bufferViewIndex );
						return DecodeImage( outImage, { bytes.data(), bytes.size() } );
					},
					[ & ]( const fastgltf::sources::URI &source )
					{
						outImage.uri = source.uri.string();
						if ( !source.uri.isLocalPath() )
							return false;
						const fs::path file = sDirectory / source.uri.fspath();
#ifdef XR_USE_PLATFORM_ANDROID
						auto data = fastgltf::AndroidGltfDataBuffer::FromAsset( file );
#else
						auto data = fastgltf::GltfDataBuffer::FromPath( file );
#endif
						if ( data.error() != fastgltf::Error::None || source.fileByteOffset > data->totalSize() )
							return false;
						auto bytes = static_cast< fastgltf::span< std::byte > >( data.get() );
						return DecodeImage( outImage, { bytes.data() + source.fileByteOffset, bytes.size() - source.fileByteOffset } );
					},
					[]( const auto & ) { return false; } },
				image.data );
			if ( !bLoaded )
			{
				LogError( XRLIB_NAME, "Failed to load glTF image: %s", outImage.name.c_str() );
				return false;
			}
			return true;
		};

		const size_t unWorkers = ( std::min )( outModel.images.size(), static_cast< size_t >( ( std::min )( ( std::max )( 1u, unMaxWorkers ), ( std::max )( 1u, std::thread::hardware_concurrency() ) ) ) );
		if ( unWorkers <= 1 )
		{
			for ( size_t i = 0; i < outModel.images.size(); ++i )
				if ( !LoadImage( i ) )
					return false;
			return true;
		}

		// Each decoder owns its output slot. Parsed buffers remain read-only until all workers finish
		std::atomic< size_t > nextImage { 0 };
		std::vector< std::future< bool > > workers;
		workers.reserve( unWorkers );
		for ( size_t i = 0; i < unWorkers; ++i )
			workers.push_back(
				std::async(
					std::launch::async,
					[ & ]
					{
						bool loaded = true;
						for ( size_t index = nextImage.fetch_add( 1 ); index < outModel.images.size(); index = nextImage.fetch_add( 1 ) )
							loaded = LoadImage( index ) && loaded;
						return loaded;
					} ) );
		bool loaded = true;
		for ( auto &worker : workers )
			loaded = worker.get() && loaded;
		return loaded;
	}

	bool CGltf::LoadAndParse( CRenderModel *outRenderModel, VkCommandPool commandPool, const std::string &sFilename, XrVector3f scale )
	{
		SGltfModel model;
		if ( !LoadFromDisk( outRenderModel, &model, sFilename, scale ) )
			return false;
		ParseModel( outRenderModel, &model, commandPool );
		return true;
	}

	bool CGltf::LoadFromDisk( CRenderModel *outRenderModel, SGltfModel *outModel, const std::string &sFilename, XrVector3f scale )
	{
		const fs::path file( sFilename );
		if ( !outRenderModel || !outModel || file.empty() )
			return false;
		if ( file.extension() != ".glb" && file.extension() != ".gltf" )
		{
			LogError( XRLIB_NAME, "Error: Invalid file format: %s", sFilename.c_str() );
			return false;
		}

		SGltfLoadTimings timings;
		auto started = std::chrono::steady_clock::now();
#ifdef XR_USE_PLATFORM_ANDROID
		auto data = fastgltf::AndroidGltfDataBuffer::FromAsset( file );
#else
		auto data = fastgltf::GltfDataBuffer::FromPath( file );
#endif
		if ( data.error() != fastgltf::Error::None )
		{
			LogError( XRLIB_NAME, "Failed to read glTF file: %s (%s)", sFilename.c_str(), fastgltf::getErrorName( data.error() ).data() );
			return false;
		}

		timings.diskReadMs = ElapsedMilliseconds( started );
		started = std::chrono::steady_clock::now();

		// Keep each parser local so disk loading can run on worker threads
		fastgltf::Parser parser;
		auto asset = parser.loadGltf( data.get(), file.parent_path(), fastgltf::Options::LoadExternalBuffers );
		if ( asset.error() != fastgltf::Error::None )
		{
			LogError( XRLIB_NAME, "Failed to parse glTF file: %s (%s)", sFilename.c_str(), fastgltf::getErrorName( asset.error() ).data() );
			return false;
		}

		SGltfModel model;
		model.asset = std::move( asset.get() );
		if ( model.asset.scenes.empty() || fastgltf::validate( model.asset ) != fastgltf::Error::None )
		{
			LogError( XRLIB_NAME, "Invalid glTF scene: %s", sFilename.c_str() );
			return false;
		}
		timings.parseMs = ElapsedMilliseconds( started );
		started = std::chrono::steady_clock::now();
		if ( !LoadImages( model, file.parent_path(), m_options.imageDecodeWorkers, m_options.textureDirectory ) )
			return false;
		timings.imageDecodeMs = ElapsedMilliseconds( started );
		model.timings = timings;
		*outModel = std::move( model );

		// Set scale
		for ( size_t i = 0; i < outRenderModel->GetInstanceCount(); ++i )
			outRenderModel->instances[ i ].scale = scale;
		return true;
	}

	void CGltf::ParseModel( CRenderModel *outRenderModel, SGltfModel *pModel, VkCommandPool commandPool )
	{
		auto started = std::chrono::steady_clock::now();
		ParseTextures( outRenderModel, commandPool, *pModel );
		pModel->timings.textureUploadMs = ElapsedMilliseconds( started );
		started = std::chrono::steady_clock::now();
		ParseMaterials( outRenderModel, pModel->asset );
		ParseSkins( outRenderModel, pModel->asset );

		// Process all nodes in the scene
		for ( size_t unNode : pModel->asset.scenes[ 0 ].nodeIndices )
		{
			ProcessNode( pModel->asset, pModel->asset.nodes[ unNode ], outRenderModel->vertices, outRenderModel->indices, outRenderModel->materialSections );
		}
		pModel->timings.meshConversionMs = ElapsedMilliseconds( started );
	}

	void CGltf::ProcessNode( 
		const fastgltf::Asset &model,
		const fastgltf::Node &node,
		std::vector< SMeshVertex > &vertices, 
		std::vector< uint32_t > &indices, 
		std::vector< SMeshSection > &materialSections )
	{
		// Process mesh if present
		if ( node.meshIndex.has_value() )
		{
			ProcessMesh( model, model.meshes[ *node.meshIndex ], vertices, indices, materialSections );
		}

		// Process child nodes
		for ( size_t i = 0; i < node.children.size(); i++ )
		{
			ProcessNode( model, model.nodes[ node.children[ i ] ], vertices, indices, materialSections );
		}
	}

	void CGltf::ProcessMesh( 
		const fastgltf::Asset &model,
		const fastgltf::Mesh &mesh,
		std::vector< SMeshVertex > &vertices, 
		std::vector< uint32_t > &indices, 
		std::vector< SMeshSection > &materialSections )
	{
		// Process each primitive in the mesh
		for ( const auto &primitive : mesh.primitives )
		{
			uint32_t vertexBase = vertices.size();

			// Get accessor for vertex positions (required)
			const auto position = primitive.findAttribute( "POSITION" );
			if ( position == primitive.attributes.end() )
				throw std::runtime_error( "Missing glTF vertex positions" );
			const fastgltf::Accessor &posAccessor = model.accessors[ position->accessorIndex ];
			vertices.resize( vertexBase + posAccessor.count );
			for ( size_t i = vertexBase; i < vertices.size(); ++i )
			{
				vertices[ i ] = {};
				vertices[ i ].color0 = { 1.0f, 1.0f, 1.0f };

			}

			// Read attributes through fastgltf's accessor tools, including strides and normalization
			auto ReadAttribute = [&]< typename T >( const char *sName, auto pfnAssign ) {
				const auto attribute = primitive.findAttribute( sName );
				if ( attribute == primitive.attributes.end() )
					return;
				const auto &accessor = model.accessors[ attribute->accessorIndex ];
				if ( accessor.count != posAccessor.count )
					throw std::runtime_error( "Mismatched glTF vertex attribute count" );
				fastgltf::iterateAccessorWithIndex< T >( model, accessor, [&]( const T &value, size_t i ) {
					pfnAssign( vertices[ vertexBase + i ], value );
				} );
			};
			ReadAttribute.operator()< fastgltf::math::fvec3 >( "POSITION", []( SMeshVertex &vertex, const auto &value ) {
				vertex.position = { value[ 0 ], value[ 1 ], value[ 2 ] };
			} );
			ReadAttribute.operator()< fastgltf::math::fvec3 >( "NORMAL", []( SMeshVertex &vertex, const auto &value ) {
				vertex.normal = { value[ 0 ], value[ 1 ], value[ 2 ] };
			} );
			ReadAttribute.operator()< fastgltf::math::fvec2 >( "TEXCOORD_0", []( SMeshVertex &vertex, const auto &value ) {
				vertex.uv0 = { value[ 0 ], value[ 1 ] };
			} );
			ReadAttribute.operator()< fastgltf::math::fvec2 >( "TEXCOORD_1", []( SMeshVertex &vertex, const auto &value ) {
				vertex.uv1 = { value[ 0 ], value[ 1 ] };
			} );
			ReadAttribute.operator()< fastgltf::math::fvec4 >( "TANGENT", []( SMeshVertex &vertex, const auto &value ) {
				vertex.tangent = { value[ 0 ], value[ 1 ], value[ 2 ], value[ 3 ] };
			} );
			const auto color = primitive.findAttribute( "COLOR_0" );
			if ( color != primitive.attributes.end() && model.accessors[ color->accessorIndex ].type == fastgltf::AccessorType::Vec4 )
			{
				ReadAttribute.operator()< fastgltf::math::fvec4 >( "COLOR_0", []( SMeshVertex &vertex, const auto &value ) {
					vertex.color0 = { value[ 0 ], value[ 1 ], value[ 2 ] };
				} );
			}
			else
			{
				ReadAttribute.operator()< fastgltf::math::fvec3 >( "COLOR_0", []( SMeshVertex &vertex, const auto &value ) {
					vertex.color0 = { value[ 0 ], value[ 1 ], value[ 2 ] };
				} );
			}
			ReadAttribute.operator()< fastgltf::math::uvec4 >( "JOINTS_0", []( SMeshVertex &vertex, const auto &value ) {
				for ( int j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
					vertex.joints[ j ] = value[ j ];
			} );
			ReadAttribute.operator()< fastgltf::math::fvec4 >( "WEIGHTS_0", []( SMeshVertex &vertex, const auto &value ) {
				float fSum = 0.0f;
				for ( int j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
				{
					vertex.weights[ j ] = value[ j ];
					fSum += value[ j ];
				}
				if ( fSum > 1.0f )
				{
					for ( int j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
						vertex.weights[ j ] /= fSum;
				}
			} );
			if ( primitive.findAttribute( "WEIGHTS_0" ) == primitive.attributes.end() && primitive.findAttribute( "JOINTS_0" ) != primitive.attributes.end() )
			{
				for ( size_t i = vertexBase; i < vertices.size(); ++i )
					vertices[ i ].weights[ 0 ] = 1.0f;
			}

			const uint32_t firstIndex = static_cast< uint32_t >( indices.size() );
			if ( primitive.indicesAccessor )
			{
				fastgltf::iterateAccessor< uint32_t >( model, model.accessors[ *primitive.indicesAccessor ], [&]( uint32_t index ) {
					if ( index >= posAccessor.count )
						throw std::runtime_error( "glTF index exceeds vertex count" );
					indices.push_back( index + vertexBase );
				} );
			}
			else
				for ( size_t i = 0; i < posAccessor.count; ++i )
					indices.push_back( vertexBase + static_cast< uint32_t >( i ) );
			const uint32_t count = static_cast< uint32_t >( indices.size() ) - firstIndex;
			if ( count )
				materialSections.push_back( { firstIndex, count, static_cast< uint32_t >( primitive.materialIndex.value_or( model.materials.size() ) ) } );
		}
	}

	void CGltf::ParseTextures( CRenderModel *outRenderModel, VkCommandPool commandPool, const SGltfModel &model )
	{
		if ( model.asset.textures.size() < 1 )
			return;

		const size_t firstTexture = outRenderModel->textures.size();
		outRenderModel->textures.reserve( firstTexture + model.asset.textures.size() );
		for ( const auto &gltfTexture : model.asset.textures )
		{
			outRenderModel->textures.emplace_back();
			ParseTexture( &outRenderModel->textures.back(), commandPool, model, gltfTexture );
		}
		if ( m_options.batchTextureUploads )
		{
			std::vector< vkutils::SImageUpload > uploads;
			for ( size_t i = firstTexture; i < outRenderModel->textures.size(); ++i )
			{
				const auto &texture = outRenderModel->textures[ i ];
				if ( texture.image && !texture.data.empty() )
					uploads.push_back( { texture.image, texture.data, static_cast< uint32_t >( texture.width ), static_cast< uint32_t >( texture.height ), texture.format, texture.mips } );
			}
			VK_CHECK_RESULT( vkutils::UploadTextureDataToImages( m_pSession->GetVulkan()->GetVkLogicalDevice(), m_pSession->GetVulkan()->GetVkPhysicalDevice(), commandPool, m_pSession->GetVulkan()->GetVkQueue_Graphics(), uploads ) );
		}
	}

	void CGltf::ParseTexture( STexture *outTexture, VkCommandPool commandPool, const SGltfModel &model, const fastgltf::Texture &gltfTexture )
	{
		// Get the image data
		if ( !gltfTexture.imageIndex || *gltfTexture.imageIndex >= model.images.size() )
		{
			LogError( "CGltf::ParseTexture", "Invalid texture source index" );
			return;
		}

		const SGltfImage &image = model.images[ *gltfTexture.imageIndex ];

		// Basic texture properties
		outTexture->name = gltfTexture.name.empty() ? image.name : std::string( gltfTexture.name );
		outTexture->uri = image.uri;
		outTexture->width = image.width;
		outTexture->height = image.height;
		outTexture->channels = image.component;
		outTexture->bitsPerChannel = image.bits;

		// Determine format based on components and bits
		if ( image.bits == 8 )
		{
			switch ( image.component )
			{
				case 1:
					outTexture->format = VK_FORMAT_R8_UNORM;
					break;
				case 2:
					outTexture->format = VK_FORMAT_R8G8_UNORM;
					break;
				case 3:
					outTexture->format = VK_FORMAT_R8G8B8_UNORM;
					break;
				case 4:
					outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
					break;
				default:
					LogWarning( XRLIB_NAME, "Unsupported component count: %d, defaulting to RGBA8", image.component );
					outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
			}
		}
		else if ( image.bits == 16 )
		{
			switch ( image.component )
			{
				case 1:
					outTexture->format = VK_FORMAT_R16_UNORM;
					break;
				case 2:
					outTexture->format = VK_FORMAT_R16G16_UNORM;
					break;
				case 3:
					outTexture->format = VK_FORMAT_R16G16B16_UNORM;
					break;
				case 4:
					outTexture->format = VK_FORMAT_R16G16B16A16_UNORM;
					break;
				default:
					LogWarning( XRLIB_NAME, "Unsupported component count: %d, defaulting to RGBA16", image.component );
					outTexture->format = VK_FORMAT_R16G16B16A16_UNORM;
			}
		}
		else
		{
			LogWarning( XRLIB_NAME, "Unsupported bits per channel: %d, defaulting to 8-bit RGBA", image.bits );
			outTexture->format = VK_FORMAT_R8G8B8A8_UNORM;
		}

		// Parse sampler if present
		if ( gltfTexture.samplerIndex && *gltfTexture.samplerIndex < model.asset.samplers.size() )
		{
			const fastgltf::Sampler &sampler = model.asset.samplers[ *gltfTexture.samplerIndex ];

			// Set sampler properties
			outTexture->samplerConfig.minFilter = ConvertMinFilter( static_cast< int >( sampler.minFilter.value_or( fastgltf::Filter::Linear ) ) );
			outTexture->samplerConfig.magFilter = ConvertMagFilter( static_cast< int >( sampler.magFilter.value_or( fastgltf::Filter::Linear ) ) );
			outTexture->samplerConfig.addressModeU = ConvertWrappingMode( static_cast< int >( sampler.wrapS ) );
			outTexture->samplerConfig.addressModeV = ConvertWrappingMode( static_cast< int >( sampler.wrapT ) );
			outTexture->samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default for 2D textures

			// Handle mipmapping
			if ( sampler.minFilter == fastgltf::Filter::NearestMipMapNearest || sampler.minFilter == fastgltf::Filter::NearestMipMapLinear || sampler.minFilter == fastgltf::Filter::LinearMipMapNearest ||
				 sampler.minFilter == fastgltf::Filter::LinearMipMapLinear )
			{
				// Calculate max mip levels based on texture dimensions
				outTexture->samplerConfig.mipLevels = static_cast< uint32_t >( std::floor( std::log2( ( std::max )( image.width, image.height ) ) ) ) + 1;
				outTexture->samplerConfig.minLod = 0.0f;
				outTexture->samplerConfig.maxLod = static_cast< float >( outTexture->samplerConfig.mipLevels );
			}
			else
			{
				outTexture->samplerConfig.mipLevels = 1;
				outTexture->samplerConfig.minLod = 0.0f;
				outTexture->samplerConfig.maxLod = 0.0f;
			}
		}
		else
		{
			// Set default sampler properties
			outTexture->samplerConfig.minFilter = VK_FILTER_LINEAR;
			outTexture->samplerConfig.magFilter = VK_FILTER_LINEAR;
			outTexture->samplerConfig.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			outTexture->samplerConfig.mipLevels = 1;
			outTexture->samplerConfig.minLod = 0.0f;
			outTexture->samplerConfig.maxLod = 0.0f;
		}

		// Copy image data
		if ( !image.image.empty() )
		{
			outTexture->data = image.image;
			outTexture->mips = image.mips;
			outTexture->samplerConfig.mipLevels = image.mips.empty() ? 1 : static_cast< uint32_t >( image.mips.size() );
			outTexture->samplerConfig.maxLod = static_cast< float >( outTexture->samplerConfig.mipLevels - 1 );

			const size_t textureIndex = &gltfTexture - model.asset.textures.data();
			const bool colorTexture = std::any_of( model.asset.materials.begin(), model.asset.materials.end(), [&]( const auto &material ) {
				return ( material.pbrData.baseColorTexture && material.pbrData.baseColorTexture->textureIndex == textureIndex ) ||
					( material.emissiveTexture && material.emissiveTexture->textureIndex == textureIndex );
			} );
			const bool srgbView = colorTexture && outTexture->format == VK_FORMAT_R8G8B8A8_UNORM;

			// Create image
			VK_CHECK_RESULT( vkutils::CreateImage(
				outTexture->image,
				outTexture->memory,
				m_pSession->GetVulkan()->GetVkLogicalDevice(),
				m_pSession->GetVulkan()->GetVkPhysicalDevice(),
				outTexture->width,
				outTexture->height,
				outTexture->format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, srgbView ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0, outTexture->samplerConfig.mipLevels ) );

			// Create image view
			VK_CHECK_RESULT( vkutils::CreateImageView( outTexture->view, m_pSession->GetVulkan()->GetVkLogicalDevice(), outTexture->image, outTexture->format, VK_IMAGE_ASPECT_COLOR_BIT, outTexture->samplerConfig.mipLevels ) );

			if ( srgbView )
				VK_CHECK_RESULT( vkutils::CreateImageView( outTexture->srgbView, m_pSession->GetVulkan()->GetVkLogicalDevice(), outTexture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, outTexture->samplerConfig.mipLevels ) );

			// Upload image to gpu buffer and transition image for shader reads
			if ( !m_options.batchTextureUploads )
			{
				const vkutils::SImageUpload upload { outTexture->image, outTexture->data, static_cast< uint32_t >( outTexture->width ), static_cast< uint32_t >( outTexture->height ), outTexture->format, outTexture->mips };
				VK_CHECK_RESULT( vkutils::UploadTextureDataToImages( m_pSession->GetVulkan()->GetVkLogicalDevice(), m_pSession->GetVulkan()->GetVkPhysicalDevice(), commandPool, m_pSession->GetVulkan()->GetVkQueue_Graphics(), { &upload, 1 } ) );
			}

			// Create texture sampler
			VkSamplerCreateInfo samplerInfo {};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = outTexture->samplerConfig.magFilter;
			samplerInfo.minFilter = outTexture->samplerConfig.minFilter;

			if ( outTexture->samplerConfig.minFilter == VK_FILTER_LINEAR )
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			else if ( outTexture->samplerConfig.minFilter == VK_FILTER_NEAREST )
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			else
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

			samplerInfo.addressModeU = outTexture->samplerConfig.addressModeU;
			samplerInfo.addressModeV = outTexture->samplerConfig.addressModeV;
			samplerInfo.addressModeW = outTexture->samplerConfig.addressModeW;
			samplerInfo.anisotropyEnable = outTexture->samplerConfig.anisotropyEnable ? VK_TRUE : VK_FALSE;
			samplerInfo.maxAnisotropy = outTexture->samplerConfig.maxAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = outTexture->samplerConfig.minLod;
			samplerInfo.maxLod = outTexture->samplerConfig.maxLod;

			VK_CHECK_RESULT( vkCreateSampler( m_pSession->GetVulkan()->GetVkLogicalDevice(), &samplerInfo, nullptr, &outTexture->sampler ) );
		}
	}

	template< typename T >
	static int TextureIndex( const T &texture )
	{
		return texture ? static_cast< int >( texture->textureIndex ) : -1;
	}

	void CGltf::IdentifyTextureTypes( std::vector< STexture > &textures, const fastgltf::Asset &model )
	{
		// Go through each material to identify texture types
		for ( const auto &material : model.materials )
		{
			// Base color texture
			if ( TextureIndex( material.pbrData.baseColorTexture ) >= 0 )
			{
				int texIndex = static_cast< int >( model.textures[ TextureIndex( material.pbrData.baseColorTexture ) ].imageIndex.value_or( model.images.size() ) );
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::BaseColor;
				}
			}

			// Metallic roughness texture
			if ( TextureIndex( material.pbrData.metallicRoughnessTexture ) >= 0 )
			{
				int texIndex = static_cast< int >( model.textures[ TextureIndex( material.pbrData.metallicRoughnessTexture ) ].imageIndex.value_or( model.images.size() ) );
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::MetallicRoughness;
				}
			}

			// Normal map
			if ( TextureIndex( material.normalTexture ) >= 0 )
			{
				int texIndex = static_cast< int >( model.textures[ TextureIndex( material.normalTexture ) ].imageIndex.value_or( model.images.size() ) );
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Normal;
				}
			}

			// Emissive map
			if ( TextureIndex( material.emissiveTexture ) >= 0 )
			{
				int texIndex = static_cast< int >( model.textures[ TextureIndex( material.emissiveTexture ) ].imageIndex.value_or( model.images.size() ) );
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Emissive;
				}
			}

			// Occlusion map
			if ( TextureIndex( material.occlusionTexture ) >= 0 )
			{
				int texIndex = static_cast< int >( model.textures[ TextureIndex( material.occlusionTexture ) ].imageIndex.value_or( model.images.size() ) );
				if ( texIndex >= 0 && texIndex < textures.size() )
				{
					textures[ texIndex ].type = ETextureType::Occlusion;
				}
			}
		}
	}

	void CGltf::ParseMaterials( CRenderModel *outRenderModel, const fastgltf::Asset &model )
	{
		outRenderModel->materials.reserve( model.materials.size() );
		for ( const auto &gltfMaterial : model.materials )
		{
			SMaterial material;
			ParseMaterial( &material, model, gltfMaterial );
			outRenderModel->materials.push_back( material );

			// Parse texture type referenced by this material
			IdentifyTextureTypes( outRenderModel->textures, model );
		}
		const bool needsDefault = std::any_of( model.meshes.begin(), model.meshes.end(), []( const auto &mesh ) {
			return std::any_of( mesh.primitives.begin(), mesh.primitives.end(), []( const auto &primitive ) { return !primitive.materialIndex.has_value(); } );
		} );
		if ( needsDefault )
			outRenderModel->materials.emplace_back();

	}

	void CGltf::ParseMaterial( SMaterial *outMaterial, const fastgltf::Asset &model, const fastgltf::Material &gltfMaterial )
	{
		// Parse PBR Metallic Roughness properties
		if ( gltfMaterial.pbrData.baseColorFactor.size() == 4 )
		{
			for ( int i = 0; i < 4; i++ )
			{
				outMaterial->baseColorFactor[ i ] = static_cast< float >( gltfMaterial.pbrData.baseColorFactor[ i ] );
			}
		}

		outMaterial->metallicFactor = gltfMaterial.pbrData.metallicFactor;
		outMaterial->roughnessFactor = gltfMaterial.pbrData.roughnessFactor;
		outMaterial->normalScale = gltfMaterial.normalTexture ? gltfMaterial.normalTexture->scale : 1.f;
		outMaterial->occlusionStrength = gltfMaterial.occlusionTexture ? gltfMaterial.occlusionTexture->strength : 1.f;
		auto SetUV = [&]( const auto &texture, uint32_t flag ) {
			if ( texture && texture->texCoordIndex > 1 )
				throw std::runtime_error( "xrvk supports TEXCOORD_0 and TEXCOORD_1" );
			outMaterial->setTextureFlag( flag << TEXTURE_UV1_SHIFT, texture && texture->texCoordIndex == 1 );
		};
		SetUV( gltfMaterial.pbrData.baseColorTexture, TEXTURE_BASE_COLOR_BIT );
		SetUV( gltfMaterial.pbrData.metallicRoughnessTexture, TEXTURE_METALLIC_ROUGH_BIT );
		SetUV( gltfMaterial.normalTexture, TEXTURE_NORMAL_BIT );
		SetUV( gltfMaterial.emissiveTexture, TEXTURE_EMISSIVE_BIT );
		SetUV( gltfMaterial.occlusionTexture, TEXTURE_OCCLUSION_BIT );

		// Parse texture indices
		outMaterial->baseColorTexture = TextureIndex( gltfMaterial.pbrData.baseColorTexture );
		outMaterial->metallicRoughnessTexture = TextureIndex( gltfMaterial.pbrData.metallicRoughnessTexture );
		outMaterial->normalTexture = TextureIndex( gltfMaterial.normalTexture );
		outMaterial->occlusionTexture = TextureIndex( gltfMaterial.occlusionTexture );
		outMaterial->emissiveTexture = TextureIndex( gltfMaterial.emissiveTexture );

		// Parse emissive factor
		if ( gltfMaterial.emissiveFactor.size() == 3 )
		{
			for ( int i = 0; i < 3; i++ )
			{
				outMaterial->emissiveFactor[ i ] = static_cast< float >( gltfMaterial.emissiveFactor[ i ] );
			}
		}

		// Parse alpha mode
		if ( gltfMaterial.alphaMode == fastgltf::AlphaMode::Mask )
		{
			outMaterial->setAlphaMode( EAlphaMode::Mask );
		}
		else if ( gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend )
		{
			outMaterial->setAlphaMode( EAlphaMode::Blend );
		}
		else // "OPAQUE" or any other value
		{
			outMaterial->setAlphaMode( EAlphaMode::Opaque );
		}

		// Parse alpha cutoff
		outMaterial->alphaCutoff = static_cast< float >( gltfMaterial.alphaCutoff );

		// Parse double sided flag
		outMaterial->doubleSided = gltfMaterial.doubleSided;
	}

	void CGltf::ParseSkins( CRenderModel *outRenderModel, const fastgltf::Asset &model )
	{
		if ( model.skins.size() < 1 )
			return;

		outRenderModel->skins.resize( model.skins.size() );
		for ( uint32_t i = 0; i < model.skins.size(); i++ )
		{
			ParseSkin( &outRenderModel->skins[ i ], model, model.skins[ i ] );
		}

		if ( model.skins.size() > MAX_JOINT_COUNT )
		{
			LogError( XRLIB_NAME, "Max joint count (%i) execeeded in gltf file (%i).\nLoad skeletal meshes as single files if possible.", 
				MAX_JOINT_COUNT, model.skins.size() );
			throw std::runtime_error( "Maximum joints supported by the renderer reached in glTF file" );
		}
			
	}

	void CGltf::ParseSkin( SSkin *outSkin, const fastgltf::Asset &model, const fastgltf::Skin &gltfSkin )
	{
		outSkin->name = gltfSkin.name;
		outSkin->skeleton = gltfSkin.skeleton ? static_cast< int >( *gltfSkin.skeleton ) : -1;

		// Parse joints with validation
		outSkin->joints.reserve( gltfSkin.joints.size() );
		for ( const int jointIndex : gltfSkin.joints )
		{
			if ( jointIndex < 0 )
			{
				throw std::runtime_error( "Invalid negative joint index in glTF skin" );
			}
			if ( jointIndex >= static_cast< int >( model.nodes.size() ) )
			{
				throw std::runtime_error( "Joint index exceeds number of nodes in glTF model" );
			}

			outSkin->joints.push_back( static_cast< uint32_t > ( jointIndex ) );
		}

		// Build parent-child hierarchy
		outSkin->hierarchy.clear();

		// Create mapping from node index to joint index for quick lookups
		std::unordered_map< uint32_t, uint32_t > nodeToJointIndex;
		for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
		{
			nodeToJointIndex[ gltfSkin.joints[ i ] ] = i;
		}

		// Iterate over joints and process their children
		for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
		{
			uint32_t jointNodeIndex = gltfSkin.joints[ i ];
			const fastgltf::Node &node = model.nodes[ jointNodeIndex ];

			// Process each child of this node
			for ( int childNodeIndex : node.children )
			{
				// Check if the child is also a joint in this skin
				auto childJointIt = nodeToJointIndex.find( childNodeIndex );
				if ( childJointIt != nodeToJointIndex.end() )
				{
					// Add child joint index to parent's hierarchy entry
					outSkin->hierarchy[ i ].push_back( childJointIt->second );
				}
			}
		}

		// Parse inverse bind matrices if available
		if ( gltfSkin.inverseBindMatrices.has_value() )
		{
			const fastgltf::Accessor &accessor = model.accessors[ *gltfSkin.inverseBindMatrices ];

			size_t numMatrices = accessor.count;
			outSkin->inverseBindMatrices.resize( numMatrices );

			// Create a mapping from node index to joint index
			std::unordered_map< uint32_t, uint32_t > nodeToJointIndex;
			for ( size_t i = 0; i < gltfSkin.joints.size(); ++i )
			{
				nodeToJointIndex[ gltfSkin.joints[ i ] ] = i;
			}

			// Process each joint node
			for ( size_t i = 0; i < numMatrices; i++ )
			{
				uint32_t nodeIndex = gltfSkin.joints[ i ];			 // Get the node index from joints array
				uint32_t jointIndex = nodeToJointIndex[ nodeIndex ]; // Convert to sequential joint index

				// Get the inverse bind matrix for this joint
				float mat[ 16 ];
				const auto matrix = fastgltf::getAccessorElement< fastgltf::math::fmat4x4 >( model, accessor, i );
				memcpy( mat, matrix.data(), sizeof( mat ) );

				// Manually transpose the matrix (glTF is column-major, we need row-major)
				XrMatrix4x4f transposed;
				transposed.m[ 0 ] = mat[ 0 ];
				transposed.m[ 1 ] = mat[ 4 ];
				transposed.m[ 2 ] = mat[ 8 ];
				transposed.m[ 3 ] = mat[ 12 ];
				transposed.m[ 4 ] = mat[ 1 ];
				transposed.m[ 5 ] = mat[ 5 ];
				transposed.m[ 6 ] = mat[ 9 ];
				transposed.m[ 7 ] = mat[ 13 ];
				transposed.m[ 8 ] = mat[ 2 ];
				transposed.m[ 9 ] = mat[ 6 ];
				transposed.m[ 10 ] = mat[ 10 ];
				transposed.m[ 11 ] = mat[ 14 ];
				transposed.m[ 12 ] = mat[ 3 ];
				transposed.m[ 13 ] = mat[ 7 ];
				transposed.m[ 14 ] = mat[ 11 ];
				transposed.m[ 15 ] = mat[ 15 ];

				float pitch, yaw, roll;
				ExtractEulerAngles( transposed, pitch, yaw, roll );

				// Add correction for coordinate systems
				XrQuaternionf xRotation {
					-0.7071067811865476f, // sin(90/2) for X rotation
					0.0f,
					0.0f,
					0.7071067811865476f // cos(90/2)
				};
				XrQuaternionf yRotation {
					0.0f,
					0.0f,
					0.7071067811865476f, // sin(90/2) for Y rotation
					0.7071067811865476f	 // cos(90/2)
				};

				// Convert matrix to TRS components
				XrVector3f position, scale;
				XrQuaternionf rotation;

				XrMatrix4x4f_GetTranslation( &position, &transposed );
				XrMatrix4x4f_GetRotation( &rotation, &transposed );
				XrMatrix4x4f_GetScale( &scale, &transposed );

				// Apply correction
				XrQuaternionf tempRotation;
				XrQuaternionf_Multiply( &tempRotation, &xRotation, &rotation );
				XrQuaternionf_Multiply( &rotation, &yRotation, &tempRotation );

				// Rebuild matrix with corrected rotation
				XrMatrix4x4f_CreateTranslationRotationScale( &outSkin->inverseBindMatrices[ jointIndex ], &position, &rotation, &scale );

				ExtractEulerAngles( outSkin->inverseBindMatrices[ jointIndex ], pitch, yaw, roll );

				// Normalize the matrix (removes additional scaling from gltf file)
				NormalizeMatrix( outSkin->inverseBindMatrices[ jointIndex ] );
			}
		}

	}

	VkFilter CGltf::ConvertMagFilter( int gltfFilter )
	{
		switch ( gltfFilter )
		{
			case static_cast< int >( fastgltf::Filter::Nearest ):
				return VK_FILTER_NEAREST;
			case static_cast< int >( fastgltf::Filter::Linear ):
				return VK_FILTER_LINEAR;
			default:
				return VK_FILTER_LINEAR; // Default to linear filtering
		}
	}

	VkFilter CGltf::ConvertMinFilter( int gltfFilter )
	{
		switch ( gltfFilter )
		{
			case static_cast< int >( fastgltf::Filter::Nearest ):
			case static_cast< int >( fastgltf::Filter::NearestMipMapNearest ):
			case static_cast< int >( fastgltf::Filter::NearestMipMapLinear ):
				return VK_FILTER_NEAREST;

			case static_cast< int >( fastgltf::Filter::Linear ):
			case static_cast< int >( fastgltf::Filter::LinearMipMapNearest ):
			case static_cast< int >( fastgltf::Filter::LinearMipMapLinear ):
				return VK_FILTER_LINEAR;

			default:
				return VK_FILTER_LINEAR; // Default to linear filtering
		}
	}

	VkSamplerAddressMode CGltf::ConvertWrappingMode( int gltfWrap )
	{
		switch ( gltfWrap )
		{
			case static_cast< int >( fastgltf::Wrap::ClampToEdge ):
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			case static_cast< int >( fastgltf::Wrap::MirroredRepeat ):
				return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

			case static_cast< int >( fastgltf::Wrap::Repeat ):
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;

			default:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default to repeat mode
		}
	}

} // namespace xrlib
