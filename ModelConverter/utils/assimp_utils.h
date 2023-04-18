#ifndef OBSCURA_ASSIMP_UTILS_H
#define OBSCURA_ASSIMP_UTILS_H

#include <vector>
#include "assimp/mesh.h"
#include "assimp/material.h"
#include "game/Texture.h"
#include "game/sgd_types.h"
#include "assimp/scene.h"
#include <filesystem>

aiMesh* CreateNewMesh(int numPoints, int matIndex);
aiMaterial *FindMaterial(std::vector<aiMaterial *> materials, const std::string& name, int *matIndex);
aiMaterial* CreateNewMaterial(std::filesystem::path exportFolder, const std::string& name, Texture *t, SGDMATERIAL* pMaterial);
void ExportScene(std::filesystem::path exportFolder, const std::string& format, aiScene *scene, int exporterOptions);

#endif //OBSCURA_ASSIMP_UTILS_H