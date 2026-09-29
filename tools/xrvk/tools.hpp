/*
 * Copyright 2026 Rune Berg
 * https://github.com/1runeberg | http://runeberg.io
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xrlib::tools
{
	using Options = std::map< std::string, std::string, std::less<> >;

	// Subcommands receive the arguments after their name and return the process exit code
	int PrepareGltf( std::span< char *const > args );
	int PrepareEnvironment( std::span< char *const > args );
	int PrepareBackground( std::span< char *const > args );
	int PrepareNightGrid( std::span< char *const > args );

	// Value options take the following argument, switches are stored with an empty value
	Options ParseArguments( std::span< char *const > args, std::initializer_list< std::string_view > values, std::initializer_list< std::string_view > switches );
	const std::string &Required( const Options &options, std::string_view name );
	uint32_t ParsePositive( std::string_view text );
	float ParsePositiveFloat( std::string_view text );

	std::vector< uint8_t > ReadFile( const std::filesystem::path &path );
	void WriteFile( const std::filesystem::path &path, std::span< const uint8_t > bytes );

	// Resolves a path or a program name on PATH
	std::optional< std::filesystem::path > FindProgram( const std::string &program );

	// Runs a host program directly, throws if it can't start or returns a non-zero exit code
	void RunProgram( const std::vector< std::string > &args );
	std::string CaptureProgram( const std::vector< std::string > &args );
} // namespace xrlib::tools
