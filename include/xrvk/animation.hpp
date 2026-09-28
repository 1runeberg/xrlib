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

#pragma once

#include <fastgltf/types.hpp>
#include <string_view>
#include <xrvk/mesh.hpp>

namespace xrlib
{

	// std430 entries shared with the morph buffers in skinning.glsl
	struct alignas( 16 ) SMorphDelta
	{
		XrVector4f position {};
		XrVector4f normal {};
		XrVector4f tangent {};
	};

	struct alignas( 16 ) SMorphVertex
	{
		uint32_t firstDelta = 0;
		uint32_t targetCount = 0;
		uint32_t firstWeight = 0;
		uint32_t targetStride = 0;
	};

	static_assert( sizeof( SMorphDelta ) == 48 );
	static_assert( sizeof( SMorphVertex ) == 16 );

	struct SAnimationMesh
	{
		size_t nodeIndex;
		size_t firstVertex;
		size_t vertexCount;
		std::vector< std::vector< SMorphDelta > > morphTargets;
		bool bRecalculateNormals = false;
	};

	// Owns decoded model data and playback state, the source asset can be released after construction
	class CAnimation
	{
	  public:

		CAnimation( const fastgltf::Asset &asset, const std::vector< SMeshVertex > &vertices, const std::vector< SAnimationMesh > &meshes );

		void SetClip( size_t unClipIndex );
		void SetClip( std::string_view sName );
		size_t GetClipCount() const { return m_vecClips.size(); }
		std::string_view GetClipName( size_t unClipIndex ) const;
		double GetDuration() const;

		void Sample( double seconds, bool bLoop = true );

		void Deform( std::vector< SMeshVertex > &outVertices ) const;

		// Rest vertices with validated weights and indices into GetSkinningMatrices
		void GetSkinningVertices( std::vector< SMeshVertex > &outVertices ) const;

		// Model-space transforms for rigid nodes and skins, with identity at index zero
		void GetSkinningMatrices( std::vector< XrMatrix4x4f > &outMatrices ) const;

		const std::vector< SMorphVertex > &GetMorphVertices() const { return m_vecMorphVertices; }
		const std::vector< SMorphDelta > &GetMorphDeltas() const { return m_vecMorphDeltas; }
		const std::vector< float > &GetMorphWeights() const { return m_vecMorphWeights; }

	  private:
		struct SNode
		{
			std::variant< fastgltf::TRS, fastgltf::math::fmat4x4 > transform;
			std::vector< size_t > children;
			size_t skinIndex = SIZE_MAX;
			size_t firstWeight = 0;
			std::vector< float > weights;
		};

		struct SSkinData
		{
			std::vector< size_t > joints;
			std::vector< fastgltf::math::fmat4x4 > inverseBind;
		};

		struct SCurve
		{
			size_t nodeIndex;
			fastgltf::AnimationPath path;
			fastgltf::AnimationInterpolation interpolation;
			std::vector< float > times;
			std::vector< fastgltf::math::fvec4 > values;
			std::vector< float > weights;
		};

		struct SClip
		{
			std::string name;
			std::vector< SCurve > curves;
			float start = 0.f;
			float end = 0.f;
		};

		std::vector< SNode > m_vecNodes;
		std::vector< std::variant< fastgltf::TRS, fastgltf::math::fmat4x4 > > m_vecPose;
		std::vector< size_t > m_vecNodeOrder;
		std::vector< size_t > m_vecParents;
		std::vector< SSkinData > m_vecSkins;
		std::vector< SClip > m_vecClips;
		std::vector< SAnimationMesh > m_vecMeshes;
		std::vector< SMeshVertex > m_vecRestVertices;
		std::vector< SMorphVertex > m_vecMorphVertices;
		std::vector< SMorphDelta > m_vecMorphDeltas;
		std::vector< float > m_vecMorphWeights;
		std::vector< fastgltf::math::fmat4x4 > m_vecWorld;
		std::vector< std::vector< fastgltf::math::fmat4x4 > > m_vecPalettes;
		size_t m_unClipIndex = SIZE_MAX;
	};
} // namespace xrlib
