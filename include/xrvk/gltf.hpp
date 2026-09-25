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

#include <xrlib/session.hpp>
#include <xrvk/buffer.hpp>
#include <xrvk/mesh.hpp>

#include <unordered_map>
#include <functional>

#include <fastgltf/types.hpp>

namespace xrlib
{
	static void inline ExtractEulerAngles( const XrMatrix4x4f &matrix, float &pitch, float &yaw, float &roll )
	{
		// Extract pitch (X rotation)
		pitch = asinf( -matrix.m[ 9 ] ); // -sin(pitch)

		// Check for gimbal lock
		if ( std::abs( matrix.m[ 9 ] ) > 0.9999f )
		{
			// Gimbal lock: pitch is +/- 90 degrees
			yaw = atan2f( -matrix.m[ 8 ], matrix.m[ 0 ] ); // Use yaw derived from X/Z
			roll = 0.0f;								   // Roll is undefined in gimbal lock
		}
		else
		{
			// Extract yaw (Y rotation) and roll (Z rotation)
			yaw = atan2f( matrix.m[ 8 ], matrix.m[ 10 ] ); // atan2(forward.x, forward.z)
			roll = atan2f( matrix.m[ 1 ], matrix.m[ 5 ] ); // atan2(up.x, up.y)
		}

		// Convert radians to degrees
		pitch *= 180.0f / M_PI;
		yaw *= 180.0f / M_PI;
		roll *= 180.0f / M_PI;
	}

	static inline XrQuaternionf CreateQuaternionFromEuler( float pitch, float yaw, float roll )
	{
		// Convert degrees to radians 
		const float toRadians = 3.14159265358979323846f / 180.0f;
		pitch *= toRadians;
		yaw *= toRadians;
		roll *= toRadians;

		// Calculate half angles
		const float halfPitch = pitch * 0.5f;
		const float halfYaw = yaw * 0.5f;
		const float halfRoll = roll * 0.5f;

		// Calculate sine and cosine for half angles
		const float sinPitch = sinf( halfPitch );
		const float cosPitch = cosf( halfPitch );
		const float sinYaw = sinf( halfYaw );
		const float cosYaw = cosf( halfYaw );
		const float sinRoll = sinf( halfRoll );
		const float cosRoll = cosf( halfRoll );

		// Compute quaternion components
		XrQuaternionf quaternion;
		quaternion.x = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
		quaternion.y = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
		quaternion.z = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
		quaternion.w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

		return quaternion;
	}

	/// <summary>Decoded image data retained between disk loading and GPU upload</summary>
	struct SGltfImage
	{
		std::string name;
		std::string uri;
		int width = 0;
		int height = 0;
		int component = 4;
		int bits = 8;
		std::vector< uint8_t > image;
		std::vector< vkutils::SImageMip > mips; // Prepared mip ranges; empty for PNG/JPEG
	};

	/// <summary>CPU wall-clock durations in milliseconds for completed loading stages</summary>
	struct SGltfLoadTimings
	{
		double diskReadMs = 0;		 // Main glTF/GLB file read; filesystem cache may satisfy the read
		double parseMs = 0;			 // glTF parsing, validation and external buffer reads
		double imageDecodeMs = 0;	 // Image decoding or prepared KTX2 reads and validation, including external image reads
		double textureUploadMs = 0;	 // Texture creation and upload calls, including their waits
		double meshConversionMs = 0; // Materials, skins and mesh conversion on the CPU
	};

	/// <summary>Owns parsed glTF data and decoded images for the two-stage loading API</summary>
	struct SGltfModel
	{
		fastgltf::Asset asset;
		SGltfLoadTimings timings;
		std::vector< SGltfImage > images;
	};

	/// <summary>Loading execution options; decoded pixels and texture settings are unchanged</summary>
	struct SGltfLoadOptions
	{
		uint32_t imageDecodeWorkers = 4; // Maximum concurrent image readers/decoders; 1 selects serial decoding
		bool batchTextureUploads = true; // Submit texture transfers together instead of one image at a time
		std::string textureDirectory;	 // Prepared KTX2 directory for this asset (filesystem or Android assets); requires one image-N.ktx2 per glTF image
	};

	class CGltf
	{
	  public:
		CGltf( CSession *pSession );

		/// <summary>Creates a loader with explicit execution options for profiling or constrained devices</summary>
		CGltf( CSession *pSession, SGltfLoadOptions options );
		~CGltf();

		bool LoadAndParse( CRenderModel *outRenderModel, VkCommandPool commandPool, const std::string &sFilename, XrVector3f scale = { 1.f, 1.f, 1.f } );

		/// <summary>Loads glTF data and decodes images without uploading to the GPU</summary>
		/// <param name="outRenderModel">Render model whose instance scales will be set after loading</param>
		/// <param name="outModel">Receives owned data on success; unchanged on failure</param>
		/// <param name="sFilename">Path to a glTF/GLB file, or an Android asset path</param>
		/// <param name="scale">Scale applied to each render model instance</param>
		/// <returns>True if the model and its images loaded successfully</returns>
		bool LoadFromDisk( CRenderModel *outRenderModel, SGltfModel *outModel, const std::string &sFilename, XrVector3f scale = { 1.f, 1.f, 1.f } );

		/// <summary>Converts a loaded model and uploads its textures using the current Vulkan session</summary>
		/// <param name="outRenderModel">Receives mesh, material, texture and skin data</param>
		/// <param name="pModel">Successfully loaded data; must remain alive until this call returns</param>
		/// <param name="commandPool">Command pool used for texture uploads</param>
		void ParseModel( CRenderModel *outRenderModel, SGltfModel *pModel, VkCommandPool commandPool );

	  private:
		CSession *m_pSession = nullptr;
		SGltfLoadOptions m_options;

		// Helper functions to process gltf data
		void ProcessNode( 
			const fastgltf::Asset &model,
			const fastgltf::Node &node,
			std::vector< SMeshVertex > &vertices, 
			std::vector< uint32_t > &indices, 
			std::vector< SMeshSection > &materialSections );

		void ProcessMesh( 
			const fastgltf::Asset &model,
			const fastgltf::Mesh &mesh,
			std::vector< SMeshVertex > &vertices, 
			std::vector< uint32_t > &indices, 
			std::vector< SMeshSection > &materialSections );

		void ParseTextures( CRenderModel *outRenderModel, VkCommandPool commandPool, const SGltfModel &model );
		void ParseTexture( STexture *outTexture, VkCommandPool commandPool, const SGltfModel &model, const fastgltf::Texture &gltfTexture );
		void IdentifyTextureTypes( std::vector< STexture > &textures, const fastgltf::Asset &model );

		void ParseMaterials( CRenderModel *outRenderModel, const fastgltf::Asset &model );
		void ParseMaterial( SMaterial *outMaterial, const fastgltf::Asset &model, const fastgltf::Material &gltfMaterial );

		void ParseSkins( CRenderModel *outRenderModel, const fastgltf::Asset &model );
		void ParseSkin( SSkin *outSkin, const fastgltf::Asset &model, const fastgltf::Skin &gltfSkin );

		VkFilter ConvertMagFilter( int gltfFilter );
		VkFilter ConvertMinFilter( int gltfFilter );
		VkSamplerAddressMode ConvertWrappingMode( int gltfWrap );

		void NormalizeRotation( XrMatrix4x4f &rotationMatrix )
		{
			// Step 1: Normalize each column of the 3x3 rotation matrix manually
			float col0Length = sqrt( rotationMatrix.m[ 0 ] * rotationMatrix.m[ 0 ] + rotationMatrix.m[ 1 ] * rotationMatrix.m[ 1 ] + rotationMatrix.m[ 2 ] * rotationMatrix.m[ 2 ] );
			float col1Length = sqrt( rotationMatrix.m[ 4 ] * rotationMatrix.m[ 4 ] + rotationMatrix.m[ 5 ] * rotationMatrix.m[ 5 ] + rotationMatrix.m[ 6 ] * rotationMatrix.m[ 6 ] );
			float col2Length = sqrt( rotationMatrix.m[ 8 ] * rotationMatrix.m[ 8 ] + rotationMatrix.m[ 9 ] * rotationMatrix.m[ 9 ] + rotationMatrix.m[ 10 ] * rotationMatrix.m[ 10 ] );

			// Normalize the columns (if length is non-zero)
			if ( col0Length != 0.0f )
			{
				rotationMatrix.m[ 0 ] /= col0Length;
				rotationMatrix.m[ 1 ] /= col0Length;
				rotationMatrix.m[ 2 ] /= col0Length;
			}

			if ( col1Length != 0.0f )
			{
				rotationMatrix.m[ 4 ] /= col1Length;
				rotationMatrix.m[ 5 ] /= col1Length;
				rotationMatrix.m[ 6 ] /= col1Length;
			}

			if ( col2Length != 0.0f )
			{
				rotationMatrix.m[ 8 ] /= col2Length;
				rotationMatrix.m[ 9 ] /= col2Length;
				rotationMatrix.m[ 10 ] /= col2Length;
			}

			// Step 2: Gram-Schmidt process to make the matrix orthogonal
			// Orthogonalize the second column (col1) against the first column (col0)
			float dot0_1 = rotationMatrix.m[ 0 ] * rotationMatrix.m[ 4 ] + rotationMatrix.m[ 1 ] * rotationMatrix.m[ 5 ] + rotationMatrix.m[ 2 ] * rotationMatrix.m[ 6 ];
			rotationMatrix.m[ 4 ] -= dot0_1 * rotationMatrix.m[ 0 ];
			rotationMatrix.m[ 5 ] -= dot0_1 * rotationMatrix.m[ 1 ];
			rotationMatrix.m[ 6 ] -= dot0_1 * rotationMatrix.m[ 2 ];

			// Normalize the second column again
			float col1LengthAfterGram = sqrt( rotationMatrix.m[ 4 ] * rotationMatrix.m[ 4 ] + rotationMatrix.m[ 5 ] * rotationMatrix.m[ 5 ] + rotationMatrix.m[ 6 ] * rotationMatrix.m[ 6 ] );
			if ( col1LengthAfterGram != 0.0f )
			{
				rotationMatrix.m[ 4 ] /= col1LengthAfterGram;
				rotationMatrix.m[ 5 ] /= col1LengthAfterGram;
				rotationMatrix.m[ 6 ] /= col1LengthAfterGram;
			}

			// Orthogonalize the third column (col2) against the first column (col0) and second column (col1)
			float dot0_2 = rotationMatrix.m[ 0 ] * rotationMatrix.m[ 8 ] + rotationMatrix.m[ 1 ] * rotationMatrix.m[ 9 ] + rotationMatrix.m[ 2 ] * rotationMatrix.m[ 10 ];
			rotationMatrix.m[ 8 ] -= dot0_2 * rotationMatrix.m[ 0 ];
			rotationMatrix.m[ 9 ] -= dot0_2 * rotationMatrix.m[ 1 ];
			rotationMatrix.m[ 10 ] -= dot0_2 * rotationMatrix.m[ 2 ];

			float dot1_2 = rotationMatrix.m[ 4 ] * rotationMatrix.m[ 8 ] + rotationMatrix.m[ 5 ] * rotationMatrix.m[ 9 ] + rotationMatrix.m[ 6 ] * rotationMatrix.m[ 10 ];
			rotationMatrix.m[ 8 ] -= dot1_2 * rotationMatrix.m[ 4 ];
			rotationMatrix.m[ 9 ] -= dot1_2 * rotationMatrix.m[ 5 ];
			rotationMatrix.m[ 10 ] -= dot1_2 * rotationMatrix.m[ 6 ];

			// Normalize the third column again
			float col2LengthAfterGram = sqrt( rotationMatrix.m[ 8 ] * rotationMatrix.m[ 8 ] + rotationMatrix.m[ 9 ] * rotationMatrix.m[ 9 ] + rotationMatrix.m[ 10 ] * rotationMatrix.m[ 10 ] );
			if ( col2LengthAfterGram != 0.0f )
			{
				rotationMatrix.m[ 8 ] /= col2LengthAfterGram;
				rotationMatrix.m[ 9 ] /= col2LengthAfterGram;
				rotationMatrix.m[ 10 ] /= col2LengthAfterGram;
			}
		}

		void NormalizeMatrix( XrMatrix4x4f &matrix )
		{
			// Extract rotation part (top-left 3x3 matrix) and translation part (last column)
			XrMatrix4x4f rotationMatrix = { matrix.m[ 0 ], matrix.m[ 1 ], matrix.m[ 2 ], 0.0f, matrix.m[ 4 ], matrix.m[ 5 ], matrix.m[ 6 ], 0.0f, matrix.m[ 8 ], matrix.m[ 9 ], matrix.m[ 10 ], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

			// Normalize the rotation part by creating an orthonormal matrix
			// Use Gram-Schmidt process or another method to preserve orthogonality
			NormalizeRotation( rotationMatrix );

			// Rebuild the matrix with the normalized rotation and original translation
			matrix.m[ 0 ] = rotationMatrix.m[ 0 ];
			matrix.m[ 1 ] = rotationMatrix.m[ 1 ];
			matrix.m[ 2 ] = rotationMatrix.m[ 2 ];

			matrix.m[ 4 ] = rotationMatrix.m[ 4 ];
			matrix.m[ 5 ] = rotationMatrix.m[ 5 ];
			matrix.m[ 6 ] = rotationMatrix.m[ 6 ];

			matrix.m[ 8 ] = rotationMatrix.m[ 8 ];
			matrix.m[ 9 ] = rotationMatrix.m[ 9 ];
			matrix.m[ 10 ] = rotationMatrix.m[ 10 ];

			// Translation part stays unchanged
			matrix.m[ 3 ] /= 100;	// Translation x
			matrix.m[ 7 ] /= 100;	// Translation y
			matrix.m[ 11 ] /= 100; // Translation z
		}

	};
} // namespace xrlib
