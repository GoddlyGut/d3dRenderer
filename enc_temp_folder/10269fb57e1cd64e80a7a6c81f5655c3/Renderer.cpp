#include "stdafx.h"
#include "Renderer.h"
#include <iostream>

Renderer::Renderer(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<float>(width), static_cast<float>(height)),
	m_rtvDescriptorSize(0) 
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

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};


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



	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void Renderer::SetupRootSignature() {
	
	D3D12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Create a root signature description.
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumStaticSamplers = 0;
	rootSignatureDesc.pStaticSamplers = nullptr;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	rootSignatureDesc.Init(
		_countof(rootParameters),                              // Number of root parameters
		rootParameters,                 // Pointer to array of root parameters
		0,                              // Number of static samplers
		nullptr,                        // Pointer to array of static samplers
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void Renderer::SetupVertexBuffer() {
	//Vertex triangleVertices[] = {
	//	{ { -1.0f / m_aspectRatio, 1.0f / m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // 0
	//	{ { 1.0f / m_aspectRatio, 1.0f / m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },  // 1
	//	{ { -1.0f / m_aspectRatio, -1.0f / m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // 2
	//	{ { 1.0f / m_aspectRatio, -1.0f / m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }  // 5
	//};



	//uint32_t indices[] = {
	//	0, 1, 2,  // First Triangle
	//	2, 1, 3   // Second Triangle
	//};

	//Vertex cubeVertices[] =
	//{
	//	{ { -1.0f / m_aspectRatio,  1.0f / m_aspectRatio, -1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // 0
	//	{ {  1.0f / m_aspectRatio,  1.0f / m_aspectRatio, -1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // 1
	//	{ {  1.0f / m_aspectRatio, -1.0f / m_aspectRatio, -1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // 2
	//	{ { -1.0f / m_aspectRatio, -1.0f / m_aspectRatio, -1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } }, // 3
	//	{ { -1.0f / m_aspectRatio,  1.0f / m_aspectRatio,  1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } }, // 4
	//	{ {  1.0f / m_aspectRatio,  1.0f / m_aspectRatio,  1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } }, // 5
	//	{ {  1.0f / m_aspectRatio, -1.0f / m_aspectRatio,  1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } }, // 6
	//	{ { -1.0f / m_aspectRatio, -1.0f / m_aspectRatio,  1.0f }, { 0.5f, 0.5f, 0.5f, 1.0f } }  // 7
	//};

	Vertex cubeVertices[] = {
		// front face
		{ { -0.5f,  0.5f, -0.5f}, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ {0.5f, -0.5f, -0.5f}, { 1.0f, 0.0f, 1.0f, 1.0f } },
		{ {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f} },
		{ { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} },
		  //{ right side face	  }
		  { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f, 1.0f} },
		  { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f} },
		  { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f, 1.0f} },
		  { { 0.5f,  0.5f, -0.5f}, { 0.0f, 1.0f, 0.0f, 1.0f} },
		//{ left side face	  }
		{ {-0.5f,  0.5f,  0.5f},{ 1.0f, 0.0f, 0.0f, 1.0f} },
		{ {-0.5f, -0.5f, -0.5f},{ 1.0f, 0.0f, 1.0f, 1.0f} },
		{ {-0.5f, -0.5f,  0.5f},{ 0.0f, 0.0f, 1.0f, 1.0f} },
		{ {-0.5f,  0.5f, -0.5f},{ 0.0f, 1.0f, 0.0f, 1.0f} },
				
	// back face
		{ { 0.5f,  0.5f,  0.5f},{ 1.0f, 0.0f, 0.0f, 1.0f} },
		{ {-0.5f, -0.5f,  0.5f},{ 1.0f, 0.0f, 1.0f, 1.0f} },
		{ { 0.5f, -0.5f,  0.5f},{ 0.0f, 0.0f, 1.0f, 1.0f} },
		{ {-0.5f,  0.5f,  0.5f},{ 0.0f, 1.0f, 0.0f, 1.0f} },
		// top face
		{ {-0.5f,  0.5f, -0.5f},{ 1.0f, 0.0f, 0.0f, 1.0f } },
		{ {0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f, 1.0f } },
		{ {0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f } },
		{ {-0.5f,  0.5f,  0.5f},{ 0.0f, 1.0f, 0.0f, 1.0f } },
		// bottom face	
		{ { 0.5f, -0.5f,  0.5f},{ 1.0f, 0.0f, 0.0f, 1.0f} },
		{ {-0.5f, -0.5f, -0.5f},{ 1.0f, 0.0f, 1.0f, 1.0f} },
		{ { 0.5f, -0.5f, -0.5f},{ 0.0f, 0.0f, 1.0f, 1.0f} },
		{ {-0.5f, -0.5f,  0.5f},{ 0.0f, 1.0f, 0.0f, 1.0f} },
	};

	uint16_t indices[] = {
		// ffront face
		0, 1, 2, // first triangle
		0, 3, 1, // second triangle

		// left face
		4, 5, 6, // first triangle
		4, 7, 5, // second triangle

		// right face
		8, 9, 10, // first triangle
		8, 11, 9, // second triangle

		// back face
		12, 13, 14, // first triangle
		12, 15, 13, // second triangle

		// top face
		16, 17, 18, // first triangle
		16, 19, 17, // second triangle

		// bottom face
		20, 21, 22, // first triangle
		20, 23, 21, // second triangle
	};


	indexCount = sizeof(indices) / sizeof(uint16_t);

	const UINT vertexBufferSize = sizeof(cubeVertices);


	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer));

	m_vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	ID3D12Resource* vertexBufferUploadHeap;
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBufferUploadHeap)));
	vertexBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(cubeVertices); // pointer to our vertex array
	vertexData.RowPitch = vertexBufferSize; // size of all our triangle vertex data
	vertexData.SlicePitch = vertexBufferSize; // also the size of our triangle vertex data

	UpdateSubresources(m_commandList.Get(), m_vertexBuffer.Get(), vertexBufferUploadHeap, 0, 0, 1, &vertexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));


	const UINT indexBufferSize = sizeof(indices);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)
	));

	m_indexBuffer->SetName(L"Index Buffer Resource Heap");


	ID3D12Resource* indexBufferUploadHeap;
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBufferUploadHeap)));
	indexBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<BYTE*>(indices); // pointer to our vertex array
	indexData.RowPitch = indexBufferSize; // size of all our triangle vertex data
	indexData.SlicePitch = indexBufferSize; // also the size of our triangle vertex data

	UpdateSubresources(m_commandList.Get(), m_indexBuffer.Get(), indexBufferUploadHeap, 0, 0, 1, &indexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));


	//MATH
	XMMATRIX modelMatrix = XMMatrixIdentity();
	XMMATRIX translationMatrix = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	modelMatrix = modelMatrix * translationMatrix;		  //pitch, yaw, roll
	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(10.0f, 10.0f, 0.0f);
	modelMatrix = modelMatrix * rotationMatrix;
	XMMATRIX scaleMatrix = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	modelMatrix = modelMatrix * scaleMatrix;


	XMMATRIX viewMatrix = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f),  // eyePosition
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


	ThrowIfFailed(m_device->CreateCommittedResource(
	&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(MVPMatrix)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBuffer)));

	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read this resource on the CPU.
	m_constantBuffer->Map(0, &readRange, &pMappedData);

	// Create the MVP matrices and fill a temporary structure.
	MVPMatrix mvpMatrices;
	mvpMatrices.model = XMMatrixTranspose(modelMatrix);
	mvpMatrices.view = XMMatrixTranspose(viewMatrix);
	mvpMatrices.projection = XMMatrixTranspose(projectionMatrix);

	// Copy the matrices to the constant buffer.
	memcpy(pMappedData, &mvpMatrices, sizeof(mvpMatrices));

	// Unmap the constant buffer.
	m_constantBuffer->Unmap(0, nullptr);


	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_fenceValue++;
	m_commandQueue->Signal(m_fence.Get(), m_fenceValue);


	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_vertexBufferView.SizeInBytes = vertexBufferSize;

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = indexBufferSize;
}

void Renderer::LoadAssets() {

	SetupRootSignature();

	SetupShaders();

	SetupFence();

	SetupVertexBuffer();
}

void Renderer::OnUpdate()
{
}

void Renderer::OnRender() {
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

void Renderer::PopulateCommandList() {
	ThrowIfFailed(m_commandAllocator->Reset());

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));
	m_commandList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHandle);

	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->IASetIndexBuffer(&m_indexBufferView);
	//m_commandList->DrawInstanced(6, 1, 0, 0);

	m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	
	ThrowIfFailed(m_commandList->Close());
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