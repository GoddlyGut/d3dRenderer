#pragma once
#include "Mesh.h"
#include "DXSample.h"
#include "Time.h"
#include "Structs.h"
#include "Light.h"
#include "Camera.h"
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
	virtual void OnUpdateCameraZoomDelta(float zoomDelta);
	virtual void OnUpdateCameraPosition(int deltaX, int deltaY);

private:
	float distance;
	float m_camYaw, m_camPitch;



	static const UINT FrameCount = 2;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

	UINT indexCount;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_rtvHandle;


	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandAllocator> commandAllocator;

	// constant buffers
	ComPtr<ID3D12Resource> pixelUniformBuffer;
	

	ComPtr<ID3D12Resource> lightsUniformBuffer;

	Camera camera;


	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	Time time;


	float m_rotationAngleX, m_rotationAngleY, m_rotationAngleZ;


	std::vector<Mesh> meshes;
	std::vector<Light> lights;

	void SetupMeshResources(Mesh& mesh);
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);
	void SetupLightProperties(Light& light);
	void SetupConstantBuffers();

	void UpdateCamera();


	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList(Mesh& mesh, XMMATRIX viewMatrix, XMMATRIX projectionMatrix);
	void WaitForPreviousFrame();
	

	void SetupFence();
	void SetupShaders();
	void SetupRootSignature();
	void SetupVertexBuffer();
};

