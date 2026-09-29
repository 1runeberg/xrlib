/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tools.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#ifdef _WIN32
	#define XRVK_TOOLS_POPEN _popen
	#define XRVK_TOOLS_PCLOSE _pclose
#else
	#define XRVK_TOOLS_POPEN popen
	#define XRVK_TOOLS_PCLOSE pclose
#endif

namespace xrlib::tools
{
	namespace
	{
		std::string Quote( const std::string &argument )
		{
#ifdef _WIN32
			if ( argument.find( '"' ) != std::string::npos )
				throw std::invalid_argument( "Quotes aren't supported in program arguments: " + argument );

			return '"' + argument + '"';
#else
			std::string quoted = "'";
			for ( const char character : argument )
				quoted += character == '\'' ? std::string( "'\\''" ) : std::string( 1, character );

			return quoted + "'";
#endif
		}

		std::string CommandLine( const std::vector< std::string > &args )
		{
			std::string command;
			for ( const std::string &argument : args )
				command += ( command.empty() ? "" : " " ) + Quote( argument );

#ifdef _WIN32
			// cmd /c strips the outer quotes when the program path is quoted too
			command = '"' + command + '"';
#endif
			return command;
		}
	} // namespace

	Options ParseArguments( std::span< char *const > args, std::initializer_list< std::string_view > values, std::initializer_list< std::string_view > switches )
	{
		Options options;
		for ( size_t i = 0; i < args.size(); ++i )
		{
			const std::string_view name = args[ i ];
			if ( std::find( switches.begin(), switches.end(), name ) != switches.end() )
			{
				options[ std::string( name ) ] = {};
				continue;
			}

			if ( std::find( values.begin(), values.end(), name ) == values.end() )
				throw std::invalid_argument( "Unknown option " + std::string( name ) );

			if ( i + 1 == args.size() )
				throw std::invalid_argument( "Missing value for " + std::string( name ) );

			options[ std::string( name ) ] = args[ ++i ];
		}

		return options;
	}

	const std::string &Required( const Options &options, std::string_view name )
	{
		const auto option = options.find( name );
		if ( option == options.end() || option->second.empty() )
			throw std::invalid_argument( "Missing required option " + std::string( name ) );

		return option->second;
	}

	uint32_t ParsePositive( std::string_view text )
	{
		uint32_t value = 0;
		const auto result = std::from_chars( text.data(), text.data() + text.size(), value );
		if ( result.ec != std::errc {} || result.ptr != text.data() + text.size() || !value )
			throw std::invalid_argument( "Expected a positive integer, got " + std::string( text ) );

		return value;
	}

	float ParsePositiveFloat( std::string_view text )
	{
		const std::string value( text );
		size_t parsed = 0;
		float result = 0.f;
		try
		{
			result = std::stof( value, &parsed );
		}
		catch ( const std::exception & )
		{
			parsed = 0;
		}

		if ( parsed != value.size() || !std::isfinite( result ) || result <= 0.f )
			throw std::invalid_argument( "Expected a positive number, got " + value );

		return result;
	}

	std::vector< uint8_t > ReadFile( const std::filesystem::path &path )
	{
		std::ifstream input( path, std::ios::binary );
		if ( !input )
			throw std::runtime_error( "Couldn't open " + path.string() );

		return { std::istreambuf_iterator< char >( input ), {} };
	}

	void WriteFile( const std::filesystem::path &path, std::span< const uint8_t > bytes )
	{
		std::ofstream output( path, std::ios::binary );
		output.write( reinterpret_cast< const char * >( bytes.data() ), std::streamsize( bytes.size() ) );
		output.close();

		if ( !output )
			throw std::runtime_error( "Couldn't write " + path.string() );
	}

	std::optional< std::filesystem::path > FindProgram( const std::string &program )
	{
		const std::filesystem::path candidate( program );
		if ( candidate.has_parent_path() )
			return std::filesystem::is_regular_file( candidate ) ? std::optional( candidate ) : std::nullopt;

#ifdef _WIN32
		constexpr char k_Separator = ';';
		const std::vector< std::string > names = candidate.has_extension() ? std::vector { program } : std::vector { program + ".exe", program };
#else
		constexpr char k_Separator = ':';
		const std::vector< std::string > names { program };
#endif

		const char *path = std::getenv( "PATH" );
		std::string_view directories = path ? path : "";

		while ( !directories.empty() )
		{
			const size_t end = std::min( directories.find( k_Separator ), directories.size() );
			const std::filesystem::path directory( directories.substr( 0, end ) );
			directories.remove_prefix( std::min( end + 1, directories.size() ) );

			for ( const std::string &name : names )
			{
				std::error_code error;
				if ( !directory.empty() && std::filesystem::is_regular_file( directory / name, error ) )
					return directory / name;
			}
		}

		return std::nullopt;
	}

	void RunProgram( const std::vector< std::string > &args )
	{
		// Keep our output ordered with the child's
		std::cout.flush();

		if ( std::system( CommandLine( args ).c_str() ) != 0 )
			throw std::runtime_error( "Command failed: " + CommandLine( args ) );
	}

	std::string CaptureProgram( const std::vector< std::string > &args )
	{
		FILE *pipe = XRVK_TOOLS_POPEN( CommandLine( args ).c_str(), "r" );
		if ( !pipe )
			throw std::runtime_error( "Couldn't start " + args.front() );

		std::string output;
		char buffer[ 256 ];
		while ( const size_t count = std::fread( buffer, 1, sizeof( buffer ), pipe ) )
			output.append( buffer, count );

		if ( XRVK_TOOLS_PCLOSE( pipe ) != 0 )
			throw std::runtime_error( "Command failed: " + CommandLine( args ) );

		return output;
	}
} // namespace xrlib::tools
