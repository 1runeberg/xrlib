/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <simdjson.h>
#include "tools.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <random>
#include <stdexcept>

namespace xrlib::tools
{
	namespace
	{
		namespace fs = std::filesystem;

		// Bump when prepared outputs change for the same inputs and ktx version
		constexpr std::string_view k_PreparedFormat = "xrvk gltf ktx2 1";
		constexpr std::array< uint8_t, 8 > k_PngSignature { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

		struct SManifest
		{
			std::string fingerprint;
			std::optional< std::string > tool;
			std::map< std::string, uint64_t > outputs;
		};

		// Removes the staging directory and anything left in it
		struct SStaging
		{
			fs::path path;

			explicit SStaging( const fs::path &parent )
			{
				std::random_device random;
				do
					path = parent / ( "xrvk-assets-" + std::to_string( random() ) );
				while ( !fs::create_directory( path ) );
			}

			~SStaging()
			{
				std::error_code error;
				fs::remove_all( path, error );
			}
		};

		bool IsWithin( const fs::path &path, const fs::path &base )
		{
			const fs::path relative = path.lexically_relative( base );
			return !relative.empty() && *relative.begin() != "..";
		}

		std::string Trim( std::string text )
		{
			text.erase( text.find_last_not_of( " \t\r\n" ) + 1 );
			text.erase( 0, text.find_first_not_of( " \t\r\n" ) );
			return text;
		}

		std::string JsonString( std::string_view text )
		{
			std::string json = "\"";
			for ( const char character : text )
			{
				if ( character == '"' || character == '\\' )
					json += { '\\', character };
				else if ( uint8_t( character ) < 0x20 )
				{
					char escaped[ 8 ];
					std::snprintf( escaped, sizeof( escaped ), "\\u%04x", character );
					json += escaped;
				}
				else
					json += character;
			}

			return json + '"';
		}

		// FNV-1a is enough to detect changed inputs, it isn't used for security
		void Hash( uint64_t &hash, std::span< const uint8_t > bytes )
		{
			for ( const uint8_t byte : bytes )
				hash = ( hash ^ byte ) * 0x100000001b3ull;
		}

		void Hash( uint64_t &hash, std::string_view text ) { Hash( hash, { reinterpret_cast< const uint8_t * >( text.data() ), text.size() } ); }

		std::optional< SManifest > ReadManifest( const fs::path &path )
		{
			if ( !fs::is_regular_file( path ) )
				return std::nullopt;

			simdjson::dom::parser parser;
			simdjson::dom::element root;
			std::string_view fingerprint;
			simdjson::dom::object outputs;
			if ( parser.load( path.string() ).get( root ) || root[ "fingerprint" ].get( fingerprint ) || root[ "outputs" ].get( outputs ) )
				return std::nullopt;

			SManifest manifest { std::string( fingerprint ), std::nullopt, {} };

			std::string_view tool;
			if ( !root[ "tool" ].get( tool ) )
				manifest.tool = std::string( tool );

			for ( const auto field : outputs )
			{
				uint64_t size = 0;
				if ( field.value.get( size ) )
					return std::nullopt;

				manifest.outputs[ std::string( field.key ) ] = size;
			}

			return manifest;
		}

		void WriteManifest( const fs::path &path, const SManifest &manifest )
		{
			std::string json = "{\n  \"fingerprint\": " + JsonString( manifest.fingerprint ) + ",\n  \"tool\": " + ( manifest.tool ? JsonString( *manifest.tool ) : "null" ) + ",\n  \"outputs\": {";

			for ( auto output = manifest.outputs.begin(); output != manifest.outputs.end(); ++output )
				json += ( output == manifest.outputs.begin() ? "\n    " : ",\n    " ) + JsonString( output->first ) + ": " + std::to_string( output->second );

			json += "\n  }\n}\n";
			WriteFile( path, { reinterpret_cast< const uint8_t * >( json.data() ), json.size() } );
		}
	} // namespace

	int PrepareGltf( std::span< char *const > args )
	{
		const Options options = ParseArguments( args, { "--source", "--output", "--ktx", "--encode" }, { "--force" } );
		const fs::path source = fs::weakly_canonical( Required( options, "--source" ) );
		const fs::path output = fs::weakly_canonical( Required( options, "--output" ) );
		const fs::path directory = source.parent_path();
		const std::string ktx = options.contains( "--ktx" ) ? Required( options, "--ktx" ) : "ktx";
		const bool bForce = options.contains( "--force" );

		const std::string encode = options.contains( "--encode" ) ? Required( options, "--encode" ) : "none";
		if ( encode != "none" && encode != "astc" )
			throw std::invalid_argument( "--encode must be none or astc" );

		const bool bAstc = encode == "astc";

		if ( output == directory || IsWithin( directory, output ) )
			throw std::invalid_argument( "Generated output must be separate from the source assets" );

		// Every local input is tracked for the fingerprint and copied beside the prepared textures
		std::map< fs::path, std::vector< uint8_t > > dependencies;
		auto ReadDependency = [ & ]( const fs::path &path ) -> const std::vector< uint8_t > &
		{
			const fs::path resolved = fs::weakly_canonical( path );
			if ( !IsWithin( resolved, directory ) )
				throw std::invalid_argument( "Asset inputs must be local files beside the source" );

			auto dependency = dependencies.find( resolved );
			if ( dependency == dependencies.end() )
				dependency = dependencies.emplace( resolved, ReadFile( resolved ) ).first;

			return dependency->second;
		};

		const std::vector< uint8_t > &raw = ReadDependency( source );
		auto buffer = fastgltf::GltfDataBuffer::FromBytes( reinterpret_cast< const std::byte * >( raw.data() ), raw.size() );
		if ( buffer.error() != fastgltf::Error::None )
			throw std::runtime_error( "Couldn't read " + source.string() );

		fastgltf::Parser parser;
		auto loaded = parser.loadGltf( buffer.get(), directory, fastgltf::Options::None );
		if ( loaded.error() != fastgltf::Error::None )
			throw std::runtime_error( std::string( fastgltf::getErrorMessage( loaded.error() ) ) );

		const fastgltf::Asset &asset = loaded.get();

		// GLB and data URI bytes are held by fastgltf, external files by the dependency map
		auto SourceBytes = [ & ]( const fastgltf::DataSource &data ) -> std::span< const uint8_t >
		{
			if ( const auto *array = std::get_if< fastgltf::sources::Array >( &data ) )
				return { reinterpret_cast< const uint8_t * >( array->bytes.data() ), array->bytes.size() };

			if ( const auto *view = std::get_if< fastgltf::sources::ByteView >( &data ) )
				return { reinterpret_cast< const uint8_t * >( view->bytes.data() ), view->bytes.size() };

			const auto *uri = std::get_if< fastgltf::sources::URI >( &data );
			if ( !uri || !uri->uri.isLocalPath() || uri->fileByteOffset )
				throw std::invalid_argument( "Asset inputs must be local files" );

			return ReadDependency( directory / uri->uri.fspath() );
		};

		std::vector< std::span< const uint8_t > > buffers;
		for ( const fastgltf::Buffer &gltfBuffer : asset.buffers )
			buffers.push_back( SourceBytes( gltfBuffer.data ) );

		std::vector< std::span< const uint8_t > > images;
		for ( const fastgltf::Image &image : asset.images )
		{
			const auto *view = std::get_if< fastgltf::sources::BufferView >( &image.data );
			if ( !view )
			{
				images.push_back( SourceBytes( image.data ) );
				continue;
			}

			const fastgltf::BufferView &bufferView = asset.bufferViews.at( view->bufferViewIndex );
			const std::span< const uint8_t > data = buffers.at( bufferView.bufferIndex );
			if ( bufferView.byteOffset > data.size() || bufferView.byteLength > data.size() - bufferView.byteOffset )
				throw std::invalid_argument( "Invalid image buffer view" );

			images.push_back( data.subspan( bufferView.byteOffset, bufferView.byteLength ) );
		}

		// Color space, mip wrap and normal use come from the material slots and samplers using each image
		std::vector< std::string_view > roles( images.size() );
		std::vector< std::optional< fastgltf::Wrap > > wraps( images.size() );
		std::vector< bool > normals( images.size() );
		auto Use = [ & ]( const auto &info, std::string_view role, bool bNormal = false )
		{
			if ( !info.has_value() )
				return;

			const fastgltf::Texture &texture = asset.textures.at( info->textureIndex );
			if ( !texture.imageIndex.has_value() )
				throw std::invalid_argument( "Textures must reference a PNG or JPEG image" );

			fastgltf::Wrap wrap = fastgltf::Wrap::Repeat;
			if ( texture.samplerIndex.has_value() )
			{
				const fastgltf::Sampler &sampler = asset.samplers.at( texture.samplerIndex.value() );
				if ( sampler.wrapS != sampler.wrapT )
					throw std::invalid_argument( "Prepared mip filtering currently requires matching S/T wrap modes" );

				wrap = sampler.wrapS;
			}

			const size_t image = texture.imageIndex.value();
			if ( ( !roles.at( image ).empty() && roles[ image ] != role ) || ( wraps[ image ] && wraps[ image ] != wrap ) )
				throw std::invalid_argument( "Images shared across color spaces or different wrap modes need separate source images" );

			roles[ image ] = role;
			wraps[ image ] = wrap;
			normals[ image ] = normals[ image ] || bNormal;
		};

		for ( const fastgltf::Material &material : asset.materials )
		{
			Use( material.pbrData.baseColorTexture, "srgb" );
			Use( material.emissiveTexture, "srgb" );
			Use( material.pbrData.metallicRoughnessTexture, "linear" );
			Use( material.normalTexture, "linear", true );
			Use( material.occlusionTexture, "linear" );
		}

		uint64_t hash = 0xcbf29ce484222325ull;
		Hash( hash, k_PreparedFormat );
		for ( const auto &[ path, data ] : dependencies )
		{
			Hash( hash, path.lexically_relative( directory ).generic_string() );
			Hash( hash, std::to_string( data.size() ) );
			Hash( hash, data );
		}

		// Uncompressed fingerprints stay as they were so bundled outputs aren't regenerated
		if ( bAstc )
			Hash( hash, encode );

		char fingerprint[ 17 ];
		std::snprintf( fingerprint, sizeof( fingerprint ), "%016llx", static_cast< unsigned long long >( hash ) );

		const fs::path manifestPath = output / "manifest.json";
		const std::optional< SManifest > previous = ReadManifest( manifestPath );

		const std::optional< fs::path > tool = FindProgram( ktx );
		const std::optional< std::string > version = tool ? std::optional( Trim( CaptureProgram( { tool->string(), "--version" } ) ) ) : std::nullopt;

		// Bundled outputs stay usable without ktx installed as long as the inputs haven't changed
		const bool bUpToDate = previous && previous->fingerprint == fingerprint && ( !version || version == previous->tool ) &&
							   std::all_of( previous->outputs.begin(), previous->outputs.end(), [ & ]( const auto &prepared ) {
								   std::error_code error;
								   const fs::path path = output / prepared.first;
								   return fs::is_regular_file( path, error ) && fs::file_size( path, error ) == prepared.second;
							   } );

		if ( !bForce && bUpToDate )
		{
			std::cout << "Prepared assets are up to date: " << output.string() << '\n';
			return 0;
		}

		if ( !tool )
			throw std::runtime_error( "Prepared assets are missing or stale and " + ktx + " wasn't found. Add KTX-Software's ktx to PATH or pass --ktx" );

		fs::create_directories( output.parent_path() );
		const SStaging staging( output.parent_path() );
		fs::create_directory( staging.path / "textures" );

		for ( size_t i = 0; i < images.size(); ++i )
		{
			const std::span< const uint8_t > data = images[ i ];
			const bool bPng = data.size() > 24 && std::equal( k_PngSignature.begin(), k_PngSignature.end(), data.begin() );
			const bool bJpeg = data.size() > 2 && data[ 0 ] == 0xff && data[ 1 ] == 0xd8;
			if ( !bPng && !bJpeg )
				throw std::invalid_argument( "The preprocessing input must contain PNG or JPEG images" );

			// PNG bit depth is the first byte after the IHDR size
			const bool bWide = bPng && data[ 24 ] == 16;
			const std::string role( roles[ i ].empty() ? "linear" : roles[ i ] );
			if ( bWide && role == "srgb" )
				throw std::invalid_argument( "16-bit sRGB preprocessing isn't supported by this profile" );

			const fs::path input = staging.path / ( bPng ? "input.png" : "input.jpg" );
			WriteFile( input, data );

			const fs::path target = staging.path / "textures" / ( "image-" + std::to_string( i ) + ".ktx2" );
			std::string format = bWide ? "R16G16B16A16_UNORM" : ( role == "srgb" ? "R8G8B8A8_SRGB" : "R8G8B8A8_UNORM" );

			// Normals keep smaller blocks to stay sharp, 16-bit sources stay uncompressed for their precision
			const bool bEncode = bAstc && !bWide;
			if ( bEncode )
				format = std::string( normals[ i ] ? "ASTC_4x4_" : "ASTC_6x6_" ) + ( role == "srgb" ? "SRGB_BLOCK" : "UNORM_BLOCK" );

			std::string wrap = "wrap";
			if ( wraps[ i ] == fastgltf::Wrap::ClampToEdge )
				wrap = "clamp";
			else if ( wraps[ i ] == fastgltf::Wrap::MirroredRepeat )
				wrap = "reflect";

			std::vector< std::string > command { tool->string(), "create", "--format", format, "--assign-tf", role, "--assign-primaries", role == "srgb" ? "bt709" : "none",
												 "--assign-texcoord-origin", "top-left", "--generate-mipmap", "--mipmap-filter", "box", "--mipmap-wrap", wrap };

			if ( bEncode )
				command.insert( command.end(), { "--astc-quality", "thorough" } );

			command.insert( command.end(), { input.string(), target.string() } );
			RunProgram( command );

			RunProgram( { tool->string(), "validate", target.string() } );
			fs::remove( input );
		}

		// Preserve source geometry and material metadata, textures are supplied by the sidecar directory
		for ( const auto &[ path, data ] : dependencies )
		{
			const fs::path destination = staging.path / path.lexically_relative( directory );
			fs::create_directories( destination.parent_path() );
			WriteFile( destination, data );
		}

		SManifest manifest { fingerprint, version, {} };
		for ( const auto &entry : fs::recursive_directory_iterator( staging.path ) )
			if ( entry.is_regular_file() )
				manifest.outputs[ entry.path().lexically_relative( staging.path ).generic_string() ] = entry.file_size();

		fs::create_directories( output );
		for ( const auto &[ name, size ] : manifest.outputs )
		{
			const fs::path destination = output / name;
			fs::create_directories( destination.parent_path() );
			fs::rename( staging.path / name, destination );
		}

		// Write the manifest last so an interrupted run is regenerated next time
		const fs::path temporaryManifest = output / "manifest.tmp";
		WriteManifest( temporaryManifest, manifest );
		fs::rename( temporaryManifest, manifestPath );

		std::cout << "Prepared " << images.size() << ( bAstc ? " ASTC" : "" ) << " KTX2 textures in " << output.string() << '\n';
		return 0;
	}
} // namespace xrlib::tools
