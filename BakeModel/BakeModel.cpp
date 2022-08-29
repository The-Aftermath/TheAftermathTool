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

#include <iostream>
#include <string>
#include <cstdint>

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

void processNode() {

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
		//processNode(mMesh, scene->mRootNode, scene);
	}
}

