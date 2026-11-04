// Copyright 2025 DimAlek. All Rights Reserved.

#include "UI/TextureChannelPackerUIConversions.h"

FString FTextureChannelPackerUIConversions::OperationToString(const ETextureChannelPackerOperation Operation)
{
	switch (Operation)
	{
		case ETextureChannelPackerOperation::Pack:
		{
			return TEXT("Pack");
		}
		case ETextureChannelPackerOperation::Repack:
		{
			return TEXT("Repack");
		}
		case ETextureChannelPackerOperation::Unpack:
		{
			return TEXT("Unpack");
		}
		case ETextureChannelPackerOperation::Copy:
		{
			return TEXT("Copy Channels");
		}
		default:
		{
			return TEXT("Pack");
		}
	}
}

ETextureChannelPackerOperation FTextureChannelPackerUIConversions::StringToOperation(const FString& OperationName)
{
	if (OperationName == TEXT("Repack"))
	{
		return ETextureChannelPackerOperation::Repack;
	}
	if (OperationName == TEXT("Unpack"))
	{
		return ETextureChannelPackerOperation::Unpack;
	}
	if (OperationName == TEXT("Copy Channels"))
	{
		return ETextureChannelPackerOperation::Copy;
	}

	return ETextureChannelPackerOperation::Pack;
}

FString FTextureChannelPackerUIConversions::ChannelToString(const ETextureChannelPackerChannel Channel)
{
	switch (Channel)
	{
		case ETextureChannelPackerChannel::Red:
		{
			return TEXT("R");
		}
		case ETextureChannelPackerChannel::Green:
		{
			return TEXT("G");
		}
		case ETextureChannelPackerChannel::Blue:
		{
			return TEXT("B");
		}
		case ETextureChannelPackerChannel::Alpha:
		{
			return TEXT("A");
		}
		default:
		{
			return TEXT("R");
		}
	}
}

ETextureChannelPackerChannel FTextureChannelPackerUIConversions::StringToChannel(const FString& ChannelName)
{
	if (ChannelName == TEXT("G"))
	{
		return ETextureChannelPackerChannel::Green;
	}
	if (ChannelName == TEXT("B"))
	{
		return ETextureChannelPackerChannel::Blue;
	}
	if (ChannelName == TEXT("A"))
	{
		return ETextureChannelPackerChannel::Alpha;
	}

	return ETextureChannelPackerChannel::Red;
}

FString FTextureChannelPackerUIConversions::CompressionToString(const TextureCompressionSettings Compression)
{
	switch (Compression)
	{
		case TC_Masks:
		{
			return TEXT("Masks");
		}
		case TC_Grayscale:
		{
			return TEXT("Grayscale");
		}
		case TC_HDR:
		{
			return TEXT("HDR");
		}
		case TC_VectorDisplacementmap:
		{
			return TEXT("Vector Displacement");
		}
		case TC_Default:
		default:
		{
			return TEXT("Default");
		}
	}
}

TextureCompressionSettings FTextureChannelPackerUIConversions::StringToCompression(const FString& Compression)
{
	if (Compression == TEXT("Masks"))
	{
		return TC_Masks;
	}
	if (Compression == TEXT("Grayscale"))
	{
		return TC_Grayscale;
	}
	if (Compression == TEXT("HDR"))
	{
		return TC_HDR;
	}
	if (Compression == TEXT("Vector Displacement"))
	{
		return TC_VectorDisplacementmap;
	}

	return TC_Default;
}

FString FTextureChannelPackerUIConversions::MipToString(const TextureMipGenSettings Mip)
{
	switch (Mip)
	{
		case TMGS_NoMipmaps:
		{
			return TEXT("No Mipmaps");
		}
		case TMGS_Sharpen0:
		{
			return TEXT("Sharpen 0");
		}
		case TMGS_Sharpen4:
		{
			return TEXT("Sharpen 4");
		}
		case TMGS_FromTextureGroup:
		default:
		{
			return TEXT("From Texture Group");
		}
	}
}

TextureMipGenSettings FTextureChannelPackerUIConversions::StringToMip(const FString& Mip)
{
	if (Mip == TEXT("No Mipmaps"))
	{
		return TMGS_NoMipmaps;
	}
	if (Mip == TEXT("Sharpen 0"))
	{
		return TMGS_Sharpen0;
	}
	if (Mip == TEXT("Sharpen 4"))
	{
		return TMGS_Sharpen4;
	}

	return TMGS_FromTextureGroup;
}

FString FTextureChannelPackerUIConversions::FilterToString(const TextureFilter Filter)
{
	switch (Filter)
	{
		case TF_Nearest:
		{
			return TEXT("Nearest");
		}
		case TF_Bilinear:
		{
			return TEXT("Bilinear");
		}
		case TF_Trilinear:
		{
			return TEXT("Trilinear");
		}
		case TF_Default:
		default:
		{
			return TEXT("Default");
		}
	}
}

TextureFilter FTextureChannelPackerUIConversions::StringToFilter(const FString& Filter)
{
	if (Filter == TEXT("Nearest"))
	{
		return TF_Nearest;
	}
	if (Filter == TEXT("Bilinear"))
	{
		return TF_Bilinear;
	}
	if (Filter == TEXT("Trilinear"))
	{
		return TF_Trilinear;
	}

	return TF_Default;
}

FString FTextureChannelPackerUIConversions::LODGroupToString(const TextureGroup LODGroup)
{
	switch (LODGroup)
	{
		case TEXTUREGROUP_UI:
		{
			return TEXT("UI");
		}
		case TEXTUREGROUP_Pixels2D:
		{
			return TEXT("Pixels 2D");
		}
		case TEXTUREGROUP_WorldNormalMap:
		{
			return TEXT("World Normal Map");
		}
		case TEXTUREGROUP_WorldSpecular:
		{
			return TEXT("World Specular");
		}
		case TEXTUREGROUP_World:
		default:
		{
			return TEXT("World");
		}
	}
}

TextureGroup FTextureChannelPackerUIConversions::StringToLODGroup(const FString& LODGroup)
{
	if (LODGroup == TEXT("UI"))
	{
		return TEXTUREGROUP_UI;
	}
	if (LODGroup == TEXT("Pixels 2D"))
	{
		return TEXTUREGROUP_Pixels2D;
	}
	if (LODGroup == TEXT("World Normal Map"))
	{
		return TEXTUREGROUP_WorldNormalMap;
	}
	if (LODGroup == TEXT("World Specular"))
	{
		return TEXTUREGROUP_WorldSpecular;
	}

	return TEXTUREGROUP_World;
}
