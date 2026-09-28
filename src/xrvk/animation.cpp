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

#include <cmath>
#include <fastgltf/tools.hpp>
#include <limits>
#include <xrvk/animation.hpp>

namespace xrlib
{
	using namespace fastgltf::math;

	CAnimation::CAnimation( const fastgltf::Asset &asset, const std::vector< SMeshVertex > &vertices, const std::vector< SAnimationMesh > &meshes )
		: m_vecMeshes( meshes )
		, m_vecRestVertices( vertices )
	{
		m_vecParents.resize( asset.nodes.size(), SIZE_MAX );
		for ( size_t i = 0; i < asset.nodes.size(); ++i )
		{
			const auto &node = asset.nodes[ i ];
			SNode data { node.transform, {}, node.skinIndex.value_or( SIZE_MAX ) };
			if ( data.skinIndex != SIZE_MAX && data.skinIndex >= asset.skins.size() )
				throw std::runtime_error( "Invalid animation skin index" );

			data.firstWeight = m_vecMorphWeights.size();
			if ( node.meshIndex )
			{
				const auto &mesh = asset.meshes.at( *node.meshIndex );
				const size_t count = mesh.primitives.empty() ? 0 : mesh.primitives.front().targets.size();
				for ( const auto &primitive : mesh.primitives )
					if ( primitive.targets.size() != count )
						throw std::runtime_error( "Morph target counts differ between mesh primitives" );

				if ( !mesh.weights.empty() && mesh.weights.size() != count )
					throw std::runtime_error( "Invalid mesh morph weight count" );

				const auto &weights = node.weights.empty() ? mesh.weights : node.weights;
				if ( !weights.empty() && weights.size() != count )
					throw std::runtime_error( "Invalid node morph weight count" );

				data.weights.assign( count, 0.f );
				for ( size_t j = 0; j < weights.size(); ++j )
				{
					data.weights[ j ] = static_cast< float >( weights[ j ] );
					if ( !std::isfinite( data.weights[ j ] ) )
						throw std::runtime_error( "Non-finite morph weight" );
				}

				if ( count > UINT32_MAX - m_vecMorphWeights.size() )
					throw std::runtime_error( "Too many morph weights" );

				m_vecMorphWeights.insert( m_vecMorphWeights.end(), data.weights.begin(), data.weights.end() );
			}
			else if ( !node.weights.empty() )
				throw std::runtime_error( "Morph weights require a mesh" );

			for ( size_t child : node.children )
			{
				if ( child >= asset.nodes.size() || m_vecParents[ child ] != SIZE_MAX )
					throw std::runtime_error( "Invalid animation node hierarchy" );

				m_vecParents[ child ] = i;
				data.children.push_back( child );
			}

			m_vecNodes.push_back( std::move( data ) );
		}

		for ( size_t i = 0; i < m_vecNodes.size(); ++i )
			if ( m_vecParents[ i ] == SIZE_MAX )
				m_vecNodeOrder.push_back( i );

		for ( size_t i = 0; i < m_vecNodeOrder.size(); ++i )
			for ( size_t child : m_vecNodes[ m_vecNodeOrder[ i ] ].children )
				m_vecNodeOrder.push_back( child );

		if ( m_vecNodeOrder.size() != m_vecNodes.size() )
			throw std::runtime_error( "Cyclic animation node hierarchy" );

		for ( auto &mesh : m_vecMeshes )
		{
			if ( mesh.nodeIndex >= m_vecNodes.size() || mesh.firstVertex > vertices.size() || mesh.vertexCount > vertices.size() - mesh.firstVertex )
				throw std::runtime_error( "Invalid animated mesh range" );

			if ( mesh.bRecalculateNormals && mesh.vertexCount % 3 )
				throw std::runtime_error( "Flat morph meshes require split triangles" );

			const auto &node = m_vecNodes[ mesh.nodeIndex ];
			if ( mesh.morphTargets.size() != node.weights.size() )
				throw std::runtime_error( "Missing or mismatched mesh morph targets" );
			if ( mesh.morphTargets.empty() )
				continue;

			if ( vertices.size() > UINT32_MAX || mesh.vertexCount > ( UINT32_MAX - m_vecMorphDeltas.size() ) / mesh.morphTargets.size() )
				throw std::runtime_error( "Too much morph vertex data" );

			if ( m_vecMorphVertices.empty() )
				m_vecMorphVertices.resize( vertices.size() );

			const uint32_t firstDelta = static_cast< uint32_t >( m_vecMorphDeltas.size() );
			for ( const auto &target : mesh.morphTargets )
			{
				if ( target.size() != mesh.vertexCount )
					throw std::runtime_error( "Invalid morph vertex count" );

				m_vecMorphDeltas.insert( m_vecMorphDeltas.end(), target.begin(), target.end() );
			}

			for ( size_t i = 0; i < mesh.vertexCount; ++i )
				m_vecMorphVertices[ mesh.firstVertex + i ] = { firstDelta + static_cast< uint32_t >( i ), static_cast< uint32_t >( node.weights.size() ), static_cast< uint32_t >( node.firstWeight ), static_cast< uint32_t >( mesh.vertexCount ) };

			// The flattened buffers own the deltas from here
			std::vector< std::vector< SMorphDelta > >().swap( mesh.morphTargets );
		}

		for ( const auto &skin : asset.skins )
		{
			SSkinData data;
			for ( size_t joint : skin.joints )
			{
				if ( joint >= m_vecNodes.size() )
					throw std::runtime_error( "Invalid animation joint" );

				data.joints.push_back( joint );
			}

			data.inverseBind.resize( skin.joints.size() );
			if ( skin.inverseBindMatrices )
			{
				const auto &accessor = asset.accessors.at( *skin.inverseBindMatrices );
				if ( accessor.count != skin.joints.size() )
					throw std::runtime_error( "Invalid inverse bind matrix count" );

				fastgltf::copyFromAccessor< fmat4x4 >( asset, accessor, data.inverseBind.data() );
			}

			m_vecSkins.push_back( std::move( data ) );
		}

		for ( const auto &animation : asset.animations )
		{
			SClip clip;
			clip.name = animation.name;
			clip.start = std::numeric_limits< float >::infinity();
			for ( const auto &channel : animation.channels )
			{
				if ( !channel.nodeIndex )
					continue;

				const bool bWeights = channel.path == fastgltf::AnimationPath::Weights;
				const auto &node = m_vecNodes.at( *channel.nodeIndex );
				if ( bWeights && node.weights.empty() )
					throw std::runtime_error( "Weight animation requires morph targets" );

				if ( !bWeights && !std::holds_alternative< fastgltf::TRS >( node.transform ) )
					throw std::runtime_error( "Animated nodes must use TRS transforms" );

				const auto &sampler = animation.samplers.at( channel.samplerIndex );
				SCurve curve { *channel.nodeIndex, channel.path, sampler.interpolation, {}, {}, {} };
				fastgltf::iterateAccessor< float >( asset, asset.accessors.at( sampler.inputAccessor ), [ & ]( float time ) { curve.times.push_back( time ); } );
				if ( curve.times.empty() )
					throw std::runtime_error( "Animation channel has no keys" );

				for ( size_t i = 0; i < curve.times.size(); ++i )
					if ( !std::isfinite( curve.times[ i ] ) || curve.times[ i ] < 0 || ( i && curve.times[ i ] <= curve.times[ i - 1 ] ) )
						throw std::runtime_error( "Animation key times must increase" );

				const auto &output = asset.accessors.at( sampler.outputAccessor );
				const size_t multiplier = sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline ? 3 : 1;
				if ( output.count != curve.times.size() * multiplier * ( bWeights ? node.weights.size() : 1 ) )
					throw std::runtime_error( "Animation key counts don't match" );

				if ( bWeights )
				{
					if ( output.type != fastgltf::AccessorType::Scalar || output.componentType != fastgltf::ComponentType::Float )
						throw std::runtime_error( "Morph animation weights must be float scalars" );

					fastgltf::iterateAccessor< float >(
						asset,
						output,
						[ & ]( float value )
						{
							if ( !std::isfinite( value ) )
								throw std::runtime_error( "Non-finite morph animation key" );

							curve.weights.push_back( value );
						} );
				}
				else if ( channel.path == fastgltf::AnimationPath::Rotation )
					fastgltf::iterateAccessor< fvec4 >( asset, output, [ & ]( auto value ) { curve.values.push_back( value ); } );
				else
					fastgltf::iterateAccessor< fvec3 >( asset, output, [ & ]( auto value ) { curve.values.emplace_back( value[ 0 ], value[ 1 ], value[ 2 ], 0.f ); } );

				for ( const auto &value : curve.values )
					for ( size_t i = 0; i < 4; ++i )
						if ( !std::isfinite( value[ i ] ) )
							throw std::runtime_error( "Non-finite animation key" );

				clip.start = std::min( clip.start, curve.times.front() );
				clip.end = std::max( clip.end, curve.times.back() );
				clip.curves.push_back( std::move( curve ) );
			}

			if ( clip.curves.empty() )
				clip.start = clip.end = 0.f;

			m_vecClips.push_back( std::move( clip ) );
		}

		m_vecPose.resize( m_vecNodes.size() );
		m_vecWorld.resize( m_vecNodes.size() );
		m_vecPalettes.resize( m_vecSkins.size() );
		for ( size_t i = 0; i < m_vecSkins.size(); ++i )
			m_vecPalettes[ i ].resize( m_vecSkins[ i ].joints.size() );

		Sample( 0 );
	}

	void CAnimation::SetClip( size_t unClipIndex )
	{
		if ( unClipIndex >= m_vecClips.size() )
			throw std::out_of_range( "Animation clip wasn't found" );

		m_unClipIndex = unClipIndex;
		Sample( 0 );
	}

	void CAnimation::SetClip( std::string_view sName )
	{
		for ( size_t i = 0; i < m_vecClips.size(); ++i )
			if ( m_vecClips[ i ].name == sName )
			{
				SetClip( i );
				return;
			}

		throw std::runtime_error( "Animation clip wasn't found" );
	}

	std::string_view CAnimation::GetClipName( size_t unClipIndex ) const { return m_vecClips.at( unClipIndex ).name; }

	double CAnimation::GetDuration() const
	{
		if ( m_unClipIndex == SIZE_MAX )
			return 0;

		const auto &clip = m_vecClips[ m_unClipIndex ];
		return static_cast< double >( clip.end ) - clip.start;
	}

	void CAnimation::Sample( double seconds, bool bLoop )
	{
		if ( !std::isfinite( seconds ) )
			throw std::invalid_argument( "Animation time must be finite" );

		for ( size_t i = 0; i < m_vecNodes.size(); ++i )
		{
			const auto &node = m_vecNodes[ i ];
			m_vecPose[ i ] = node.transform;
			std::copy( node.weights.begin(), node.weights.end(), m_vecMorphWeights.begin() + node.firstWeight );
		}

		if ( m_unClipIndex != SIZE_MAX )
		{
			const auto &clip = m_vecClips[ m_unClipIndex ];
			const double duration = GetDuration();
			const double elapsed = bLoop && duration > 0 ? std::fmod( std::max( 0.0, seconds ), duration ) : std::clamp( seconds, 0.0, duration );
			const float time = clip.start + static_cast< float >( elapsed );
			for ( const auto &curve : clip.curves )
			{
				const auto upper = std::upper_bound( curve.times.begin(), curve.times.end(), time );
				const size_t next = std::min( static_cast< size_t >( upper - curve.times.begin() ), curve.times.size() - 1 );
				const size_t previous = upper == curve.times.begin() ? 0 : static_cast< size_t >( upper - curve.times.begin() - 1 );
				const float interval = next == previous ? 0.f : curve.times[ next ] - curve.times[ previous ];
				const float amount = interval == 0 ? 0.f : std::clamp( ( time - curve.times[ previous ] ) / interval, 0.f, 1.f );
				if ( curve.path == fastgltf::AnimationPath::Weights )
				{
					const auto &node = m_vecNodes[ curve.nodeIndex ];
					const size_t count = node.weights.size();
					for ( size_t i = 0; i < count; ++i )
					{
						float value;
						if ( curve.interpolation == fastgltf::AnimationInterpolation::CubicSpline )
						{
							const float t2 = amount * amount, t3 = t2 * amount;
							value = curve.weights[ ( previous * 3 + 1 ) * count + i ] * ( 2 * t3 - 3 * t2 + 1 ) + curve.weights[ ( previous * 3 + 2 ) * count + i ] * ( ( t3 - 2 * t2 + amount ) * interval ) +
									curve.weights[ ( next * 3 + 1 ) * count + i ] * ( -2 * t3 + 3 * t2 ) + curve.weights[ next * 3 * count + i ] * ( ( t3 - t2 ) * interval );
						}
						else
						{
							const float a = curve.weights[ previous * count + i ];
							const float b = curve.weights[ next * count + i ];
							value = curve.interpolation == fastgltf::AnimationInterpolation::Step ? a : a + ( b - a ) * amount;
						}

						if ( !std::isfinite( value ) )
							throw std::runtime_error( "Animation produced a non-finite morph weight" );

						m_vecMorphWeights[ node.firstWeight + i ] = value;
					}

					continue;
				}

				fvec4 value;
				if ( curve.interpolation == fastgltf::AnimationInterpolation::CubicSpline )
				{
					const float t2 = amount * amount, t3 = t2 * amount;
					value = curve.values[ previous * 3 + 1 ] * ( 2 * t3 - 3 * t2 + 1 ) + curve.values[ previous * 3 + 2 ] * ( ( t3 - 2 * t2 + amount ) * interval ) + curve.values[ next * 3 + 1 ] * ( -2 * t3 + 3 * t2 ) +
							curve.values[ next * 3 ] * ( ( t3 - t2 ) * interval );
				}
				else
				{
					const auto &a = curve.values[ previous ];
					const auto &b = curve.values[ next ];
					const float factor = curve.interpolation == fastgltf::AnimationInterpolation::Step ? 0.f : amount;
					if ( curve.path == fastgltf::AnimationPath::Rotation )
					{
						const auto rotation = slerp( fquat( a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] ), fquat( b[ 0 ], b[ 1 ], b[ 2 ], b[ 3 ] ), factor );
						value = fvec4( rotation.x(), rotation.y(), rotation.z(), rotation.w() );
					}
					else
						value = a + ( b - a ) * factor;
				}

				auto &trs = std::get< fastgltf::TRS >( m_vecPose[ curve.nodeIndex ] );
				if ( curve.path == fastgltf::AnimationPath::Rotation )
				{
					if ( dot( value, value ) <= 1e-20f )
						throw std::runtime_error( "Animation produced a zero quaternion" );

					trs.rotation = normalize( fquat( value[ 0 ], value[ 1 ], value[ 2 ], value[ 3 ] ) );
				}
				else
				{
					auto &target = curve.path == fastgltf::AnimationPath::Translation ? trs.translation : trs.scale;
					target = { value[ 0 ], value[ 1 ], value[ 2 ] };
				}
			}
		}

		for ( size_t node : m_vecNodeOrder )
		{
			fastgltf::Node pose;
			pose.transform = m_vecPose[ node ];
			const auto local = fastgltf::getTransformMatrix( pose );
			const size_t parent = m_vecParents[ node ];
			m_vecWorld[ node ] = parent == SIZE_MAX ? local : m_vecWorld[ parent ] * local;
		}

		for ( size_t i = 0; i < m_vecSkins.size(); ++i )
			for ( size_t j = 0; j < m_vecSkins[ i ].joints.size(); ++j )
				m_vecPalettes[ i ][ j ] = m_vecWorld[ m_vecSkins[ i ].joints[ j ] ] * m_vecSkins[ i ].inverseBind[ j ];
	}

	void CAnimation::GetSkinningVertices( std::vector< SMeshVertex > &outVertices ) const
	{
		size_t count = 1 + m_vecWorld.size();
		std::vector< size_t > offsets;
		for ( const auto &palette : m_vecPalettes )
		{
			offsets.push_back( count );
			count += palette.size();
		}

		if ( count > static_cast< size_t >( INT32_MAX ) )
			throw std::runtime_error( "Too many skinning matrices" );

		outVertices = m_vecRestVertices;
		for ( auto &vertex : outVertices )
		{
			std::fill_n( vertex.joints, JOINT_INFLUENCE_COUNT, 0u );
			std::fill_n( vertex.weights, JOINT_INFLUENCE_COUNT, 0.f );
			vertex.weights[ 0 ] = 1.f;
		}

		for ( const auto &mesh : m_vecMeshes )
		{
			const size_t skin = m_vecNodes[ mesh.nodeIndex ].skinIndex;
			for ( size_t i = mesh.firstVertex; i < mesh.firstVertex + mesh.vertexCount; ++i )
			{
				auto &vertex = outVertices[ i ];

				// The PBR fragment shader derives flat normals from the deformed surface
				if ( mesh.bRecalculateNormals )
				{
					vertex.normal = {};
					vertex.tangent = {};
				}

				if ( skin == SIZE_MAX )
				{
					vertex.joints[ 0 ] = static_cast< uint32_t >( 1 + mesh.nodeIndex );
					continue;
				}

				const auto &rest = m_vecRestVertices[ i ];
				float total = 0;
				for ( size_t j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
				{
					const float weight = rest.weights[ j ];
					if ( !std::isfinite( weight ) || weight < 0 )
						throw std::runtime_error( "Invalid skin weight" );
					if ( weight > 0 && rest.joints[ j ] >= m_vecPalettes[ skin ].size() )
						throw std::runtime_error( "Invalid skin joint index" );

					vertex.weights[ j ] = weight;
					vertex.joints[ j ] = weight > 0 ? static_cast< uint32_t >( offsets[ skin ] + rest.joints[ j ] ) : 0u;
					total += weight;
				}

				if ( !std::isfinite( total ) || total <= 0 )
					throw std::runtime_error( "Skinned vertex has no valid joint weights" );
				for ( auto &weight : vertex.weights )
					weight /= total;
			}
		}
	}

	void CAnimation::GetSkinningMatrices( std::vector< XrMatrix4x4f > &outMatrices ) const
	{
		size_t count = 1 + m_vecWorld.size();
		for ( const auto &palette : m_vecPalettes )
			count += palette.size();
		outMatrices.resize( count );
		XrMatrix4x4f_CreateIdentity( &outMatrices[ 0 ] );

		size_t index = 1;
		for ( const auto &world : m_vecWorld )
			std::memcpy( outMatrices[ index++ ].m, world.data(), sizeof( XrMatrix4x4f ) );
		for ( const auto &palette : m_vecPalettes )
			for ( const auto &joint : palette )
				std::memcpy( outMatrices[ index++ ].m, joint.data(), sizeof( XrMatrix4x4f ) );
	}

	void CAnimation::Deform( std::vector< SMeshVertex > &outVertices ) const
	{
		outVertices = m_vecRestVertices;
		for ( const auto &mesh : m_vecMeshes )
		{
			const size_t skin = m_vecNodes[ mesh.nodeIndex ].skinIndex;
			for ( size_t i = mesh.firstVertex; i < mesh.firstVertex + mesh.vertexCount; ++i )
			{
				auto rest = m_vecRestVertices[ i ];
				if ( !m_vecMorphVertices.empty() )
				{
					const auto &morph = m_vecMorphVertices[ i ];
					for ( size_t target = 0; target < morph.targetCount; ++target )
					{
						const float weight = m_vecMorphWeights[ morph.firstWeight + target ];
						const auto &delta = m_vecMorphDeltas[ morph.firstDelta + target * morph.targetStride ];
						auto Add = [ weight ]( auto &value, const XrVector4f &offset )
						{
							value.x += offset.x * weight;
							value.y += offset.y * weight;
							value.z += offset.z * weight;
						};
						Add( rest.position, delta.position );
						Add( rest.normal, delta.normal );
						Add( rest.tangent, delta.tangent );
					}
				}

				auto transform = m_vecWorld[ mesh.nodeIndex ];
				if ( skin != SIZE_MAX )
				{
					transform = fmat4x4( 0.f );
					float total = 0.f;
					for ( size_t j = 0; j < JOINT_INFLUENCE_COUNT; ++j )
					{
						const float weight = rest.weights[ j ];
						if ( !std::isfinite( weight ) || weight < 0 )
							throw std::runtime_error( "Invalid skin weight" );

						if ( weight == 0 )
							continue;

						const auto &joint = m_vecPalettes[ skin ].at( rest.joints[ j ] );
						for ( size_t column = 0; column < 4; ++column )
							transform[ column ] += joint[ column ] * weight;

						total += weight;
					}

					if ( total <= 0 )
						throw std::runtime_error( "Skinned vertex has no joint weights" );

					transform = transform * ( 1.f / total );
				}

				auto &vertex = outVertices[ i ];
				const auto position = transform * fvec4( rest.position.x, rest.position.y, rest.position.z, 1.f );
				vertex.position = { position[ 0 ], position[ 1 ], position[ 2 ] };

				const fmat3x3 linear( transform );
				const float determinant = dot( linear[ 0 ], cross( linear[ 1 ], linear[ 2 ] ) );
				if ( std::abs( determinant ) <= 1e-20f )
					throw std::runtime_error( "Animation produced a singular vertex transform" );

				auto normal = transpose( inverse( linear ) ) * fvec3( rest.normal.x, rest.normal.y, rest.normal.z );
				normal = normal / std::sqrt( std::max( dot( normal, normal ), 1e-20f ) );
				vertex.normal = { normal[ 0 ], normal[ 1 ], normal[ 2 ] };
				if ( rest.tangent.w != 0 )
				{
					auto tangent = linear * fvec3( rest.tangent.x, rest.tangent.y, rest.tangent.z );
					tangent = tangent - normal * dot( normal, tangent );
					tangent = tangent / std::sqrt( std::max( dot( tangent, tangent ), 1e-20f ) );
					vertex.tangent = { tangent[ 0 ], tangent[ 1 ], tangent[ 2 ], rest.tangent.w * ( determinant < 0 ? -1.f : 1.f ) };
				}
			}

			if ( mesh.bRecalculateNormals )
			{
				for ( size_t i = mesh.firstVertex; i < mesh.firstVertex + mesh.vertexCount; i += 3 )
				{
					const auto &a = outVertices[ i ].position;
					const auto &b = outVertices[ i + 1 ].position;
					const auto &c = outVertices[ i + 2 ].position;
					auto normal = cross( fvec3( b.x - a.x, b.y - a.y, b.z - a.z ), fvec3( c.x - a.x, c.y - a.y, c.z - a.z ) );
					normal = dot( normal, normal ) > 1e-20f ? normalize( normal ) : fvec3( 0.f, 1.f, 0.f );
					for ( size_t j = 0; j < 3; ++j )
					{
						outVertices[ i + j ].normal = { normal[ 0 ], normal[ 1 ], normal[ 2 ] };
						outVertices[ i + j ].tangent = {};
					}
				}
			}
		}
	}
} // namespace xrlib
