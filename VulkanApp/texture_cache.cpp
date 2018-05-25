#include "texture_cache.h"

VulkanTextureCache::VulkanTextureCache(VulkanDevices* devices)
{
	loaded_texture_size_ = 0;
	devices_ = devices;
}

void VulkanTextureCache::Cleanup()
{
	for (Texture* texture : textures_)
	{
		texture->Cleanup();
		delete texture;
		texture = nullptr;
	}
	textures_.clear();
}

Texture* VulkanTextureCache::LoadTexture(std::string texture_filename)
{
	for (Texture* texture : textures_)
	{
		if (texture->GetTextureName() == texture_filename)
		{
			texture->IncrementUsageCount();
			return texture;
		}
	}

	Texture* texture = new Texture();
	texture->Init(devices_, texture_filename);
	texture->IncrementUsageCount();
	textures_.push_back(texture);
	loaded_texture_size_ += texture->GetImageSize();
	return texture;
}

void VulkanTextureCache::ReleaseTexture(Texture*& texture)
{
	bool remove = false;
	auto texture_it = textures_.begin();
	for (texture_it = textures_.begin(); texture_it != textures_.end(); texture_it++)
	{
		if ((*texture_it)->GetTextureName() == texture->GetTextureName())
		{
			texture->DecrementUsageCount();
			if (texture->GetUsageCount() == 0)
			{
				texture->Cleanup();
				delete texture;
				remove = true;
				break;
			}
		}
	}

	if (remove)
	{
		textures_.erase(texture_it);
	}

	texture = nullptr;
}