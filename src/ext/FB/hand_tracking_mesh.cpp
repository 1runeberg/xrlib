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

#include <xrlib/ext/FB/hand_tracking_mesh.hpp>
#include <xrlib/common.hpp>

namespace xrlib::FB
{
	CHandTrackingMesh::CHandTrackingMesh( XrInstance xrInstance ) :
		CExtBase( xrInstance, XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME )
	{
		INIT_PFN( xrInstance, xrGetHandMeshFB );
	}

	XrResult CHandTrackingMesh::GetMesh( XrHandTrackerEXT handTracker, SMesh &outMesh )
	{
		if ( !xrGetHandMeshFB )
			return XR_ERROR_FUNCTION_UNSUPPORTED;

		// Get mesh counts
		XrHandTrackingMeshFB xrMesh { XR_TYPE_HAND_TRACKING_MESH_FB };
		XrResult xrResult = xrGetHandMeshFB( handTracker, &xrMesh );

		if ( XR_FAILED( xrResult ) )
			return xrResult;

		outMesh.jointBindPoses.resize( xrMesh.jointCountOutput );
		outMesh.jointRadii.resize( xrMesh.jointCountOutput );
		outMesh.jointParents.resize( xrMesh.jointCountOutput );

		outMesh.vertexPositions.resize( xrMesh.vertexCountOutput );
		outMesh.vertexNormals.resize( xrMesh.vertexCountOutput );
		outMesh.vertexUVs.resize( xrMesh.vertexCountOutput );
		outMesh.vertexBlendIndices.resize( xrMesh.vertexCountOutput );
		outMesh.vertexBlendWeights.resize( xrMesh.vertexCountOutput );

		outMesh.indices.resize( xrMesh.indexCountOutput );

		xrMesh.jointCapacityInput = xrMesh.jointCountOutput;
		xrMesh.jointBindPoses = outMesh.jointBindPoses.data();
		xrMesh.jointRadii = outMesh.jointRadii.data();
		xrMesh.jointParents = outMesh.jointParents.data();

		xrMesh.vertexCapacityInput = xrMesh.vertexCountOutput;
		xrMesh.vertexPositions = outMesh.vertexPositions.data();
		xrMesh.vertexNormals = outMesh.vertexNormals.data();
		xrMesh.vertexUVs = outMesh.vertexUVs.data();
		xrMesh.vertexBlendIndices = outMesh.vertexBlendIndices.data();
		xrMesh.vertexBlendWeights = outMesh.vertexBlendWeights.data();

		xrMesh.indexCapacityInput = xrMesh.indexCountOutput;
		xrMesh.indices = outMesh.indices.data();

		return xrGetHandMeshFB( handTracker, &xrMesh );
	}
}
