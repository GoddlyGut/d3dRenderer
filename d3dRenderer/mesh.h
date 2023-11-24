

#pragma once
#include "DXSampleHelper.h"
#include "Utils.h";
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class Mesh
{
public:
	struct Vertex {
		XMFLOAT3 position;
		XMFLOAT3 color;
		XMFLOAT2 texCoord;
	};

	struct FaceIndices {
		unsigned int a, b, c;
	};

	struct MVPMatrix
	{
		XMMATRIX model;
		XMMATRIX view;
		XMMATRIX projection;
	};


	struct SubMesh {
		UINT indexCount;
		UINT startIndexLocation;
		INT baseVertexLocation;
	};


	Mesh(LPCWSTR texturePath, ComPtr<ID3D12Device> device);

	std::vector<Vertex> vertices;
	std::vector<FaceIndices> indices;
	//BYTE* texture;
	LPCWSTR texturePath;
	std::vector<SubMesh> subMeshes;
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	ComPtr<ID3D12Resource> constantBuffer;
	ComPtr< ID3D12DescriptorHeap> mainDescriptorHeap;
	ComPtr<ID3D12Resource> textureBuffer;
	ComPtr<ID3D12Resource> textureBufferUploadHeap;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12GraphicsCommandList> commandList;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;
	ComPtr<ID3D12CommandAllocator> commandAllocator;

private:
	void setupMesh();
};