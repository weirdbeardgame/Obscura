#include "sgdMdl.h"
#include "game/packfile.h"
#include "tim2.h"
#include "utils/utility.h"
#include "game/Exporter.h"
#include "assimp/scene.h"
#include "assimp/Exporter.hpp"
#include "assimp/postprocess.h"
#include "utils/assimp_utils.h"
#include <vector>

std::vector<Texture *> textures;
SGDFILEHEADER *sgdTop;
SGDFILEHEADER *sgdCurr;
SGDPROCUNITHEADER *s_ppuhVUVN;
SGDCOORDINATEDESC *sgdCoordintate;
SGDVUMATERIALDESC *sgdMaterial;
aiScene *scene = new aiScene();
std::vector<aiMesh*> aiMeshes;
std::vector<aiMaterial*> aiMaterials;
std::vector<aiTexture*> aiTextures;

void DisplayFF2Model(const char *filename) {
    auto pakFile = (PK2_HEAD *) ReadFullFile(filename);
    auto mdlPak = (PK2_HEAD *) GetFileInPak(pakFile, 0);
    auto texturePak = (PK2_HEAD *) GetFileInPak(pakFile, 1);
    sgdTop = (SGDFILEHEADER *) GetFileInPak(mdlPak, 0);

    scene->mRootNode = new aiNode();

    if (pakFile->pack_num != 1) {
        if (mdlPak->pack_num == SGD_VALID_VERSIONID) {
            texturePak = (PK2_HEAD *) Pk2GetAddr(pakFile, 1);
        }

        for (auto i = 0; i < texturePak->pack_num; i++) {
            auto tim2 = (TIM2_FILEHEADER *) GetFileInPak(texturePak, i);

            if (tim2 == nullptr || ((int64_t) tim2 & 0xf) != 0 || !Tim2CheckFileHeader(tim2)) {
                programLogger->critical("Found broken model with invalid textures");
                continue;
            }

            auto ph = Tim2GetPictureHeader(tim2, 0);

            auto convertedTim2 = LoadTim2Texture(tim2);
            auto img = CreateTextureFromRawData(convertedTim2->Width, convertedTim2->Height, convertedTim2->image, ph->GsTex0.TBP0);
            textures.emplace_back(img);
        }
    }

    if (mdlPak->pack_num == SGD_VALID_VERSIONID) {
        sgdTop = (SGDFILEHEADER *) mdlPak;
        sgdCurr = (SGDFILEHEADER *) mdlPak;
        sgdRemap(sgdCurr);
        HandleProcUnit(sgdCurr);
    }
    else {
        for (auto i = 0; i < mdlPak->pack_num; i++) {
            sgdCurr = (SGDFILEHEADER *) GetFileInPak(mdlPak, i);
            sgdRemap(sgdCurr);
            HandleProcUnit(sgdCurr);
        }
    }

    auto file = std::filesystem::path(filename);

    scene->mRootNode->mMeshes = new unsigned int[aiMeshes.size()];
    scene->mRootNode->mNumMeshes = aiMeshes.size();

    for (auto i = 0; i < aiMeshes.size(); i++)
    {
        scene->mRootNode->mMeshes[i] = i;
    }

    scene->mNumMeshes = aiMeshes.size();
    scene->mMeshes = aiMeshes.data();
    scene->mNumMaterials = aiMaterials.size();
    scene->mMaterials = aiMaterials.data();
    scene->mNumTextures = aiTextures.size();
    scene->mTextures = aiTextures.data();

    Assimp::Exporter exporter;
    auto result = exporter.Export(scene, "obj", (std::filesystem::current_path() / file.filename().replace_extension(".obj").filename()).string(),
                                  aiProcess_ValidateDataStructure | aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes);

    if (result != aiReturn_SUCCESS)
    {
        programLogger->error("Failed to export the scene: {}", exporter.GetErrorString());
    }
    else
    {
        programLogger->info("Successfully exported the scene");
    }
}

void HandleProcUnit(SGDFILEHEADER *sgd) {
    for (int i = 0; i < sgd->uiNumBlock; i++) {
        PrintBlockBeginning(i);

        SGDPROCUNITHEADER *pHead = GetTopProcUnitHeaderPtr(sgd, i);

        if (pHead == (SGDPROCUNITHEADER *) nullptr) {
            PrintEmptyBlock();
        }

        while (pHead != (SGDPROCUNITHEADER *) nullptr && ((int64_t) pHead & 0xf) == 0) {
            PrintBlockInfo(pHead, sgd);

            SgSortUnitPrim(pHead);

            pHead = GetNextProcUnitHeaderPtr(pHead);
        }

        PrintBlockEnding(i);
    }

    sgdCoordintate = nullptr;
}

void SgSortUnitPrim(SGDPROCUNITHEADER *pHead) {
    switch (pHead->iCategory) {
        case VUVN:
            HandleVUVNDataBlock(pHead);
            break;
        case MESH:
            HandleMeshDataBlock(pHead);
            break;
        case MATERIAL:
            HandleMaterialDataBlock(pHead);
            break;
        case COORDINATE:
            HandleCoordinateDataBlock(pHead);
            break;
        case BOUNDING_BOX:
            HandleBoundingBoxDataBlock(pHead);
            break;
        case GS_IMAGE:
            HandleGsImageDataBlock(pHead);
            break;
        case TRI2:
            HandleTri2DataBlock(pHead);
            break;
        default:
            return;
    }
}

void HandleVUVNDataBlock(SGDPROCUNITHEADER *pHead) {
    s_ppuhVUVN = pHead;
}

void HandleMeshDataBlock(SGDPROCUNITHEADER *pHead) {
    SGDVUMESHPOINTNUM *pMeshInfo;

    if (pHead->VUMeshDesc.ucMeshType == 0x80) {
        // This just adds a triangle below the character
        pMeshInfo = (SGDVUMESHPOINTNUM *) &pHead[2];
        return;
    }
    else {
        pMeshInfo = (SGDVUMESHPOINTNUM *) &pHead[4];
    }

    auto pMaterial = GetMaterialPtr(sgdCurr, sgdMaterial->iMaterialIndex);
    auto materialName = GetMaterialStrName(pMaterial);
    auto pProcData = (SGDPROCUNITDATA *) &pHead[1];

    SGDVUMESHSTDATA *sgdMeshData;

    if (pHead->VUMeshDesc.ucMeshType == iMT_2F || pHead->VUMeshDesc.ucMeshType == iMT_2) {
        sgdMeshData = RelOffsetToPtr<SGDVUMESHSTDATA>(pProcData, (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);
    }
    else {
        /// For a mesh order is SGDVUMESHPOINTNUM -> SGDVUMESHSTREGSET -> SGDVUMESHSTDATA
        auto sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[pHead->VUMeshDesc.ucNumMesh];
        sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    }

    auto offsetVertex = 0;
    auto matIndex = -1;

    aiMaterial* currentMaterial = FindMaterial(aiMaterials, materialName, &matIndex);

    if (currentMaterial == nullptr)
    {
        auto mesh_tex_reg = RelOffsetToPtr<sceGsTex0>(pProcData, 0x18);
        matIndex = aiMaterials.size();
        Texture* t = nullptr;

        for (auto v : textures)
        {
            if (v->GetAddress() == mesh_tex_reg->TBP0)
            {
                t = v;
                break;
            }
        }

        if (t == nullptr && sgdMaterial->iMaterialIndex < textures.size())
        {
            t = textures[sgdMaterial->iMaterialIndex];
        }

        currentMaterial = CreateNewMaterial(materialName, t, pMaterial);

        aiMaterials.push_back(currentMaterial);
    }

    auto pVMCD = (_SGDVUMESHCOLORDATA *) (&pHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    for (auto i = 0; i < pHead->VUMeshDesc.ucNumMesh; i++) {
        if (pHead->VUMeshDesc.ucMeshType == iMT_2F || pHead->VUMeshDesc.ucMeshType == iMT_2) {
            pVMCD = (_SGDVUMESHCOLORDATA *) GetNextUnpackAddr((uint *) pVMCD);
        }

        unsigned int numPoint;

        if (pHead->VUMeshDesc.ucMeshType == iMT_2F || pHead->VUMeshDesc.ucMeshType == iMT_2) {
            numPoint = pVMCD->VifUnpack.NUM;
        }
        else {
            numPoint = pMeshInfo[i].uiPointNum;
        }

        auto aiMesh = CreateNewMesh(numPoint, matIndex);

        if (pHead->VUMeshDesc.ucMeshType == iMT_2F || pHead->VUMeshDesc.ucMeshType == iMT_2) {
            aiMesh->mColors[0] = new aiColor4D[numPoint];
        }

        for (auto currPointIndex = 0; currPointIndex < numPoint; currPointIndex++) {
            Vector3 v{};
            Vector3 n{};

            /// MeshType == IMT_2
            if (pHead->VUMeshDesc.ucMeshType == iMT_2) {
                HandleNVLMesh(offsetVertex + currPointIndex, v, n);
                auto color = pVMCD->avColor[currPointIndex];
                aiMesh->mColors[0][currPointIndex] = {color.x / 128.0f, color.y / 128.0f, color.z / 128.0f, 0.0f};
            }
            /// VectorType == 0x5
            else if (pHead->VUMeshDesc.MeshType.NVL == true) {
                HandleNVLMesh(offsetVertex + currPointIndex, v, n);
            }
            /// VectorType == 0x6
            else if (pHead->VUMeshDesc.MeshType.VTYPE == SVA_WEIGHTED) {
                HandleWeightedMesh(offsetVertex + currPointIndex, v, n);
                aiMesh->mNumBones = 1;
                aiMesh->mBones = new aiBone*[1];
                aiMesh->mBones[0] = new aiBone();
                aiMesh->mBones[0]->mName = aiString(materialName);
                aiMesh->mBones[0]->mNumWeights = 1;
                aiMesh->mBones[0]->mWeights = new aiVertexWeight[1];
                aiMesh->mBones[0]->mWeights[0].mVertexId = currPointIndex;
                aiMesh->mBones[0]->mWeights[0].mWeight = 1.0f;
                const auto coord = GetCurrentCoordinate();
                aiMesh->mBones[0]->mOffsetMatrix = *(aiMatrix4x4*)&coord->matLocalWorld;
            }
            /// MeshType == IMT_2F
            else if (pHead->VUMeshDesc.MeshType.FLAT == 1 && pHead->VUMeshDesc.MeshType.PRE == 1) {
                HandleFlatMesh(offsetVertex + currPointIndex, v, n);
                auto color = pVMCD->avColor[currPointIndex];
                aiMesh->mColors[0][currPointIndex] = {color.x / 255.0f, color.y / 255.0f, color.z / 255.0f, 0.0f};
            }
            /// VectorType == 0x6
            else if (pHead->VUMeshDesc.MeshType.VTYPE == SVA_UNIQUE) {
                HandleUniqueMesh(offsetVertex + currPointIndex, v, n);
            }

            aiMesh->mVertices[currPointIndex] = {v.x, v.y, v.z};
            aiMesh->mNormals[currPointIndex] = {n.x, n.y, n.z};

            if (pHead->VUMeshDesc.MeshType.TEX == true) {
                if (((Vector2i *) (sgdMeshData->astData))[currPointIndex].y == 0x1) {
                    sgdMeshData->astData[currPointIndex].fT = sgdMeshData->astData[currPointIndex - 2].fT;
                }

                /// Need to substract to compensate for the png loader flipping the Y-axis
                aiMesh->mTextureCoords[0][currPointIndex] = {sgdMeshData->astData[currPointIndex].fS, 1.0f - sgdMeshData->astData[currPointIndex].fT, 0.0f};
            }

            if (currPointIndex >= numPoint - 2) {
                continue;
            }

            aiMesh->mFaces[currPointIndex].mNumIndices = 3;
            aiMesh->mFaces[currPointIndex].mIndices = new unsigned int[3];

            if (currPointIndex % 2 == 0) {
                aiMesh->mFaces[currPointIndex].mIndices[0] = currPointIndex;
                aiMesh->mFaces[currPointIndex].mIndices[1] = currPointIndex + 1;
                aiMesh->mFaces[currPointIndex].mIndices[2] = currPointIndex + 2;
            }
            else {
                aiMesh->mFaces[currPointIndex].mIndices[0] = currPointIndex + 1;
                aiMesh->mFaces[currPointIndex].mIndices[1] = currPointIndex;
                aiMesh->mFaces[currPointIndex].mIndices[2] = currPointIndex + 2;
            }
        }

        sgdMeshData = (SGDVUMESHSTDATA*) &sgdMeshData->astData[numPoint];

        if (pHead->VUMeshDesc.ucMeshType == iMT_2F || pHead->VUMeshDesc.ucMeshType == iMT_2) {
            pVMCD = (_SGDVUMESHCOLORDATA *) &pVMCD->avColor[pVMCD->VifUnpack.NUM];
        }

        offsetVertex += numPoint;
        aiMeshes.push_back(aiMesh);
    }
}

void HandleMaterialDataBlock(SGDPROCUNITHEADER *pHead) {
    sgdMaterial = &pHead->VUMaterialDesc;
}

void HandleCoordinateDataBlock(SGDPROCUNITHEADER *pHead) {
    sgdCoordintate = &pHead->CoordDesc;
}

void HandleBoundingBoxDataBlock(SGDPROCUNITHEADER *pHead) {
    /*
      auto* v4 = (Vector4*)((int)pHead + 0x10);

      int numPts = (int)(((int)pHead->pNext - (int)v4) / 16);

      // Creates the bounding boxes
      for (int i = 0; i < numPts; i++)
      {
          Vector3 v3 = Vector3{v4[i].x, v4[i].y, v4[i].z};

          //points.push_back(v3);
      }
       */
}

void HandleGsImageDataBlock(SGDPROCUNITHEADER *pHead) {
    auto sgdGsImage = RelOffsetToPtr<SGDGSIMAGEDESC>(&pHead[1], pHead->TexDesc.iPaddingSize);
}

void HandleTri2DataBlock(SGDPROCUNITHEADER *pHead) {
    auto pTRI2HeadTop = RelOffsetToPtr<SGDTRI2FILEHEADER>(&pHead[1], pHead->TexDesc.iPaddingSize);
    auto rTexDesc = &pHead->TexDesc;

    for (auto i = 0; i < rTexDesc->iNumTexture; i++) {
        int image_h = pTRI2HeadTop->gsli.trxreg.RRH;
        int image_w = pTRI2HeadTop->gsli.trxreg.RRW;
        auto data_size = image_w * image_h;
        auto numColors = 0;
        auto clutType = NO_CLUT;
        auto clutColorType = RGBA16;
        auto image_color_index_off = 0;

        switch(pTRI2HeadTop->gsli.bitbltbuf.DPSM)
        {
            case PSMT4:
                clutType = IDTEX4;
                clutColorType = RGBA32;
                numColors = 16;
                image_color_index_off = data_size >> 1;
                break;
            case PSMT8H:
            case PSMT8:
                clutType = IDTEX8;
                clutColorType = RGBA32;
                numColors = 256;
                image_color_index_off = data_size;
                break;
            case PSMCT32:
                clutType = NO_CLUT;
                clutColorType = RGBA32;
                numColors = 0;
                image_color_index_off = 0;
                image_h = image_w;
                data_size = image_w * image_w;
                break;
        }

        auto image_color_index = RelOffsetToPtr<uint8_t>(&pTRI2HeadTop[1], 0);
        auto image_color_data = RelOffsetToPtr<uint8_t>(&image_color_index[image_color_index_off], sizeof(sceGsLoadImage));
        auto image_data = new std::vector<unsigned int>(data_size);

        for(auto x = 0; x < image_w; x++)
        {
            for(auto y = 0; y < image_h; y++)
            {
                if (pTRI2HeadTop->gsli.bitbltbuf.DPSM == PSMCT32)
                {
                    auto image_offset = x + y * image_w;
                    unsigned char r, g, b, a;
                    r = image_color_index[image_offset * 4 + 0];
                    g = image_color_index[image_offset * 4 + 1];
                    b = image_color_index[image_offset * 4 + 2];
                    a = image_color_index[image_offset * 4 + 3];

                    image_data->data()[image_offset] = (unsigned int)((a << 24) | (b << 16) | (g << 8) | r);
                }
                else
                {
                    auto image_offset = x + y * image_w;
                    auto index = Tim2GetTexel(image_color_index, x, y, image_w, clutType);
                    image_data->data()[image_offset] = Tim2GetClutColor(image_color_data, clutType, clutColorType, numColors, 0, index);
                }
            }
        }

        textures.emplace_back(CreateTextureFromRawData(image_w, image_h, image_data->data(), pTRI2HeadTop->gsli.bitbltbuf.DBP));

        pTRI2HeadTop = RelOffsetToPtr<SGDTRI2FILEHEADER>(&pTRI2HeadTop->gsli,
                                                         pTRI2HeadTop->uiVif1Code_DIRECT.size * 0x10);
    }
}

void HandleFlatMesh(int meshIndex, Vector3 &vertex, Vector3 &normal) {
    const auto coord = GetCurrentCoordinate();
    auto rVUVNData = (SGDPROCUNITDATA *) &s_ppuhVUVN[1];
    auto vV = (Vector3 *) (rVUVNData->VUVNData_Preset.aui + (meshIndex + s_ppuhVUVN->VUVNDesc.sNumNormal) * 3 + 10);

    vertex = vV[0];
    normal = rVUVNData->VUVNData_Preset.vt2f.avNormal[meshIndex];
    vertex = Vector3Transform(
            vertex, MatrixTranspose(coord[sgdCoordintate->iCoordId0].matCoord));
}

void HandleNVLMesh(int meshIndex, Vector3 &vertex, Vector3 &normal) {
    const auto coord = GetCurrentCoordinate();

    auto pVUVNData = (SGDPROCUNITDATA *) &s_ppuhVUVN[1];

    vertex = pVUVNData->VUVNData_Preset.avt2[meshIndex].vVertex;
    normal = pVUVNData->VUVNData_Preset.avt2[meshIndex].vNormal;

    vertex = Vector3Transform(
            vertex, MatrixTranspose(coord[sgdCoordintate->iCoordId0].matCoord));
}

void HandleWeightedMesh(int meshIndex, Vector3 &vertex, Vector3 &normal) {
    auto pVUVNDataWeighted3 = (_SGDVUVNDATA_WEIGHTED_3 *) &s_ppuhVUVN[3];
    const auto coord = GetCurrentCoordinate();
    const auto pVectorInfo = GetVectorInfoPtr(sgdCurr);

    const auto pWeightedVertex3 = RelOffsetToPtr<_SGDVUVNDATA_WEIGHTEDVERTEX_3>(
            sgdCurr, pVectorInfo->aAddress[SVA_WEIGHTED].pvVertex);
    const auto pWeightedNormal3 = RelOffsetToPtr<Vector4>(
            sgdCurr, pVectorInfo->aAddress[SVA_WEIGHTED].pvNormal);

    auto wVertex = pWeightedVertex3[pVUVNDataWeighted3[meshIndex]
            .pWeightedVertex];

    auto v0 = Vector3Transform(
            {wVertex.vVertex.x, wVertex.vVertex.y, wVertex.vVertex.z},
            MatrixTranspose(coord[wVertex.ucBoneId0].matCoord));

    auto v1 = Vector3Transform(
            wVertex.aui, MatrixTranspose(coord[wVertex.ucBoneId1].matCoord));

    auto w1 = 255 - wVertex.vVertex.w;

    vertex = (v0 * (wVertex.vVertex.w / 255)) + (v1 * (w1 / 255));

    auto n = pWeightedNormal3[pVUVNDataWeighted3[meshIndex].pNormal];

    normal = {n.x, n.y, n.z};
}

void HandleUniqueMesh(int meshIndex, Vector3 &vertex, Vector3 &normal) {
    auto pVUVNDataWeighted3 = (_SGDVUVNDATA_WEIGHTED_3 *) &s_ppuhVUVN[3];
    const auto coord = GetCurrentCoordinate();
    const auto pVectorInfo = GetVectorInfoPtr(sgdCurr);

    const auto pSVAUniqueVertex = RelOffsetToPtr<Vector4>(
            sgdCurr, pVectorInfo->aAddress[SVA_UNIQUE].pvVertex);

    const auto pSVAUniqueNormal = RelOffsetToPtr<Vector4>(
            sgdCurr, pVectorInfo->aAddress[SVA_UNIQUE].pvNormal);

    auto wVertex = pSVAUniqueVertex[pVUVNDataWeighted3[meshIndex]
            .pWeightedVertex];

    vertex = Vector3Transform(
            {wVertex.x, wVertex.y, wVertex.z},
            MatrixTranspose(coord[sgdCoordintate->iCoordId0].matCoord));

    auto n = pSVAUniqueNormal[pVUVNDataWeighted3[meshIndex].pNormal];

    normal = {n.x, n.y, n.z};
}

SGDCOORDINATE *GetCurrentCoordinate() {
    return GetCoordinatePtr(sgdCurr) == nullptr ? GetCoordinatePtr(sgdTop)
                                                : GetCoordinatePtr(sgdCurr);
}
