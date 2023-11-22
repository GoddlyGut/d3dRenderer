#pragma once

#include "DXSample.h"
#include "Time.h"

#include <iostream>


using namespace DirectX;

using Microsoft::WRL::ComPtr;


class Renderer : public DXSample
{
public:
	Renderer(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

private:
	static const UINT FrameCount = 2;

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


	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_constantBuffer;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

	UINT indexCount;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	
	ComPtr<ID3D12Resource> m_textureBuffer;
	ComPtr< ID3D12DescriptorHeap> m_mainDescriptorHeap;
	ComPtr<ID3D12Resource> m_textureBufferUploadHeap;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	Time time;


	float m_rotationAngleX, m_rotationAngleY, m_rotationAngleZ;

	struct SubMesh {
		UINT indexCount;
		UINT startIndexLocation;
		INT baseVertexLocation;
	};

	std::vector<SubMesh> subMeshes;
	std::vector<FaceIndices> allFaces;
	std::vector<Vertex> allVertices;


	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();

	void SetupFence();
	void SetupShaders();
	void SetupRootSignature();
	void SetupVertexBuffer();
};

