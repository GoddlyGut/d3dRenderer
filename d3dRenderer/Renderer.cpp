#include "Renderer.h"
#include "Utils.h"

#define _USE_MATH_DEFINES
#include <math.h>

Renderer::Renderer(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<float>(width), static_cast<float>(height)),
	m_rtvDescriptorSize(0),
	time(Time())
{

}

void Renderer::OnInit() {
	LoadPipeline();
	LoadAssets();
}

void Renderer::LoadPipeline() {
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif


	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice) {
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)));
	}
	else {
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));
	}

	// Describe and create the command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain));

	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources

	{
		m_rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

		for (UINT n = 0; n < FrameCount; n++) {
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, m_rtvHandle);
			m_rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}


}

void Renderer::SetupFence() {
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValue = 1;

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Renderer::SetupShaders() {
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG) 
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
	ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
	//{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	//};

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_width;  // same as back buffer width
	resourceDesc.Height = m_height;  // same as back buffer height
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // 24-bit depth & 8-bit stencil
	resourceDesc.SampleDesc.Count = 1;  // no multi-sampling
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	ID3D12Resource* depthBuffer;
	ThrowIfFailed(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&depthBuffer)));


	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

	m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();  // Assume dsvHeap is your descriptor heap for Depth-Stencil View
	m_device->CreateDepthStencilView(depthBuffer, &dsvDesc, m_dsvHandle);

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depthStencilDesc.StencilEnable = FALSE;  // set to TRUE if you're using stencil
	// ... set other stencil settings if enabled


	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;  // Also can be D3D12_FILL_MODE_WIREFRAME
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;   // Culling back faces. Other options: D3D12_CULL_MODE_FRONT, D3D12_CULL_MODE_NONE
	rasterizerDesc.FrontCounterClockwise = TRUE;     // Specifies whether triangles are front-facing if counter-clockwise
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;



	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

}

void Renderer::SetupRootSignature() {

	D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorTableRanges[0].NumDescriptors = 1;
	descriptorTableRanges[0].BaseShaderRegister = 0;
	descriptorTableRanges[0].RegisterSpace = 0;
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// create a descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); // we only have one range
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0]; // the pointer to the beginning of our ranges array

	D3D12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
	rootParameters[1].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now


	// create a static sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;



	// Create a root signature description.
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 1;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	rootSignatureDesc.Init(
		_countof(rootParameters),                              // Number of root parameters
		rootParameters,                 // Pointer to array of root parameters
		1,                              // Number of static samplers
		&sampler,                        // Pointer to array of static samplers
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}


void Renderer::SetupVertexBuffer() {

	LPCWSTR texturePath = L"C:/Users/ilyai/Documents/Visual Studio 2022/Projects/d3dRenderer/d3dRenderer/diffuse.jpg";

	Mesh mesh = Mesh(texturePath, m_device);

	meshes.push_back(mesh);


	for (const auto& mesh : meshes) {
		ID3D12CommandList* ppCommandLists[] = { mesh.commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		m_fenceValue++;
		m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
	}




}

void Renderer::LoadAssets() {

	SetupRootSignature();

	SetupShaders();

	SetupFence();

	SetupVertexBuffer();
}

void Renderer::OnUpdate()
{
	//delta time
	time.Update();

	//rotation speeds
	float rotationSpeed = 60.0f; // degrees per second
	m_rotationAngleY += rotationSpeed * time.GetDeltaTime();
}

void Renderer::OnRender() {
	m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

	for (const auto& mesh : meshes) {
		PopulateCommandList(mesh);

		ID3D12CommandList* ppCommandLists[] = { mesh.commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}



	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

void Renderer::PopulateCommandList(Mesh mesh) {



	ThrowIfFailed(mesh.commandList->Reset(mesh.commandAllocator.Get(), m_pipelineState.Get()));
	mesh.commandList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	mesh.commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mesh.mainDescriptorHeap.Get() };
	mesh.commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mesh.commandList->SetGraphicsRootDescriptorTable(1, mesh.mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	mesh.commandList->SetGraphicsRootConstantBufferView(0, mesh.constantBuffer->GetGPUVirtualAddress());
	mesh.commandList->RSSetViewports(1, &m_viewport);
	mesh.commandList->RSSetScissorRects(1, &m_scissorRect);
	mesh.commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	mesh.commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	mesh.commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	mesh.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mesh.commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
	mesh.commandList->IASetIndexBuffer(&mesh.indexBufferView);

	for (const auto& subMesh : mesh.subMeshes) {
		mesh.commandList->DrawIndexedInstanced(subMesh.indexCount, 1, subMesh.startIndexLocation, subMesh.baseVertexLocation, 0);
	}
	mesh.commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));




	//MATH
	XMMATRIX modelMatrix = XMMatrixIdentity();
	XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	modelMatrix = modelMatrix * translationMatrix;		  //pitch, yaw, roll
	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(m_rotationAngleX, m_rotationAngleY * M_PI / 180.f, m_rotationAngleZ);
	modelMatrix = modelMatrix * rotationMatrix;
	XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	modelMatrix = modelMatrix * scaleMatrix;


	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f),  // eyePosition
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),   // focalPoint
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)    // upDirection
	);


	XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
		XM_PIDIV4,           // Field of View in radians
		m_aspectRatio,         // Aspect ratio (width/height)
		0.1f,          // Near clipping plane
		1000.0f            // Far clipping plane
	);



	XMMATRIX mvpMatrix = modelMatrix * viewMatrix * projectionMatrix;


	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read this resource on the CPU.
	mesh.constantBuffer->Map(0, &readRange, &pMappedData);

	// Create the MVP matrices and fill a temporary structure.
	MVPMatrix mvpMatrices;
	mvpMatrices.model = XMMatrixTranspose(modelMatrix);
	mvpMatrices.view = XMMatrixTranspose(viewMatrix);
	mvpMatrices.projection = XMMatrixTranspose(projectionMatrix);

	// Copy the matrices to the constant buffer.
	memcpy(pMappedData, &mvpMatrices, sizeof(mvpMatrices));

	// Unmap the constant buffer.
	mesh.constantBuffer->Unmap(0, nullptr);

	ThrowIfFailed(mesh.commandList->Close());
}


void Renderer::WaitForPreviousFrame() {
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;
	//std::cout << fence << std::endl;
	//printf(char*)(m_fence->GetCompletedValue())
	if (m_fence->GetCompletedValue() < fence) {
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}