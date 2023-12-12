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

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));


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
	commandList->SetPipelineState(m_pipelineState.Get());


	// Fragment Uniform

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(FragmentUniform)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&fragmentUniformBuffer)
	);
	fragmentUniformBuffer->SetName(L"Fragment Uniform Buffer");

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(Light)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&lightsUniformBuffer)
	);
	lightsUniformBuffer->SetName(L"Lights Buffer");

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

	D3D12_ROOT_PARAMETER rootParameters[4];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
	rootParameters[1].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // our pixel shader will be the only shader accessing this parameter for now

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].Descriptor.ShaderRegister = 1;
	rootParameters[2].Descriptor.RegisterSpace = 0;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].Descriptor.ShaderRegister = 2;
	rootParameters[3].Descriptor.RegisterSpace = 0;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;


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

void Renderer::SetupMeshResources(Mesh& mesh) {
	UpdateSubresources(commandList.Get(), mesh.vertexBuffer.Get(), mesh.vertexBufferUploadHeap.Get(), 0, 0, 1, &mesh.vertexData);
	TransitionResource(mesh.vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	UpdateSubresources(commandList.Get(), mesh.indexBuffer.Get(), mesh.indexBufferUploadHeap.Get(), 0, 0, 1, &mesh.indexData);
	TransitionResource(mesh.indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

	UpdateSubresources(commandList.Get(), mesh.textureBuffer.Get(), mesh.textureBufferUploadHeap.Get(), 0, 0, 1, &mesh.textureData);
	TransitionResource(mesh.textureBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Renderer::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource, stateBefore, stateAfter));
}


void Renderer::SetupLightProperties(Light& light) {
	light.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	light.specularColor = XMFLOAT3(0.6f, 0.6f, 0.6f);
	light.intensity = 1;
	light.attenuation = XMFLOAT3(1.0f, 0.0f, 0.0f);
	light.type = 1;
}

void Renderer::SetupVertexBuffer() {

	// camera #1
	camera = Camera();


	// mesh #1
	Mesh mesh = Mesh("C:/Users/ilyai/Documents/Visual Studio 2022/Projects/d3dRenderer/d3dRenderer/assets/backpack/backpack.obj", L"C:/Users/ilyai/Documents/Visual Studio 2022/Projects/d3dRenderer/d3dRenderer/assets/backpack/diffuse.jpg", m_device);
	mesh.position.y = 3;
	SetupMeshResources(mesh);
	meshes.push_back(mesh);

	// mesh #2
	mesh = Mesh("C:/Users/ilyai/Documents/Visual Studio 2022/Projects/d3dRenderer/d3dRenderer/assets/ground/plane.obj", L"C:/Users/ilyai/Documents/Visual Studio 2022/Projects/d3dRenderer/d3dRenderer/assets/ground/diffuse.png", m_device);
	mesh.scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
	SetupMeshResources(mesh);
	meshes.push_back(mesh);

	// light #1
	Light light = Light();
	Renderer::SetupLightProperties(light);
	light.type = 2;
	light.position = XMFLOAT3(0.0f, 5.0f, 10.0f);
	light.color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	light.attenuation = XMFLOAT3(1.0f, 0.02f, 0.001f);
	light.coneDirection = XMFLOAT3(0, -0.2f, -1.0f);
	lights.push_back(light);


	commandList->Close();

	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_fenceValue++;
	m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
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
	camera.Update(m_width, m_height);
}


void Renderer::OnUpdateCameraZoomDelta(float zoomDelta) {
	camera.UpdateZoomDelta(zoomDelta);
}


void Renderer::OnUpdateCameraPosition(int deltaX, int deltaY) {
	camera.UpdateCameraPosition(deltaX, deltaY);
}


void Renderer::OnRender() {
	//m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);




	// reset to prevent memory leaks
	ThrowIfFailed(commandAllocator->Reset());
	ThrowIfFailed(commandList->Reset(commandAllocator.Get(), m_pipelineState.Get()));
	
	
	

	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 };
	fragmentUniformBuffer->Map(0, &readRange, &pMappedData);

	fragmentUniform.cameraPosition = XMFLOAT3(0.0f, 0.0f, 0.0f); //fix this
	fragmentUniform.lightCount = (float)lights.size();
	memcpy(pMappedData, &fragmentUniform, sizeof(fragmentUniform));
	fragmentUniformBuffer->Unmap(0, nullptr);

	pMappedData = nullptr;
	readRange = { 0, 0 };
	lightsUniformBuffer->Map(0, &readRange, &pMappedData);
	memcpy(pMappedData, &lights[0], sizeof(lights[0]));
	lightsUniformBuffer->Unmap(0, nullptr);

	//Light* bufferPointer = nullptr;
	//readRange = { 0, 0 };
	//lightsUniformBuffer->Map(0, &readRange, reinterpret_cast<void**>(&bufferPointer));
	//size_t lightCount = std::min(lights.size(), static_cast<size_t>(1));
	//memcpy(bufferPointer, lights.data(), sizeof(Light) * lightCount);
	//lightsUniformBuffer->Unmap(0, nullptr);


	commandList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	commandList->RSSetViewports(1, &m_viewport);
	commandList->RSSetScissorRects(1, &m_scissorRect);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
	commandList->SetGraphicsRootConstantBufferView(2, fragmentUniformBuffer->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, lightsUniformBuffer->GetGPUVirtualAddress());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &m_dsvHandle);

	const float clearColor[] = { 0.07f, 0.07f, 0.07f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	

	

	for (Mesh& mesh : meshes) {
		PopulateCommandList(mesh, camera.viewMatrix(), camera.projectionMatrix());
	}


	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(commandList->Close());

	ID3D12CommandList* ppCommandLists[] = { commandList.Get() };


	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	

	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();

	
	
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();

	m_swapChain.Reset();
	m_device.Reset();
	for (UINT n = 0; n < FrameCount; n++) {
		m_renderTargets[n].Reset();
	}
	m_commandQueue.Reset();
	m_rootSignature.Reset();
	m_rtvHeap.Reset();
	m_pipelineState.Reset();
	m_dsvHeap.Reset();
	commandList.Reset();
	commandAllocator.Reset();
	m_fence.Reset();

	CloseHandle(m_fenceEvent);
}

void Renderer::PopulateCommandList(Mesh& mesh, XMMATRIX viewMatrix, XMMATRIX projectionMatrix) {

	ID3D12DescriptorHeap* descriptorHeaps[] = { mesh.mainDescriptorHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	commandList->SetGraphicsRootDescriptorTable(1, mesh.mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	commandList->SetGraphicsRootConstantBufferView(0, mesh.constantBuffer->GetGPUVirtualAddress());
	
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
	commandList->IASetIndexBuffer(&mesh.indexBufferView);

	for (const auto& subMesh : mesh.subMeshes) {
		commandList->DrawIndexedInstanced(subMesh.indexCount, 1, subMesh.startIndexLocation, subMesh.baseVertexLocation, 0);
	}

	XMMATRIX mvpMatrix = mesh.modelMatrix() * viewMatrix * projectionMatrix;


	void* pMappedData = nullptr;
	D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read this resource on the CPU.
	mesh.constantBuffer->Map(0, &readRange, &pMappedData);

	// Create the MVP matrices and fill a temporary structure.
	MVPMatrix mvpMatrices;
	mvpMatrices.model = XMMatrixTranspose(mesh.modelMatrix());
	mvpMatrices.view = XMMatrixTranspose(viewMatrix);
	mvpMatrices.projection = XMMatrixTranspose(projectionMatrix);


	XMFLOAT4X4 fMatrix;
	XMStoreFloat4x4(&fMatrix, mesh.modelMatrix()); // Convert XMMATRIX to XMFLOAT4X4

	// Access elements in the first row
	float m11 = fMatrix.m[0][0];
	float m12 = fMatrix.m[0][1];
	float m13 = fMatrix.m[0][2];
	float m14 = fMatrix.m[0][3];

	// Access elements in the second row
	float m21 = fMatrix.m[1][0];
	float m22 = fMatrix.m[1][1];
	float m23 = fMatrix.m[1][2];
	float m24 = fMatrix.m[1][3];

	// Access elements in the third row
	float m31 = fMatrix.m[2][0];
	float m32 = fMatrix.m[2][1];
	float m33 = fMatrix.m[2][2];
	float m34 = fMatrix.m[2][3];

	// Access elements in the fourth row
	float m41 = fMatrix.m[3][0];
	float m42 = fMatrix.m[3][1];
	float m43 = fMatrix.m[3][2];
	float m44 = fMatrix.m[3][3];
	XMMATRIX upperLeft3x3 = XMMATRIX(
		m11, m12, m13, 0.0f,
		m21, m22, m23, 0.0f,
		m31, m32, m33, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	mvpMatrices.normalMatrix = upperLeft3x3;
	// Copy the matrices to the constant buffer.
	memcpy(pMappedData, &mvpMatrices, sizeof(mvpMatrices));

	// Unmap the constant buffer.
	mesh.constantBuffer->Unmap(0, nullptr);
}



void Renderer::WaitForPreviousFrame() {
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	if (m_fence->GetCompletedValue() < fence) {
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}