/* 
 * Copyright 2023-24 Beyond Reality Labs Ltd (https://beyondreality.io)
 * Copyright 2021-24 Rune Berg (GitHub: https://github.com/1runeberg, YT: https://www.youtube.com/@1RuneBerg, X: https://twitter.com/1runeberg, BSky: https://runeberg.social)
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#pragma once

#include <xrlib/ext/ext_base.hpp>
#include <xrlib/common.hpp>
#include <xrlib/instance.hpp>
#include <xrlib/data_types_bitmasks.hpp>

#include <assert.h>
#include <string>
#include <vector>


namespace xrlib
{
	struct Instance;
	class ExtBase_Passthrough : public xrlib::ExtBase
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

		ExtBase_Passthrough( XrInstance xrInstance, std::string sName ) : ExtBase ( xrInstance, sName ) {};
		~ExtBase_Passthrough() {};
		bool IsActive() { return m_bIsActive; }

		virtual XrResult Init( XrSession xrSession, CInstance *pInstance, void *pOtherInfo ) = 0;
		virtual XrResult Start() = 0;
		virtual XrResult Stop() = 0;
		virtual XrResult PauseLayer( int index = -1 ) = 0;
		virtual XrResult ResumeLayer( int index = -1 ) = 0;

		virtual XrResult AddLayer( XrSession xrSession, ELayerType eLayerType, XrCompositionLayerFlags flags, 
			float fOpacity = 1.0f, XrSpace xrSpace = XR_NULL_HANDLE, void* pOtherInfo = nullptr ) = 0;

		virtual XrResult RemoveLayer( uint32_t unIndex ) = 0;
		
		virtual void GetCompositionLayers( std::vector< XrCompositionLayerBaseHeader * > &outCompositionLayers ) = 0;

	protected:
		bool m_bIsActive = false;

	};

} 
