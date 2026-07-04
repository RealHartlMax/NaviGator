#pragma once

#include "types.h"
#include "application/ACamera.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <memory>

namespace pugi {
	class xml_node;
}

struct CEntity {
	std::string ArchetypeName;
	glm::vec3 Position = glm::vec3(0.0f);
	glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Quaternion (w, x, y, z)
	uint32_t GUID = 0;
	bool Visible = true;
	
	// RDR2-specific attributes
	float BlendAgeLayer = 0.0f;
	float BlendAgeDirt = 0.0f;
	
	// GTA5-specific attributes
	uint32_t AmbientOcclusionMultiplier = 0;
	uint32_t ArtificialAmbientOcclusion = 0;
	
	// Common attributes
	uint32_t Flags = 0;
	int32_t ParentIndex = -1;
	float LodDist = 0.0f;
	float ChildLodDist = 0.0f;
	std::string LodLevel;  // e.g., \"SOLLUMZ_LOW\"
	std::string PriorityLevel;  // e.g., \"SOLLUMZ_DONT_LOAD\"
	uint32_t NumChildren = 0;
	float ScaleXY = 1.0f;
	float ScaleZ = 1.0f;
	uint32_t TintValue = 0;
	
	bool IsRDR2 = false;  // Track which game this entity is from
};

class AEntityContext {
	std::vector<std::shared_ptr<CEntity>> mEntities;
	std::string mLastLoadError;
	std::string mLastLoadInfo;
	uint32_t mRDR2EntityCount = 0;
	uint32_t mGTA5EntityCount = 0;

	// Helper methods
	bool IsRDRPath(const std::string& path) const;
	std::shared_ptr<CEntity> ParseEntity(const std::string& filePath, const pugi::xml_node& itemNode, bool isRDR2);

public:
	AEntityContext();
	~AEntityContext();

	bool LoadYmap(std::filesystem::path filePath);
	void ClearEntities();
	
	size_t GetLoadedEntityCount() const { return mEntities.size(); }
	uint32_t GetRDR2EntityCount() const { return mRDR2EntityCount; }
	uint32_t GetGTA5EntityCount() const { return mGTA5EntityCount; }
	const std::string& GetLastLoadError() const { return mLastLoadError; }
	const std::string& GetLastLoadInfo() const { return mLastLoadInfo; }
	
	const std::vector<std::shared_ptr<CEntity>>& GetEntities() const { return mEntities; }
};
