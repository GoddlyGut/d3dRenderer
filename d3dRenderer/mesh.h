

#pragma once
#include "DXSampleHelper.h"
#include "Utils.h";
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Structs.h"
#include "Node.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Mesh: public Node
{
public:
	Mesh(std::string meshPath, LPCWSTR texturePath, ComPtr<ID3D12Device>& device);

	std::vector<Vertex> vertices;
	std::vector<FaceIndices> indices;
	
	std::string meshPath;
	LPCWSTR texturePath;
	std::vector<SubMesh> subMeshes;
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	ComPtr<ID3D12Resource> constantBuffer;
	ComPtr< ID3D12DescriptorHeap> mainDescriptorHeap;
	ComPtr<ID3D12Resource> textureBuffer;
	ComPtr<ID3D12Resource> textureBufferUploadHeap;
	ComPtr<ID3D12Device> device;
	
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	
	ComPtr<ID3D12Resource> vertexBufferUploadHeap;
	ComPtr<ID3D12Resource> indexBufferUploadHeap;
	D3D12_SUBRESOURCE_DATA vertexData = {};
	D3D12_SUBRESOURCE_DATA indexData = {};
	D3D12_SUBRESOURCE_DATA textureData = {};

private:
	D3D12_RESOURCE_DESC textureDesc;

	void SetupMesh();
	void LoadMeshData();
	void ProcessScene(const aiScene* scene);
	void CreateBuffers();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void LoadTextureResources();
	void SetupTextureShader();
	void FinalizeSetup();
};