#include "application/AEntityContext.hpp"

#include <pugixml.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <glm/gtc/quaternion.hpp>

AEntityContext::AEntityContext() {

}

AEntityContext::~AEntityContext() {
	ClearEntities();
}

bool AEntityContext::IsRDRPath(const std::string& path) const {
	return path.find(".rsc") != std::string::npos || 
	       path.find("RDR") != std::string::npos ||
	       path.find("rdr") != std::string::npos;
}

std::shared_ptr<CEntity> AEntityContext::ParseEntity(const std::string& filePath, const pugi::xml_node& itemNode, bool isRDR2) {
	auto entity = std::make_shared<CEntity>();
	entity->IsRDR2 = isRDR2;
	
	// Parse position
	auto posNode = itemNode.child("position");
	if (posNode) {
		entity->Position = glm::vec3(
			posNode.attribute("x").as_float(0.0f),
			posNode.attribute("y").as_float(0.0f),
			posNode.attribute("z").as_float(0.0f)
		);
	}
	
	// Parse rotation (quaternion XYZW -> convert to glm::quat(w,x,y,z))
	auto rotNode = itemNode.child("rotation");
	if (rotNode) {
		float x = rotNode.attribute("x").as_float(0.0f);
		float y = rotNode.attribute("y").as_float(0.0f);
		float z = rotNode.attribute("z").as_float(0.0f);
		float w = rotNode.attribute("w").as_float(1.0f);
		entity->Rotation = glm::quat(w, x, y, z);  // glm::quat(w, x, y, z)
	}
	
	// Parse archetype name
	auto archetypeNode = itemNode.child("archetypeName");
	if (archetypeNode) {
		entity->ArchetypeName = archetypeNode.text().as_string("Unknown");
	}
	
	// Parse GUID/ID (different attribute name for RDR2 vs GTA5)
	auto guidNode = isRDR2 ? itemNode.child("id") : itemNode.child("guid");
	if (guidNode) {
		entity->GUID = guidNode.text().as_uint(0);
	}
	
	// Parse scaling
	auto scaleXyNode = itemNode.child("scaleXY");
	if (scaleXyNode) {
		entity->ScaleXY = scaleXyNode.text().as_float(1.0f);
	}
	auto scaleZNode = itemNode.child("scaleZ");
	if (scaleZNode) {
		entity->ScaleZ = scaleZNode.text().as_float(1.0f);
	}
	
	// Parse common optional attributes
	auto flagsNode = itemNode.child("flags");
	if (flagsNode) {
		entity->Flags = flagsNode.text().as_uint(0);
	}
	
	auto parentIndexNode = itemNode.child("parentIndex");
	if (parentIndexNode) {
		entity->ParentIndex = parentIndexNode.text().as_int(-1);
	}
	
	auto lodDistNode = itemNode.child("lodDist");
	if (lodDistNode) {
		entity->LodDist = lodDistNode.text().as_float(0.0f);
	}
	
	auto childLodDistNode = itemNode.child("childLodDist");
	if (childLodDistNode) {
		entity->ChildLodDist = childLodDistNode.text().as_float(0.0f);
	}
	
	auto lodLevelNode = itemNode.child("lodLevel");
	if (lodLevelNode) {
		entity->LodLevel = lodLevelNode.text().as_string();
	}
	
	auto priorityLevelNode = itemNode.child("priorityLevel");
	if (priorityLevelNode) {
		entity->PriorityLevel = priorityLevelNode.text().as_string();
	}
	
	auto numChildrenNode = itemNode.child("numChildren");
	if (numChildrenNode) {
		entity->NumChildren = numChildrenNode.text().as_uint(0);
	}
	
	auto tintValueNode = itemNode.child("tintValue");
	if (tintValueNode) {
		entity->TintValue = tintValueNode.text().as_uint(0);
	}
	
	// RDR2-specific attributes
	if (isRDR2) {
		auto blendAgeLayerNode = itemNode.child("blendAgeLayer");
		if (blendAgeLayerNode) {
			entity->BlendAgeLayer = blendAgeLayerNode.text().as_float(0.0f);
		}
		
		auto blendAgeDirtNode = itemNode.child("blendAgeDirt");
		if (blendAgeDirtNode) {
			entity->BlendAgeDirt = blendAgeDirtNode.text().as_float(0.0f);
		}
	} else {
		// GTA5-specific attributes
		auto aaoMultiplierNode = itemNode.child("ambientOcclusionMultiplier");
		if (aaoMultiplierNode) {
			entity->AmbientOcclusionMultiplier = aaoMultiplierNode.text().as_uint(0);
		}
		
		auto aaaoNode = itemNode.child("artificialAmbientOcclusion");
		if (aaaoNode) {
			entity->ArtificialAmbientOcclusion = aaaoNode.text().as_uint(0);
		}
	}
	
	return entity;
}

bool AEntityContext::LoadYmap(std::filesystem::path filePath) {
	mLastLoadError.clear();
	mLastLoadInfo.clear();

	try {
		// Only accept .ymap.xml files
		if (filePath.extension() != ".xml" || filePath.stem().extension() != ".ymap") {
			mLastLoadError = "File must be .ymap.xml format (got: " + filePath.filename().string() + ")";
			return false;
		}

		if (!std::filesystem::exists(filePath)) {
			mLastLoadError = "File not found: " + filePath.string();
			return false;
		}

		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(filePath.c_str());

		if (!result) {
			mLastLoadError = "Failed to parse XML: " + std::string(result.description());
			return false;
		}

		// Find root <CMapData> element
		auto mapData = doc.child("CMapData");
		if (!mapData) {
			mLastLoadError = "No CMapData root element found";
			return false;
		}

		// Find entities container
		auto entities = mapData.child("entities");
		if (!entities) {
			mLastLoadInfo = "No entities found in ymap";
			return true;  // Valid but empty
		}

		bool isRDR2 = IsRDRPath(filePath.string());
		int entityCount = 0;

		// Iterate through all entity items
		for (auto item : entities.children("Item")) {
			auto entity = ParseEntity(filePath.string(), item, isRDR2);
			if (entity) {
				mEntities.push_back(entity);
				entityCount++;
				
				if (isRDR2) {
					mRDR2EntityCount++;
				} else {
					mGTA5EntityCount++;
				}
			}
		}

		mLastLoadInfo = "Loaded " + std::to_string(entityCount) + " entities from '" + filePath.filename().string() + "'" + 
		                (isRDR2 ? " (RDR2)" : " (GTA5)");
		return true;

	} catch (const std::exception& ex) {
		mLastLoadError = "Exception while loading ymap: " + std::string(ex.what());
		return false;
	} catch (...) {
		mLastLoadError = "Unknown exception while loading ymap";
		return false;
	}
}

void AEntityContext::ClearEntities() {
	mEntities.clear();
	mRDR2EntityCount = 0;
	mGTA5EntityCount = 0;
	mLastLoadInfo = "Cleared entities";
}
