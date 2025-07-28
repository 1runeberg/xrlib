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

#include <xrlib/ext/ext_base.hpp>
#include <xrlib/common.hpp>
#include <xrlib/instance.hpp>
#include <xrlib/data_types_bitmasks.hpp>

#include <string>
#include <vector>


namespace xrlib
{
	struct Instance;
	class ExtBase_Passthrough : public xrlib::CExtBase
	{
	  public:
		enum class ELayerType
		{
			FULLSCREEN = 1 << 0,		// 1
			MESH_PROJECTION = 1 << 1,	// 2
			//FLAG_RESERVED = 1 << 2,	// 4
			//FLAG_RESERVED = 1 << 3,	// 8
			//FLAG_RESERVED = 1 << 4,	// 16
			//FLAG_RESERVED = 1 << 5,	// 32
			//FLAG_RESERVED = 1 << 6,	// 64
			//FLAG_RESERVED = 1 << 7	// 128 
		};

		Flag8 flagSupportedLayerTypes { 0 };

		ExtBase_Passthrough( XrInstance xrInstance, const std::string& sName ) : CExtBase ( xrInstance, sName ) {};
		~ExtBase_Passthrough() = default;
		bool IsActive() const { return m_bIsActive; }

		virtual XrResult Init( XrSession xrSession, CInstance *pInstance, void *pOtherInfo = nullptr ) = 0;
		virtual XrResult Start() = 0;
		virtual XrResult Stop() = 0;
		virtual XrResult PauseLayer( int index = -1 ) = 0;
		virtual XrResult ResumeLayer( int index = -1 ) = 0;

		virtual XrResult AddLayer( XrSession xrSession, ELayerType eLayerType, XrCompositionLayerFlags flags, XrFlags64 layerFlags = 0,
			float fOpacity = 1.0f, XrSpace xrSpace = XR_NULL_HANDLE, void* pOtherInfo = nullptr ) = 0;

		virtual XrResult RemoveLayer( uint32_t unIndex ) = 0;
		
		virtual void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, bool bReset = true ) = 0;
		virtual void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers, std::vector< uint32_t > &vecIndices, bool bReset = true ) = 0;

	protected:
		bool m_bIsActive = false;

	};

} 
