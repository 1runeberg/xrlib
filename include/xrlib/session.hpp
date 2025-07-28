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

#include <vector>
#include <xrlib/instance.hpp>

#define VK_CHECK_SUCCESS( result ) ( ( result ) == VK_SUCCESS )

namespace xrlib
{
	struct SSessionSettings
	{
		bool bUseMultiviewRendering = true;
		VkSurfaceKHR *pSurface = nullptr;
		XrSessionCreateFlags flgAdditionalCreateInfo = 0;
		void *pVkInstanceNext = nullptr;
		void *pXrVkInstanceNext = nullptr;
		void *pVkLogicalDeviceNext = nullptr;
		void *pXrLogicalDeviceNext = nullptr;
	};

	class CInstance;
	class CVulkan;
	class CSession
	{
	  public:
		CSession( CInstance *pInstance );
		~CSession();

		friend class CVulkan;

		XrPosef xrAppReferencePose { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } };
		XrReferenceSpaceType xrAppReferenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		XrViewConfigurationType xrViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

		XrResult Init( SSessionSettings &settings );
		XrResult Init(
			VkSurfaceKHR *pSurface = nullptr,
			XrSessionCreateFlags flgAdditionalCreateInfo = 0,
			void *pVkInstanceNext = nullptr,
			void *pXrVkInstanceNext = nullptr,
			void *pVkLogicalDeviceNext = nullptr,
			void *pXrLogicalDeviceNext = nullptr );

		XrResult RequestExit();
		XrResult InitVulkan( VkSurfaceKHR *pSurface = nullptr, void *pVkInstanceNext = nullptr, void *pXrVkInstanceNext = nullptr, void *pVkLogicalDeviceNext = nullptr, void *pXrLogicalDeviceNext = nullptr );
		XrResult CreateXrSession( XrSessionCreateFlags flgAdditionalCreateInfo = 0, void *pNext = nullptr );
		XrResult CreateAppSpace( XrPosef referencePose, XrReferenceSpaceType referenceSpaceType, void *pNext = nullptr );
		XrResult CreateHmdSpace( XrPosef referencePose, void *pNext = nullptr );
		
		XrResult Start( void *pOtherBeginInfo = nullptr);
		XrResult Poll( XrEventDataBaseHeader *outEventData );
		XrResult End( bool bRequestToExit = false );

		XrResult StartFrame( XrFrameState* pFrameState, void *pWaitFrameNext = nullptr, void *pBeginFrameNext = nullptr );

		XrResult UpdateEyeStates( 
			std::vector< XrView > &outEyeViews,
			std::array< XrMatrix4x4f, 2 > &outEyeProjections,
			XrViewState *outEyeViewsState,
			XrFrameState *pFrameState, 		
			XrSpace space, 
			const float fNearZ = 0.1f,
			const float fFarZ = 10000.f,
			XrViewConfigurationType viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 
			void *pNext = nullptr );

		XrResult AcquireFrameImage(
			uint32_t *outImageIndex,
			XrSwapchain swapchain,
			void *pNext = nullptr );

		XrResult AcquireFrameImage( 
			uint32_t *outColorImageIndex, 
			uint32_t *outDepthImageIndex,
			XrSwapchain colorSwapchain, 
			XrSwapchain depthSwapchain, 
			void *pColorAcquireNext = nullptr, 
			void *pDepthAcquireNext = nullptr );

		XrResult WaitForFrameImage( 
			XrSwapchain swapchain, 
			XrDuration duration = XR_INFINITE_DURATION, 
			void *pNext = nullptr );

		XrResult WaitForFrameImage( 
			XrSwapchain colorSwapchain, 
			XrSwapchain depthSwapchain, 
			XrDuration duration = XR_INFINITE_DURATION, 
			void *pNext = nullptr );

		XrResult ReleaseFrameImage( XrSwapchain swapchain, void *pNext = nullptr );
		XrResult ReleaseFrameImage( XrSwapchain colorSwapchain, XrSwapchain depthSwapchain, void *pNext = nullptr );

		XrResult EndFrame( 
			XrFrameState *pFrameState, 
			std::vector< XrCompositionLayerBaseHeader * > &vecFrameLayers, 
			XrEnvironmentBlendMode blendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE, 
			void *pNext = nullptr );

		XrResult LocateSpace( XrSpace baseSpace, XrSpace targetSpace, XrTime predictedDisplayTime, XrSpaceLocation *outSpaceLocation );
		XrResult UpdateHmdPose( XrTime predictedDisplayTime );

		std::vector< XrReferenceSpaceType > GetSupportedReferenceSpaceTypes();
		XrResult GetSupportedTextureFormats( std::vector< int64_t > &outSupportedFormats );
		int64_t SelectColorTextureFormat( const std::vector< int64_t > &vecRequestedFormats );
		int64_t SelectDepthTextureFormat( const std::vector< int64_t > &vecRequestedFormats );

		CInstance *GetAppInstance() { return m_pInstance;  }
		CVulkan *GetVulkan() { return m_pVulkan; }
		const XrSession GetXrSession() { return m_xrSession; }
		const XrSessionState GetState() { return m_xrSessionState; }
		const XrSpace GetAppSpace() { return m_xrAppSpace; }
		const XrSpace GetHmdSpace() { return m_xrHmdSpace; }
		const void GetHmdPose( XrPosef &outPose );
		const bool IsSessionRunning() { return m_xrSessionState < XR_SESSION_STATE_VISIBLE; }

		const ELogLevel GetMinLogLevel() { return m_pInstance->GetMinLogLevel(); }

	  private:
		CInstance *m_pInstance = nullptr;
		CVulkan *m_pVulkan = nullptr;

		XrSession m_xrSession = XR_NULL_HANDLE;
		XrSessionState m_xrSessionState = XR_SESSION_STATE_UNKNOWN;
		XrSpace m_xrAppSpace = XR_NULL_HANDLE;
		
		XrSpace m_xrHmdSpace = XR_NULL_HANDLE;
		XrSpaceLocation m_xrHmdLocation { XR_TYPE_SPACE_LOCATION };

	};
}