/* 
 * Copyright 2024,2025 Copyright Rune Berg 
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

#include <xrvk/renderables.hpp>

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

	struct SColoredVertex
	{
		XrVector3f position = { 0.f, 0.f, 0.f };
		XrVector4f color =  { 0.f, 0.f, 0.f, 1.f };	  
	};

	class CDeviceBuffer;
	class CPlane2D : public CRenderable
	{
	  public:
		CPlane2D(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CPlane2D(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		~CPlane2D();

		const VkDeviceSize vertexOffsets[ 1 ] = { 0 };

		XrSpace space = XR_NULL_HANDLE;
		XrPosef pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } };
		XrVector3f scale = { 1.f, 1.f, 1.f };

		void AddTri( XrVector2f v1, XrVector2f v2, XrVector2f v3 );
		void AddIndex( unsigned short index );
		void AddVertex( XrVector2f vertex );

		// Interfaces
		void Reset() override;
		VkResult InitBuffers( bool bReset = false ) override;
		void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) override;

		void ResetIndices();
		void ResetVertices();

		std::vector< unsigned short > *GetIndices() { return &m_vecIndices; }
		std::vector< XrVector2f > *GetVertices() { return &m_vecVertices; }

	  protected:
		std::vector< unsigned short > m_vecIndices;
		std::vector< XrVector2f > m_vecVertices;

		// Interfaces
		void DeleteBuffers() override;
	};

	class CPrimitive : public CRenderable
	{
	  public:

		CPrimitive( 
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CPrimitive(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		~CPrimitive();

		const VkDeviceSize vertexOffsets[ 1 ] = { 0 };

		// Interfaces
		void Reset() override;
		VkResult InitBuffers( bool bReset = false ) override;
		virtual void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) override;

		void AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 );
		void AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );

		void AddIndex( unsigned short index );
		void AddVertex( XrVector3f vertex );

		void ResetIndices();
		void ResetVertices();
	
		std::vector< unsigned short > &GetIndices() { return m_vecIndices; }
		std::vector< XrVector3f > &GetVertices() { return m_vecVertices; }

	  protected:
		std::vector< unsigned short > m_vecIndices;
		std::vector< XrVector3f > m_vecVertices;

		void DeleteBuffers() override;
	};

	class CColoredPrimitive : public CRenderable
	{
	  public:
		CColoredPrimitive(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			float fAlpha = 1.0f,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CColoredPrimitive(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE,
			float fAlpha = 1.0f );

		~CColoredPrimitive() {}

		const VkDeviceSize vertexOffsets[ 2 ] = { 0, sizeof( XrVector3f ) };

		// Interfaces
		void Reset() override;
		VkResult InitBuffers( bool bReset = false ) override;
		virtual void Draw( const VkCommandBuffer commandBuffer, const CRenderInfo &renderInfo ) override;

		void AddIndex( unsigned short index );
		void AddVertex( XrVector3f vertex );
		void AddColoredVertex( XrVector3f vertex, XrVector3f color, float fAlpha = 1.f );

		void AddTri( XrVector3f v1, XrVector3f v2, XrVector3f v3 );
		void AddColoredTri( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f color, float fAlpha = 1.f );

		void AddQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddColoredQuadCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha = 1.f );

		void AddQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4 );
		void AddColoredQuadCCW( XrVector3f v1, XrVector3f v2, XrVector3f v3, XrVector3f v4, XrVector3f color, float fAlpha = 1.f );
		
		void Recolor( XrVector3f color, float fAlpha = 1.f );

		void ResetIndices();
		void ResetVertices();

		std::vector< unsigned short > &GetIndices() { return m_vecIndices; }
		std::vector< SColoredVertex > &GetVertices() { return m_vecVertices; }

	  protected:
		std::vector< unsigned short > m_vecIndices;
		std::vector< SColoredVertex > m_vecVertices;

		void DeleteBuffers() override;
	};

	class CPlane : public CPrimitive
	{
	  public:
		CPlane(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			bool bFacePlayer = false,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CPlane( CSession *pSession, CRenderInfo *pRenderInfo, bool bFacePlayer = false, bool bIsVisible = true, XrVector3f xrScale = { 1.f, 1.f, 1.f }, XrSpace xrSpace = XR_NULL_HANDLE );

		~CPlane();

	  protected:
		void InitShape( bool bFacePlayer = false );
	};

	class CColoredPlane : public CColoredPrimitive
	{
	  public:
		CColoredPlane(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			bool bFacePlayer = false,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			float fAlpha = 1.0f,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CColoredPlane( CSession *pSession, CRenderInfo *pRenderInfo, bool bFacePlayer = false, bool bIsVisible = true, XrVector3f xrScale = { 1.f, 1.f, 1.f }, XrSpace xrSpace = XR_NULL_HANDLE, float fAlpha = 1.0f );

		~CColoredPlane() {}

	  private:
		void InitShape( bool bFacePlayer = false, float fAlpha = 1.0f );
	};

	class CPyramid : public CPrimitive
	{
	  public:
		CPyramid( 
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CPyramid(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		~CPyramid() {}

		private:
			void InitShape();
	};

	class CColoredPyramid : public CColoredPrimitive
	{
	  public:
		CColoredPyramid(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			float fAlpha = 1.0f,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CColoredPyramid(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE,
			float fAlpha = 1.0f );

		~CColoredPyramid() {}

		private:
			void InitShape( float fAlpha = 1.0f );
	};

	class CColoredCube : public CColoredPrimitive
	{
	  public:
		CColoredCube(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			float fAlpha = 1.0f,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CColoredCube(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE,
			float fAlpha = 1.0f );

		~CColoredCube() {}

		private:
			void InitShape( float fAlpha = 1.0f );
	};

	class CInvertedCube : public CColoredPrimitive
	{
	  public:
		CInvertedCube(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			uint16_t pipelineLayoutIdx,
			uint16_t graphicsPipelineIdx,
			uint32_t descriptorLayoutIdx = std::numeric_limits< uint32_t >::max(),
			bool bIsVisible = true,
			float fAlpha = 1.0f,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE );

		CInvertedCube(
			CSession *pSession,
			CRenderInfo *pRenderInfo,
			bool bIsVisible = true,
			XrVector3f xrScale = { 1.f, 1.f, 1.f },
			XrSpace xrSpace = XR_NULL_HANDLE,
			float fAlpha = 1.0f );

		~CInvertedCube() {}

		private:
			void InitShape( float fAlpha = 1.0f );
	};
}