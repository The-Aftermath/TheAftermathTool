#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "json.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"
#include "assimp/GltfMaterial.h"

#include <Windows.h>

#include <iostream>
#include <string>
#include <cstdint>
#include <limits>
#include <filesystem>
#include <vector>
#include <fstream>

#pragma comment(lib,"assimp-vc143-mt.lib")

struct Vertex {
	float Position[3];
	float Normal[3];
	float TexCoords[2];
};

struct Texture {
	std::string FileName;
	float BaseColorFactor[3];
	float MetallicRoughnessFactor[2];
	float NormalFactor[3];
	float AOFactor;
};

struct Mesh {
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices;

	Texture BaseColor;
	Texture MetallicRoughness;
	Texture Normal;
	Texture AO;
};

void processMesh(std::vector<Mesh>& mMesh, aiMesh* mesh, const aiScene* scene) {
	Mesh my_mesh;
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex{};
		vertex.Position[0] = mesh->mVertices[i].x;
		vertex.Position[1] = mesh->mVertices[i].y;
		vertex.Position[2] = mesh->mVertices[i].z;
		vertex.Normal[0] = mesh->mNormals[i].x;
		vertex.Normal[1] = mesh->mNormals[i].y;
		vertex.Normal[2] = mesh->mNormals[i].z;

		if (!mesh->mTextureCoords[0]) {
			vertex.TexCoords[0] = 0.f;
			vertex.TexCoords[1] = 0.f;
		}
		else {
			vertex.TexCoords[0] = mesh->mTextureCoords[0][i].x;
			vertex.TexCoords[1] = mesh->mTextureCoords[0][i].y;
		}
		my_mesh.Vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			my_mesh.Indices.push_back(face.mIndices[j]);
		}
	}

	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

	if (aiString baseColorTexture; material->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &baseColorTexture) == aiReturn_SUCCESS) {
		my_mesh.BaseColor.FileName = baseColorTexture.C_Str();
	}
	else if (aiColor3D color; material->Get(AI_MATKEY_BASE_COLOR, color) == aiReturn_SUCCESS) {
		my_mesh.BaseColor.BaseColorFactor[0] = color.r;
		my_mesh.BaseColor.BaseColorFactor[1] = color.g;
		my_mesh.BaseColor.BaseColorFactor[2] = color.b;
	}
	else {
		my_mesh.BaseColor.BaseColorFactor[0] = 1.f;
		my_mesh.BaseColor.BaseColorFactor[1] = 1.f;
		my_mesh.BaseColor.BaseColorFactor[2] = 1.f;
	}

	if (aiString mrTexture; material->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &mrTexture) == aiReturn_SUCCESS) {
		my_mesh.MetallicRoughness.FileName = mrTexture.C_Str();
	}
	else {
		my_mesh.MetallicRoughness.MetallicRoughnessFactor[0] = 1.f;
		my_mesh.MetallicRoughness.MetallicRoughnessFactor[1] = 1.f;
		if (float metallic; material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
			my_mesh.MetallicRoughness.MetallicRoughnessFactor[0] = metallic;
		}
		if (float roughness; material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
			my_mesh.MetallicRoughness.MetallicRoughnessFactor[1] = roughness;
		}
	}

	if (aiString normalTexture; material->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == aiReturn_SUCCESS) {
		my_mesh.Normal.FileName = normalTexture.C_Str();
	}
	else {
		my_mesh.Normal.NormalFactor[0] = 0.5f;
		my_mesh.Normal.NormalFactor[1] = 0.5f;
		my_mesh.Normal.NormalFactor[2] = 1.f;
	}

	if (aiString aoTexture; material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &aoTexture) == aiReturn_SUCCESS) {
		my_mesh.AO.FileName = aoTexture.C_Str();
	}
	else {
		my_mesh.AO.AOFactor = 1.f;
	}

	mMesh.push_back(my_mesh);
}

void processNode(std::vector<Mesh>& mMesh, aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mMesh, mesh, scene);
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(mMesh, node->mChildren[i], scene);
	}
}

inline uint8_t float_to_int_color(const double color) {
	constexpr double MAXCOLOR = 256.0 - std::numeric_limits<double>::epsilon() * 128;
	return static_cast<uint8_t>(color * MAXCOLOR);
}

void Bake(std::filesystem::path& path, std::vector<Mesh>& mMesh) {
	auto modelStem = path.stem();
	auto modelStemStr = modelStem.string();

	CreateDirectoryA(modelStemStr.c_str(), nullptr);

	auto modelBinPath = modelStem / modelStem;
	modelBinPath.concat(".bin");
	std::ofstream modelBinFstream(modelBinPath);

	nlohmann::json modelJson;
	modelJson["MeshCount"] = std::to_string(mMesh.size());

	nlohmann::json meshAttributes;

	int offset = 0; 
	for (size_t i = 0; i < mMesh.size(); ++i) {
		nlohmann::json meshData;
		meshData["VertexCount"] = std::to_string(mMesh[i].Vertices.size());
		meshData["VertexOffset"] = std::to_string(offset);
		auto vertexDataSize = mMesh[i].Vertices.size() * sizeof(float) * 8;
		offset += vertexDataSize;
		modelBinFstream.write((const char*)mMesh[i].Vertices.data(), vertexDataSize);

		meshData["IndexCount"] = std::to_string(mMesh[i].Indices.size());
		meshData["IndexOffset"] = std::to_string(offset);
		auto indexDataSize = mMesh[i].Indices.size() * sizeof(uint32_t);
		offset += indexDataSize;
		modelBinFstream.write((const char*)mMesh[i].Indices.data(), indexDataSize);

		if (mMesh[i].BaseColor.FileName.empty()) {
			uint8_t* picData = new uint8_t[16 * 16 * 3]{};
			for (int x = 0; x < 16; ++x) {
				for (int y = 0; y < 16; ++y) {
					picData[x * 48 + y * 3 + 0] = float_to_int_color(mMesh[i].BaseColor.BaseColorFactor[0]);
					picData[x * 48 + y * 3 + 1] = float_to_int_color(mMesh[i].BaseColor.BaseColorFactor[1]);
					picData[x * 48 + y * 3 + 2] = float_to_int_color(mMesh[i].BaseColor.BaseColorFactor[2]);
				}
			}

			auto meshBaseColorPathStr = "Mesh" + std::to_string(i) + "BaseColor.png";
			auto texturePath = modelStem / meshBaseColorPathStr;
			auto texturePathStr = texturePath.string();
			stbi_write_png(texturePathStr.c_str(), 16, 16, 3, picData, 0);
			delete[]picData;

			meshData["BaseColorTexture"] = meshBaseColorPathStr;
		}
		else {
			auto texturePath = path.parent_path();
			texturePath /= mMesh[i].BaseColor.FileName;
			std::filesystem::copy(texturePath, modelStem, std::filesystem::copy_options::skip_existing);

			auto jsonFilename = texturePath.filename();
			auto jsonFilenameStr = jsonFilename.string();
			meshData["BaseColorTexture"] = jsonFilenameStr;
		}

		if (mMesh[i].MetallicRoughness.FileName.empty()) {
			uint8_t* picData = new uint8_t[16 * 16 * 3]{};
			for (int x = 0; x < 16; ++x) {
				for (int y = 0; y < 16; ++y) {
					picData[x * 48 + y * 3 + 0] = float_to_int_color(mMesh[i].MetallicRoughness.MetallicRoughnessFactor[0]);
					picData[x * 48 + y * 3 + 1] = float_to_int_color(mMesh[i].MetallicRoughness.MetallicRoughnessFactor[1]);
					picData[x * 48 + y * 3 + 2] = 0;
				}
			}

			auto meshMetallicRoughnessPathStr = "Mesh" + std::to_string(i) + "MetallicRoughness.png";
			auto texturePath = modelStem / meshMetallicRoughnessPathStr;
			auto texturePathStr = texturePath.string();
			stbi_write_png(texturePathStr.c_str(), 16, 16, 3, picData, 0);
			delete[]picData;

			meshData["MetallicRoughnessTexture"] = meshMetallicRoughnessPathStr;
		}
		else {
			auto texturePath = path.parent_path();
			texturePath /= mMesh[i].MetallicRoughness.FileName;
			std::filesystem::copy(texturePath, modelStem, std::filesystem::copy_options::skip_existing);

			auto jsonFilename = texturePath.filename();
			auto jsonFilenameStr = jsonFilename.string();
			meshData["MetallicRoughnessTexture"] = jsonFilenameStr;
		}

		if (mMesh[i].Normal.FileName.empty()) {
			uint8_t* picData = new uint8_t[16 * 16 * 3]{};
			for (int x = 0; x < 16; ++x) {
				for (int y = 0; y < 16; ++y) {
					picData[x * 48 + y * 3 + 0] = float_to_int_color(mMesh[i].Normal.NormalFactor[0]);
					picData[x * 48 + y * 3 + 1] = float_to_int_color(mMesh[i].Normal.NormalFactor[1]);
					picData[x * 48 + y * 3 + 2] = float_to_int_color(mMesh[i].Normal.NormalFactor[2]);
				}
			}

			auto meshNormalPathStr = "Mesh" + std::to_string(i) + "Normal.png";
			auto texturePath = modelStem / meshNormalPathStr;
			auto texturePathStr = texturePath.string();
			stbi_write_png(texturePathStr.c_str(), 16, 16, 3, picData, 0);
			delete[]picData;

			meshData["NormalTexture"] = meshNormalPathStr;
		}
		else {
			auto texturePath = path.parent_path();
			texturePath /= mMesh[i].Normal.FileName;
			std::filesystem::copy(texturePath, modelStem, std::filesystem::copy_options::skip_existing);

			auto jsonFilename = texturePath.filename();
			auto jsonFilenameStr = jsonFilename.string();
			meshData["NormalTexture"] = jsonFilenameStr;
		}

		if (mMesh[i].AO.FileName.empty()) {
			uint8_t* picData = new uint8_t[16 * 16 * 3]{};
			for (int x = 0; x < 16; ++x) {
				for (int y = 0; y < 16; ++y) {
					picData[x * 48 + y * 3 + 0] = float_to_int_color(mMesh[i].AO.AOFactor);
					picData[x * 48 + y * 3 + 1] = 255;
					picData[x * 48 + y * 3 + 2] = 255;
				}
			}

			auto meshAOStr = "Mesh" + std::to_string(i) + "AO.png";
			auto texturePath = modelStem / meshAOStr;
			auto texturePathStr = texturePath.string();
			stbi_write_png(texturePathStr.c_str(), 16, 16, 3, picData, 0);
			delete[]picData;

			meshData["AOTexture"] = meshAOStr;
		}
		else {
			auto texturePath = path.parent_path();
			texturePath /= mMesh[i].AO.FileName;
			std::filesystem::copy(texturePath, modelStem, std::filesystem::copy_options::skip_existing);

			auto jsonFilename = texturePath.filename();
			auto jsonFilenameStr = jsonFilename.string();
			meshData["AOTexture"] = jsonFilenameStr;
		}

		meshAttributes[i] = meshData;
	}
	modelJson["MeshAttributes"] = meshAttributes;

	auto modelJsonPath = modelStem / modelStem;
	modelJsonPath.concat(".json");
	std::ofstream modelJsonFstream(modelJsonPath);
	modelJsonFstream << modelJson;
}

int main(int argc, char* argv[])
{
	Assimp::Importer importer;
	const aiScene* scene = nullptr;
	if (argc >= 2) {
		scene = importer.ReadFile(argv[1], aiProcess_Triangulate | aiProcess_GenNormals);
	}
	else {
		std::cout << "Need file name." << std::endl;
		return 0;
	}
	std::vector<Mesh> mMesh;
	scene = importer.ReadFile(argv[1], aiProcess_Triangulate | aiProcess_GenNormals);
	if (scene) {
		processNode(mMesh, scene->mRootNode, scene);
	}

	std::filesystem::path path{ argv[1] };
	Bake(path, mMesh);
	return 0;
}

