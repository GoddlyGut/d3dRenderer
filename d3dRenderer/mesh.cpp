#include "Mesh.h"

Mesh::Mesh(std::string meshPath, LPCWSTR texturePath, ComPtr<ID3D12Device>& device) {
	this->meshPath = meshPath;
	this->texturePath = texturePath;
	this->device = device;

	SetupMesh();
}

void Mesh::SetupMesh()
{
	InitializeCommandObjects();
	LoadMeshData();
	CreateBuffers();
	LoadTextureResources();
	SetupTextureShader();
	FinalizeSetup();

}

void Mesh::InitializeCommandObjects() {
	
}

void Mesh::LoadMeshData() {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(meshPath, aiProcess_Triangulate | aiProcess_FlipUVs);
	ProcessScene(scene);
}

void Mesh::ProcessScene(const aiScene* scene) {

	UINT currentIndexOffset = 0;
	UINT currentVertexOffset = 0;

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];
		SubMesh subMesh;

		for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
			Vertex vertex;
			vertex.position.x = mesh->mVertices[i].x;
			vertex.position.y = mesh->mVertices[i].y;
			vertex.position.z = mesh->mVertices[i].z;

			if (mesh->mTextureCoords[0]) {
				vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
				vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
			}
			else {
				vertex.texCoord.x = 1;
				vertex.texCoord.y = 1;
			}


			if (mesh->mTextureCoords[0]) {
				vertex.color.x = mesh->mNormals[i].x;
				vertex.color.y = mesh->mNormals[i].y;
				vertex.color.z = mesh->mNormals[i].z;
			}
			else {
				vertex.color.x = 1;
				vertex.color.y = 1;
				vertex.color.z = 1;
			}
			vertices.push_back(vertex);
		}

		for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];

			// Assimp should triangulate the faces when the aiProcess_Triangulate flag is used
			// So each face should have exactly 3 indices
			if (face.mNumIndices == 3) {
				FaceIndices faceIndices;
				faceIndices.a = face.mIndices[0];
				faceIndices.b = face.mIndices[1];
				faceIndices.c = face.mIndices[2];

				indices.push_back(faceIndices);
			}
		}

		subMesh.indexCount = mesh->mNumFaces * 3;
		subMesh.startIndexLocation = currentIndexOffset;
		subMesh.baseVertexLocation = currentVertexOffset;

		subMeshes.push_back(subMesh);

		// Update offsets for next submesh
		currentIndexOffset += subMesh.indexCount;
		currentVertexOffset += mesh->mNumVertices;
	}
}

void Mesh::CreateBuffers() {
	CreateVertexBuffer();
	CreateIndexBuffer();
}

void Mesh::CreateVertexBuffer() {
	const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));


	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer));
	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexBufferUploadHeap)));
	vertexBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	
	vertexData.pData = reinterpret_cast<BYTE*>(vertices.data()); // pointer to our vertex array
	vertexData.RowPitch = vertexBufferSize; // size of all our triangle vertex data
	vertexData.SlicePitch = vertexBufferSize; // also the size of our triangle vertex data

	//UpdateSubresources(commandList.Get(), vertexBuffer.Get(), vertexBufferUploadHeap, 0, 0, 1, &vertexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
}

void Mesh::CreateIndexBuffer() {

	const UINT indexBufferSize = static_cast<UINT>(indices.size() * 3 * sizeof(DWORD));

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBuffer)
	));

	indexBuffer->SetName(L"Index Buffer Resource Heap");


	
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&indexBufferUploadHeap)));
	indexBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	
	indexData.pData = reinterpret_cast<BYTE*>(indices.data()); // pointer to our vertex array
	indexData.RowPitch = indexBufferSize; // size of all our triangle vertex data
	indexData.SlicePitch = indexBufferSize; // also the size of our triangle vertex data

	//UpdateSubresources(commandList.Get(), indexBuffer.Get(), indexBufferUploadHeap, 0, 0, 1, &indexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

}

void Mesh::LoadTextureResources() {
	BYTE* texture;
	int imageBytesPerRow;
	int imageSize = Utils::LoadImageDataFromFile(&texture, textureDesc, texturePath, imageBytesPerRow);

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&textureDesc, // the description of our texture
		D3D12_RESOURCE_STATE_COPY_DEST, // We will copy the texture from the upload heap to here, so we start it out in a copy dest state
		nullptr, // used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&textureBuffer)));

	textureBuffer->SetName(L"Texture Buffer Resource Heap");
	UINT64 textureUploadBufferSize;
	device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	// now we create an upload heap to upload our texture to the GPU
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize), // resource description for a buffer (storing the image data in this heap just to copy to the default heap)
		D3D12_RESOURCE_STATE_GENERIC_READ, // We will copy the contents from this heap to the default heap above
		nullptr,
		IID_PPV_ARGS(&textureBufferUploadHeap)));

	textureBufferUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	
	textureData.pData = &texture[0]; // pointer to our image data
	textureData.RowPitch = imageBytesPerRow; // size of all our triangle vertex data
	textureData.SlicePitch = imageBytesPerRow * textureDesc.Height; // also the size of our triangle vertex data

	// Now we copy the upload buffer contents to the default heap
	//UpdateSubresources(commandList.Get(), textureBuffer.Get(), textureBufferUploadHeap.Get(), 0, 0, 1, &textureData);

	// transition the texture default heap to a pixel shader resource (we will be sampling from this heap in the pixel shader to get the color of pixels)
	//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	//delete texture;
}

void Mesh::SetupTextureShader() {

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(textureBuffer.Get(), &srvDesc, mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(MVPMatrix)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer)));
}

void Mesh::FinalizeSetup() {
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = static_cast<UINT>(indices.size() * 3 * sizeof(DWORD));

	//commandList->Close();
}