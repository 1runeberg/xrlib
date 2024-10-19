/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <xrvk/buffer.hpp>

namespace xrlib
{
	constexpr XrVector3f k_ColorRed { 1.f, 0.f, 0.f };
	constexpr XrVector3f k_ColorGreen { 0.f, 1.f, 0.f };
	constexpr XrVector3f k_ColorBlue { 0.f, 0.f, 1.f };
	constexpr XrVector3f k_ColorGold { 0.75f, 0.75f, 0.f };
	constexpr XrVector3f k_ColorPurple { 0.25f, 0.f, 0.25f };
	constexpr XrVector3f k_ColorTeal { 0.0f, 0.25f, 0.25f };
	constexpr XrVector3f k_ColorMagenta { 1.f, 0.f, 1.f };
	constexpr XrVector3f k_ColorOrange { 1.f, 0.25f, 0.f };

	enum class EAssetMotionType
	{
		STATIC_INIT = 0,
		STATIC_NO_INIT = 1,
		DYNAMIC = 2,
		MAX = 3

	};

	struct SColoredVertex
	{
		XrVector3f position = { 0.f, 0.f, 0.f };
		XrVector4f color =  { 0.f, 0.f, 0.f, 1.f };	  
	};

	class CDeviceBuffer;
	class CPrimitive
	{
	  public:

		struct SInstanceState
		{
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT;
			XrSpace space = XR_NULL_HANDLE;
			XrPosef pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } };
			XrVector3f scale = { 1.f, 1.f, 1.f };

			SInstanceState( XrVector3f defaultScale = { 1.f, 1.f, 1.f } )
				: scale( defaultScale )
			{
			}

			
			SInstanceState( EAssetMotionType defaultMotionType, XrSpace defaultSpace = XR_NULL_HANDLE, XrVector3f defaultScale = { 1.f, 1.f, 1.f } ) : 
				motionType(defaultMotionType), 
				space( defaultSpace ),
				scale( defaultScale )
			{
			}

			~SInstanceState() {}
		};

		CPrimitive( 
			CSession *pSession, 
			uint32_t drawPriority = 0, 
			XrVector3f scale = { 1.f, 1.f, 1.f },
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT, 
			VkPipeline graphicsPipeline = VK_NULL_HANDLE, 
			XrSpace space = XR_NULL_HANDLE );
		~CPrimitive();

		bool isVisible = true;

		std::vector< SInstanceState > instances;
		std::vector< XrMatrix4x4f > instanceMatrices;

		uint32_t unDrawPriority = 0;
		VkPipeline pipeline = VK_NULL_HANDLE;

		const VkDeviceSize vertexOffsets[ 1 ] = { 0 };
		const VkDeviceSize instanceOffsets[ 4 ] = { 0, 4 * sizeof( float ), 8 * sizeof( float ), 12 * sizeof( float ) };

		VkResult InitBuffers();

		VkResult InitBuffer(
			CDeviceBuffer *pBuffer,
			VkBufferUsageFlags usageFlags,
			VkDeviceSize unSize,
			void *pData,
			VkMemoryPropertyFlags memPropFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VkAllocationCallbacks *pCallbacks = nullptr );

		void ResetScale( float x, float y, float z, uint32_t unInstanceIndex = 0 );
		void ResetScale( float fScale, uint32_t unInstanceIndex = 0 );
		void Scale( float fPercent, uint32_t unInstanceIndex = 0 );

		void AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 );
		void AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );

		void AddIndex( unsigned short index );
		void AddVertex( XrVector3f vertex );

		void Reset();
		void ResetIndices();
		void ResetVertices();

		CDeviceBuffer *UpdateBuffer( VkCommandBuffer transferCmdBuffer );

		uint32_t GetInstanceCount() { return (uint32_t) instances.size(); }
		uint32_t AddInstance( uint32_t unCount, XrVector3f scale = { 1.f, 1.f, 1.f } );
		
		CDeviceBuffer *GetIndexBuffer() { return m_pIndexBuffer; }
		CDeviceBuffer *GetVertexBuffer() { return m_pVertexBuffer; }
		CDeviceBuffer *GetInstanceBuffer() { return m_pInstanceBuffer; }

		XrVector3f *GetPosition( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].pose.position; } 
		XrQuaternionf *GetOrientation( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].pose.orientation; } 
		XrVector3f *GetScale( uint32_t unInstanceindex ) { return &instances[ unInstanceindex ].scale; } 

		void UpdateModelMatrix( uint32_t unInstanceIndex = 0, XrSpace baseSpace = XR_NULL_HANDLE, XrTime time = 0, bool bForceUpdate = false );
		XrMatrix4x4f *GetModelMatrix( uint32_t unInstanceIndex = 0, bool bRefresh = false );
		XrMatrix4x4f *GetUpdatedModelMatrix( uint32_t unInstanceIndex = 0 ) { return GetModelMatrix( unInstanceIndex, true ); }

		std::vector< unsigned short > &GetIndices() { return m_vecIndices; }
		std::vector< XrVector3f > &GetVertices() { return m_vecVertices; }


	  protected:
		CSession *m_pSession = nullptr;
		CDeviceBuffer *m_pIndexBuffer = nullptr;
		CDeviceBuffer *m_pVertexBuffer = nullptr;
		CDeviceBuffer *m_pInstanceBuffer = nullptr;

		std::vector< unsigned short > m_vecIndices;
		std::vector< XrVector3f > m_vecVertices;


		void DeleteBuffers();
	};

	class CColoredPrimitive : public CPrimitive
	{
	  public:
		CColoredPrimitive(
			CSession *pSession,
			uint32_t drawPriority = 0,
			XrVector3f scale = { 1.f, 1.f, 1.f },
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT,
			float fAlpha = 1.f,
			VkPipeline graphicsPipeline = VK_NULL_HANDLE,
			XrSpace space = XR_NULL_HANDLE );
		~CColoredPrimitive();

		const VkDeviceSize vertexOffsets[ 2 ] = { 0, sizeof( XrVector3f ) };

		void AddVertex( XrVector3f vertex );
		void AddColoredVertex( XrVector3f vertex, XrVector3f color, float fAlpha = 1.f );

		void AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 );
		void AddColoredTri( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f color, float fAlpha = 1.f );

		void AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddColoredQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha = 1.f );

		void AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddColoredQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha = 1.f );
		
		void Recolor( XrVector3f color, float fAlpha = 1.f );

		VkResult InitBuffers();

		std::vector< SColoredVertex > &GetVertices() { return m_vecVertices; }

	  private:
		std::vector< SColoredVertex > m_vecVertices;
	};

	class CPyramid : public CPrimitive
	{
	  public:
		CPyramid( 
			CSession *pSession, 
			uint32_t drawPriority = 0, 
			XrVector3f scale = { 1.f, 1.f, 1.f },
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT, 
			VkPipeline graphicsPipeline = VK_NULL_HANDLE, 
			XrSpace space = XR_NULL_HANDLE );
		~CPyramid();
	};

	class CColoredPyramid : public CColoredPrimitive
	{
	  public:
		CColoredPyramid(
			CSession *pSession,
			uint32_t drawPriority = 0,
			XrVector3f scale = { 1.f, 1.f, 1.f },
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT,
			float fAlpha = 1.f,
			VkPipeline graphicsPipeline = VK_NULL_HANDLE,
			XrSpace space = XR_NULL_HANDLE );
		~CColoredPyramid();
	};

	class CColoredCube : public CColoredPrimitive
	{
	  public:
		CColoredCube(
			CSession *pSession,
			uint32_t drawPriority = 0,
			XrVector3f scale = { 1.f, 1.f, 1.f },
			EAssetMotionType motionType = EAssetMotionType::STATIC_INIT,
			float fAlpha = 1.f,
			VkPipeline graphicsPipeline = VK_NULL_HANDLE,
			XrSpace space = XR_NULL_HANDLE );
		~CColoredCube();
	};
}