// Copyright 2025 DimAlek. All Rights Reserved.

#pragma once

#include "Models/TextureChannelPackerTypes.h"

namespace TextureChannelPackerTexture
{
	/** @brief Numeric precision used by scalar channel storage. */
	enum class EChannelValueType : uint8
	{
		UInt8,
		UInt16,
		Float16,
		Float32
	};

	/** @brief Dense scalar samples and their dimensions; integers use their full unsigned range. */
	struct TEXTURECHANNELPACKER_API FChannelData
	{
		int32 Width = 0;
		int32 Height = 0;
		EChannelValueType ValueType = EChannelValueType::UInt8;
		TArray<uint8> Values;
	};

	/** @brief Output component backed by source samples, a constant or an implicit default. */
	struct TEXTURECHANNELPACKER_API FResolvedOutputChannel
	{
		ETextureChannelPackerChannel OutputChannel = ETextureChannelPackerChannel::Red;
		bool bHasData = false;
		bool bUseConstant = false;
		uint8 ConstantValue = 0;
		FChannelData Data;
	};

} // namespace TextureChannelPackerTexture
