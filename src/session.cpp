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


#include <xrlib/session.hpp>
#include <xrlib/vulkan.hpp>
#include <array>

namespace xrlib
{
	CSession::CSession( CInstance *pInstance ) : 
		m_pInstance( pInstance )
	{
		assert( pInstance );

		m_pVulkan = new CVulkan( this );
	}

	CSession::~CSession() 
	{
		if ( m_xrAppSpace != XR_NULL_HANDLE )
			xrDestroySpace( m_xrAppSpace );

		if ( m_xrHmdSpace != XR_NULL_HANDLE )
			xrDestroySpace( m_xrHmdSpace );

		if ( m_xrSession != XR_NULL_HANDLE )
			xrDestroySession( m_xrSession );

		if ( m_pVulkan )
			delete m_pVulkan;
	}

	XrResult CSession::Init( SSessionSettings &settings ) 
	{ 
		// Enable multiview rendering
		if ( settings.bUseMultiviewRendering )
		{
			VkPhysicalDeviceVulkan11Features vkPhysicalFeatures11 { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
			vkPhysicalFeatures11.multiview = VK_TRUE;

			if ( settings.pVkLogicalDeviceNext )
			{
				vkPhysicalFeatures11.pNext = settings.pVkLogicalDeviceNext;
				settings.pVkLogicalDeviceNext = &vkPhysicalFeatures11;
			}
			else
			{
				settings.pVkLogicalDeviceNext = &vkPhysicalFeatures11;
			}
		}

		// Init rendering
		return Init(
			settings.pSurface,
			settings.flgAdditionalCreateInfo, 
			settings.pVkInstanceNext, 
			settings.pXrVkInstanceNext, 
			settings.pVkLogicalDeviceNext, 
			settings.pXrLogicalDeviceNext
		); 
	}

	XrResult CSession::Init(
		VkSurfaceKHR *pSurface,
		XrSessionCreateFlags flgAdditionalCreateInfo,
		void *pVkInstanceNext,
		void *pXrVkInstanceNext,
		void *pVkLogicalDeviceNext,
		void *pXrLogicalDeviceNext ) 
	{ 
		XR_RETURN_ON_ERROR( InitVulkan( pSurface, pVkInstanceNext, pXrVkInstanceNext, pVkLogicalDeviceNext, pXrLogicalDeviceNext ) );
		XR_RETURN_ON_ERROR( CreateXrSession( flgAdditionalCreateInfo ) );
		XR_RETURN_ON_ERROR( CreateAppSpace( xrAppReferencePose, xrAppReferenceSpaceType ) );
		XR_RETURN_ON_ERROR( CreateHmdSpace( xrAppReferencePose ) );

		return XR_SUCCESS;
	}

	XrResult
		CSession::InitVulkan( 
			VkSurfaceKHR *pSurface, 
			void *pVkInstanceNext, 
			void *pXrVkInstanceNext, 
			void *pVkLogicalDeviceNext, 
			void *pXrLogicalDeviceNext ) 
	{ 
		XrResult xrResult = m_pVulkan->Init( pSurface, pVkInstanceNext, pXrVkInstanceNext, pVkLogicalDeviceNext, pXrLogicalDeviceNext );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to initialize Vulkan resources: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult CSession::CreateXrSession( XrSessionCreateFlags flgAdditionalCreateInfo, void *pNext ) 
	{ 
		// Check if there's a valid openxr instance
		if ( GetAppInstance()->GetXrSystemId() == XR_NULL_SYSTEM_ID || m_pVulkan->GetGraphicsBinding()->device == VK_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		m_pVulkan->GetGraphicsBinding()->next = pNext;

		XrSessionCreateInfo xrSessionCreateInfo { XR_TYPE_SESSION_CREATE_INFO };
		xrSessionCreateInfo.next = m_pVulkan->GetGraphicsBinding();
		xrSessionCreateInfo.systemId = GetAppInstance()->GetXrSystemId();
		xrSessionCreateInfo.createFlags = flgAdditionalCreateInfo;

		XrResult xrResult = xrCreateSession( m_pInstance->GetXrInstance(), &xrSessionCreateInfo, &m_xrSession );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to create openxr session: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		return XR_SUCCESS;
	}

	XrResult CSession::CreateAppSpace( XrPosef referencePose, XrReferenceSpaceType referenceSpaceType, void *pNext ) 
	{ 
		// Log session supported reference space types (debug only)
		if ( CheckLogLevelDebug( m_pInstance->GetMinLogLevel() ) )
		{
			auto vecSupportedReferenceSpaceTypes = GetSupportedReferenceSpaceTypes();

			LogDebug( XRLIB_NAME, "This session supports %i reference space type(s):", vecSupportedReferenceSpaceTypes.size() );
			for ( auto &xrReferenceSpaceType : vecSupportedReferenceSpaceTypes )
			{
				LogDebug( XRLIB_NAME, "\t%s", XrEnumToString( xrReferenceSpaceType ) );
			}
		}

		// Create reference space
		XrReferenceSpaceCreateInfo xrReferenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		xrReferenceSpaceCreateInfo.next = pNext;
		xrReferenceSpaceCreateInfo.poseInReferenceSpace = referencePose;
		xrReferenceSpaceCreateInfo.referenceSpaceType = referenceSpaceType;
		XR_RETURN_ON_ERROR( xrCreateReferenceSpace( m_xrSession, &xrReferenceSpaceCreateInfo, &m_xrAppSpace ) )

		if ( CheckLogLevelDebug( m_pInstance->GetMinLogLevel() ) )
			LogDebug( XRLIB_NAME, "Reference space for APP of type (%s) created with handle (%" PRIu64 ").", XrEnumToString( xrAppReferenceSpaceType ), (uint64_t) m_xrAppSpace );

		return XR_SUCCESS;
	}

	XrResult CSession::CreateHmdSpace( XrPosef referencePose, void *pNext ) 
	{ 
		// Create reference space
		XrReferenceSpaceCreateInfo xrReferenceSpaceCreateInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		xrReferenceSpaceCreateInfo.next = pNext;
		xrReferenceSpaceCreateInfo.poseInReferenceSpace = referencePose;
		xrReferenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		XR_RETURN_ON_ERROR( xrCreateReferenceSpace( m_xrSession, &xrReferenceSpaceCreateInfo, &m_xrHmdSpace ) )

		if ( CheckLogLevelDebug( m_pInstance->GetMinLogLevel() ) )
			LogDebug( XRLIB_NAME, "Reference space for HMD of type (%s) created with handle (%" PRIu64 ").", XrEnumToString( XR_REFERENCE_SPACE_TYPE_VIEW ), (uint64_t) m_xrHmdSpace );

		return XR_SUCCESS;
	}

	XrResult CSession::Start(  void *pOtherBeginInfo ) 
	{ 
		// Check if session was initialized correctly
		if ( m_xrSession == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Begin session
		XrSessionBeginInfo xrSessionBeginInfo { XR_TYPE_SESSION_BEGIN_INFO };
		xrSessionBeginInfo.next = pOtherBeginInfo;
		xrSessionBeginInfo.primaryViewConfigurationType = xrViewConfigurationType;

		XrResult xrResult = xrBeginSession( m_xrSession, &xrSessionBeginInfo );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to start session: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		LogInfo( XRLIB_NAME, "OpenXR session started (%" PRIu64 ") with view configuration type: %s", (uint64_t) m_xrSession, XrEnumToString( xrViewConfigurationType ) );
		return XR_SUCCESS;
	}

	XrResult CSession::Poll( XrEventDataBaseHeader *outEventData ) 
	{ 
        #ifdef XR_USE_PLATFORM_ANDROID
			// For android we need in addition, to poll android events to proceed to the session ready state
			while ( true )
			{
				// We'll wait indefinitely (-1ms) for android poll results until we get to the android app resumed state
				// and/or the openxr session has started.
				const int nTimeout = ( !m_pInstance->androidAppState.Resumed && IsSessionRunning() ) ? -1 : 0;

				int nEvents;
				struct android_poll_source *androidPollSource;
				if ( ALooper_pollOnce( nTimeout, nullptr, &nEvents, (void **) &androidPollSource ) < 0 )
				{
					break;
				}

				// Process android event
				if ( androidPollSource )
				{
					androidPollSource->process( m_pInstance->GetAndroidApp(), androidPollSource );
				}

			}
		#endif

		XrEventDataBuffer xrEventDataBuffer { XR_TYPE_EVENT_DATA_BUFFER };
		XrResult result = xrPollEvent( m_pInstance->GetXrInstance(), &xrEventDataBuffer );
		if ( !XR_SUCCEEDED( result ) )
				return result;

		// Internal event processing
		outEventData->type = xrEventDataBuffer.type;
		outEventData = reinterpret_cast< XrEventDataBaseHeader* >( &xrEventDataBuffer );

		if ( outEventData->type == XR_TYPE_EVENT_DATA_EVENTS_LOST )
		{
			// Report any lost events
			const XrEventDataEventsLost *const eventsLost = reinterpret_cast< const XrEventDataEventsLost * >( outEventData );
				LogWarning( XRLIB_NAME, "Poll events warning - there are %i events lost", eventsLost->lostEventCount );
		}
		else if ( outEventData->type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
		{
			// Update session state
			const XrEventDataSessionStateChanged xrSessionStateChangedEvent = *reinterpret_cast< const XrEventDataSessionStateChanged * >( outEventData );
				LogInfo( XRLIB_NAME, "OpenXR session state changed from %s to %s", XrEnumToString( m_xrSessionState ), XrEnumToString( xrSessionStateChangedEvent.state ) );
			m_xrSessionState = xrSessionStateChangedEvent.state;
		}

		return XR_SUCCESS;
	}

	XrResult CSession::End( bool bRequestToExit ) 
	{ 
		// Check if session was initialized correctly
		if ( m_xrSession == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		XrResult xrResult = XR_SUCCESS;
		if ( bRequestToExit )
		{
			xrResult = xrRequestExitSession( m_xrSession );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				LogError( XRLIB_NAME, "Error requesting runtime to end session: %s", XrEnumToString( xrResult ) );
				return xrResult;
			}
		}

		xrResult = xrEndSession( m_xrSession );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Unable to end session: %s", XrEnumToString( xrResult ) );
			return xrResult;
		}

		LogInfo( XRLIB_NAME, "OpenXR session ended." );
		return XR_SUCCESS; 
	}

	XrResult CSession::StartFrame( XrFrameState *pFrameState, void *pWaitFrameNext, void *pBeginFrameNext ) 
	{ 
		// @todo: debug only asserts

		XrFrameWaitInfo xrWaitFrameInfo { XR_TYPE_FRAME_WAIT_INFO, pWaitFrameNext };
		xrWaitFrameInfo.next = pWaitFrameNext;
		XR_RETURN_ON_ERROR( xrWaitFrame( m_xrSession, &xrWaitFrameInfo, pFrameState ) );
			
		XrFrameBeginInfo xrBeginFrameInfo { XR_TYPE_FRAME_BEGIN_INFO, pBeginFrameNext };
		XR_RETURN_ON_ERROR( xrBeginFrame( m_xrSession, &xrBeginFrameInfo ) );

		return XR_SUCCESS;
	}

	XrResult CSession::UpdateEyeStates( 
		std::vector< XrView > &outEyeViews,
		std::array< XrMatrix4x4f, 2 > &outEyeProjections,
		XrViewState *outEyeViewsState,
		XrFrameState *pFrameState,
		XrSpace space,
		const float fNearZ,
		const float fFarZ,
		XrViewConfigurationType viewConfigurationType,
		void *pNext ) 
	{ 
		// @todo: debug assert only as this is called per frame - views and projections count *must* match
		
		XrViewLocateInfo xrFrameSpaceTimeInfo { XR_TYPE_VIEW_LOCATE_INFO };
		xrFrameSpaceTimeInfo.next = pNext;
		xrFrameSpaceTimeInfo.displayTime = pFrameState->predictedDisplayTime;
		xrFrameSpaceTimeInfo.space = space;
		xrFrameSpaceTimeInfo.viewConfigurationType = viewConfigurationType;

		uint32_t unFoundViews;
		XR_RETURN_ON_ERROR( xrLocateViews( m_xrSession, &xrFrameSpaceTimeInfo, outEyeViewsState, (uint32_t) outEyeViews.size(), &unFoundViews, outEyeViews.data() ) );

		for ( uint32_t i = 0; i < outEyeViews.size(); i++ )
			XrMatrix4x4f_CreateProjectionFov( &outEyeProjections[ i ], GRAPHICS_VULKAN, outEyeViews[ i ].fov, fNearZ, fFarZ );

		return XR_SUCCESS;
	}

	XrResult CSession::AcquireFrameImage( uint32_t *outImageIndex, XrSwapchain swapchain, void *pNext ) 
	{ 
		if ( !pNext )
		{
			// Acquire info is optional
			XR_RETURN_ON_ERROR( xrAcquireSwapchainImage( swapchain, nullptr, outImageIndex ) );
		}
		else
		{
			XrSwapchainImageAcquireInfo acquireInfo { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, pNext };
			XR_RETURN_ON_ERROR( xrAcquireSwapchainImage( swapchain, &acquireInfo, outImageIndex ) );
		}

		return XR_SUCCESS;
	}

	XrResult CSession::AcquireFrameImage( 
		uint32_t *outColorImageIndex,
		uint32_t *outDepthImageIndex,
		XrSwapchain colorSwapchain, 
		XrSwapchain depthSwapchain,  
		void *pColorAcquireNext, 
		void *pDepthAcquireNext ) 
	{ 

		XR_RETURN_ON_ERROR( AcquireFrameImage( outColorImageIndex, colorSwapchain, pColorAcquireNext ) );
		XR_RETURN_ON_ERROR( AcquireFrameImage( outDepthImageIndex, depthSwapchain, pDepthAcquireNext ) );

		return XR_SUCCESS; 
	}

	XrResult CSession::WaitForFrameImage( 
		XrSwapchain swapchain, 
		XrDuration duration, 
		void *pNext ) 
	{ 
		XrSwapchainImageWaitInfo xrWaitInfo { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		xrWaitInfo.next = pNext;
		xrWaitInfo.timeout = duration;

		return xrWaitSwapchainImage( swapchain, &xrWaitInfo );
	}

	XrResult CSession::WaitForFrameImage( 
		XrSwapchain colorSwapchain, 
		XrSwapchain depthSwapchain, 
		XrDuration duration,
		void *pNext ) 
	{ 
		XrSwapchainImageWaitInfo xrWaitInfo { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		xrWaitInfo.next = pNext;
		xrWaitInfo.timeout = duration;

		XR_RETURN_ON_ERROR( WaitForFrameImage( colorSwapchain, duration, pNext ) );
		XR_RETURN_ON_ERROR( WaitForFrameImage( depthSwapchain, duration, pNext ) );

		return XR_SUCCESS;
	}

	XrResult CSession::ReleaseFrameImage( XrSwapchain swapchain, void *pNext ) 
	{ 
		if ( !pNext )
		{
			// release info is optional
			XR_RETURN_ON_ERROR( xrReleaseSwapchainImage( swapchain, nullptr ) );
		}
		else
		{
			XrSwapchainImageReleaseInfo releaseInfo { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, pNext };
			XR_RETURN_ON_ERROR( xrReleaseSwapchainImage( swapchain, &releaseInfo ) );
		}

		return XR_SUCCESS;
	}

	XrResult CSession::ReleaseFrameImage( XrSwapchain colorSwapchain, XrSwapchain depthSwapchain, void *pNext ) 
	{ 
		XR_RETURN_ON_ERROR( ReleaseFrameImage( colorSwapchain, pNext ) );
		XR_RETURN_ON_ERROR( ReleaseFrameImage( depthSwapchain, pNext ) );
		return XR_SUCCESS;
	}

	XrResult CSession::EndFrame( 
		XrFrameState *pFrameState, 
		std::vector< XrCompositionLayerBaseHeader * > &vecFrameLayers, 
		XrEnvironmentBlendMode blendMode, 
		void *pNext ) 
	{ 
		XrFrameEndInfo xrEndFrameInfo { XR_TYPE_FRAME_END_INFO };
		xrEndFrameInfo.next = pNext;
		xrEndFrameInfo.displayTime = pFrameState->predictedDisplayTime;
		xrEndFrameInfo.environmentBlendMode = blendMode;
		xrEndFrameInfo.layerCount = (uint32_t) vecFrameLayers.size();
		xrEndFrameInfo.layers = vecFrameLayers.data();

		return xrEndFrame( m_xrSession, &xrEndFrameInfo ); 
	}

	XrResult CSession::LocateSpace( XrSpace baseSpace, XrSpace targetSpace, XrTime predictedDisplayTime, XrSpaceLocation *outSpaceLocation ) 
	{ 
		return xrLocateSpace( targetSpace, baseSpace, predictedDisplayTime, outSpaceLocation );
	}

	XrResult CSession::UpdateHmdPose( XrTime predictedDisplayTime ) 
	{
		return LocateSpace( m_xrAppSpace, m_xrHmdSpace, predictedDisplayTime, &m_xrHmdLocation );
	}

	std::vector< XrReferenceSpaceType > CSession::GetSupportedReferenceSpaceTypes() 
	{ 
		assert( m_xrSession != XR_NULL_HANDLE );

		// Get number of reference space types supported by the runtime
		uint32_t unReferenceSpaceTypesNum = 0;

		XrResult xrResult = xrEnumerateReferenceSpaces( m_xrSession, unReferenceSpaceTypesNum, &unReferenceSpaceTypesNum, nullptr );
		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting supported reference space types from the runtime: %s", XrEnumToString( xrResult ) );
			return std::vector< XrReferenceSpaceType >();
		}

		std::vector< XrReferenceSpaceType > vecViewReferenceSpaceTypes( unReferenceSpaceTypesNum );
		xrResult = xrEnumerateReferenceSpaces( m_xrSession, unReferenceSpaceTypesNum, &unReferenceSpaceTypesNum, vecViewReferenceSpaceTypes.data() );

		if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
		{
			LogError( XRLIB_NAME, "Error getting supported reference space types from the runtime: %s", XrEnumToString( xrResult ) );
			return std::vector< XrReferenceSpaceType >();
		}

		return vecViewReferenceSpaceTypes;
	}

	XrResult CSession::GetSupportedTextureFormats( std::vector< int64_t > &outSupportedFormats ) 
	{ 
		// Check if session was initialized correctly
		if ( m_xrSession == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Clear out vector
		outSupportedFormats.clear();

		// Select a swapchain format
		uint32_t unSwapchainFormatNum = 0;
		XR_RETURN_ON_ERROR( xrEnumerateSwapchainFormats( m_xrSession, unSwapchainFormatNum, &unSwapchainFormatNum, nullptr ) );

		outSupportedFormats.resize( unSwapchainFormatNum, 0 );
		XR_RETURN_ON_ERROR( xrEnumerateSwapchainFormats( m_xrSession, unSwapchainFormatNum, &unSwapchainFormatNum, outSupportedFormats.data() ) );

		// Log supported swapchain formats
		//if ( GetMinLogLevel() == ELogLevel::LogVerbose )
		//{
		//	LogVerbose( "", "Runtime supports the following vulkan formats: " );
		//	for ( auto supportedTextureFormat : outSupportedFormats )
		//	{
		//		LogVerbose( "", "\t%i", supportedTextureFormat );
		//	}
		//}

		return XR_SUCCESS;
	}

	int64_t CSession::SelectColorTextureFormat( const std::vector< int64_t > &vecRequestedFormats ) 
	{ 
		// Check if session was initialized correctly
		if ( m_xrSession == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Retrieve this session's supported formats
		std::vector< int64_t > vecSupportedFormats;
		if ( !XR_UNQUALIFIED_SUCCESS( GetSupportedTextureFormats( vecSupportedFormats ) ) )
			return 0;

		// If there are no requested texture formats, choose the first one from the runtime
		if ( vecRequestedFormats.empty() )
		{
			// Get first format
			for ( auto textureFormat : vecSupportedFormats )
			{
				if ( !m_pVulkan->IsDepthFormat( (VkFormat) textureFormat ) )		
					return textureFormat;
			}
		}
		else
		{
			// Find matching texture format with runtime's supported ones
			for ( auto supportedFormat : vecSupportedFormats )
			{
				if ( m_pVulkan->IsDepthFormat( (VkFormat) supportedFormat ) )
					continue;

				for ( auto requestedFormat : vecRequestedFormats )
				{
					// Return selected format if a match is found
					if ( requestedFormat == supportedFormat )
						return supportedFormat;
				}
			}
		}

		return 0;
	}

	int64_t CSession::SelectDepthTextureFormat( const std::vector< int64_t > &vecRequestedFormats ) 
	{ 
		// Check if session was initialized correctly
		if ( m_xrSession == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Retrieve this session's supported formats
		std::vector< int64_t > vecSupportedFormats;
		if ( !XR_UNQUALIFIED_SUCCESS( GetSupportedTextureFormats( vecSupportedFormats ) ) )
			return 0;

		// If there are no requested texture formats, choose the first one from the runtime
		if ( vecRequestedFormats.empty() )
		{
			// Get first format
			for ( auto textureFormat : vecSupportedFormats )
			{
				if ( m_pVulkan->IsDepthFormat( (VkFormat) textureFormat ) )
					return textureFormat;
			}
		}
		else
		{
			// Find matching texture format with runtime's supported ones
			for ( auto supportedFormat : vecSupportedFormats )
			{
				if ( !m_pVulkan->IsDepthFormat( (VkFormat) supportedFormat ) )
					continue;

				for ( auto requestedFormat : vecRequestedFormats )
				{
					// Return selected format if a match is found
					if ( requestedFormat == supportedFormat )
						return supportedFormat;
				}
			}
		}

		return 0;
	}

	const void CSession::GetHmdPose( XrPosef &outPose ) 
	{ 
		if ( m_xrHmdLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT )
			outPose.position = m_xrHmdLocation.pose.position;

		if ( m_xrHmdLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT )
			outPose.orientation = m_xrHmdLocation.pose.orientation;
	}

	XrResult CSession::RequestExit()
	{
		XrResult xrResult = XR_SUCCESS;
		if ( m_xrSession != XR_NULL_HANDLE )
		{
			xrResult = xrRequestExitSession( m_xrSession );
			if ( !XR_UNQUALIFIED_SUCCESS( xrResult ) )
			{
				LogError( "CSession::RequestExit", "Error requesting runtime to end session: %s", XrEnumToString( xrResult ) );
			}
			else
			{
				LogInfo( "CSession::RequestExit", "OpenXR session exit requested." );
			}
		}
		else
		{
			LogWarning( "CSession::RequestExit", "OpenXR session is not initialized, cannot request exit." );
		}

		return xrResult;
	}
}

