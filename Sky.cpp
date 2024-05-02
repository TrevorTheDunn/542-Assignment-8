#include "Sky.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"
#include "Helpers.h"

using namespace DirectX;

#define LoadShader(type, file) std::make_shared<type>(device.Get(), context.Get(), FixPath(file).c_str())

Sky::Sky(
	const wchar_t* cubemapDDSFile, 
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS, 
	std::shared_ptr<SimplePixelShader> skyPS, 
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions, 
	Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Save params
	this->skyMesh = mesh;
	this->device = device;
	this->context = context;
	this->samplerOptions = samplerOptions;
	this->skyVS = skyVS;
	this->skyPS = skyPS;

	// Init render states
	InitRenderStates();

	// Load texture
	CreateDDSTextureFromFile(device.Get(), cubemapDDSFile, 0, skySRV.GetAddressOf());

	IBLCreateIrradianceMap(512);
	IBLCreateConvolvedSpecularMap(512);
	IBLCreateBRDFLookUpTexture(1024);
}

Sky::Sky(
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeMap,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) :
	skySRV(cubeMap),
	samplerOptions(samplerOptions),
	device(device),
	context(context)
{
	// Init render states
	InitRenderStates();

	IBLCreateIrradianceMap(512);
	IBLCreateConvolvedSpecularMap(512);
	IBLCreateBRDFLookUpTexture(1024);
}

Sky::Sky(
	const wchar_t* right, 
	const wchar_t* left, 
	const wchar_t* up, 
	const wchar_t* down, 
	const wchar_t* front, 
	const wchar_t* back, 
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS,
	std::shared_ptr<SimplePixelShader> skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Save params
	this->skyMesh = mesh;
	this->device = device;
	this->context = context;
	this->samplerOptions = samplerOptions;
	this->skyVS = skyVS;
	this->skyPS = skyPS;

	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skySRV = CreateCubemap(right, left, up, down, front, back);

	IBLCreateIrradianceMap(512);
	IBLCreateConvolvedSpecularMap(512);
	IBLCreateBRDFLookUpTexture(1024);
}

Sky::Sky(
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> right,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> left,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> up,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> down,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> front,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> back,
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS,
	std::shared_ptr<SimplePixelShader> skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Save params
	this->skyMesh = mesh;
	this->device = device;
	this->context = context;
	this->samplerOptions = samplerOptions;
	this->skyVS = skyVS;
	this->skyPS = skyPS;

	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skySRV = CreateCubemap(right, left, up, down, front, back);

	IBLCreateIrradianceMap(512);
	IBLCreateConvolvedSpecularMap(512);
	IBLCreateBRDFLookUpTexture(1024);
}

Sky::~Sky()
{
}

void Sky::Draw(std::shared_ptr<Camera> camera)
{
	// Change to the sky-specific rasterizer state
	context->RSSetState(skyRasterState.Get());
	context->OMSetDepthStencilState(skyDepthState.Get(), 0);

	// Set the sky shaders
	skyVS->SetShader();
	skyPS->SetShader();

	// Give them proper data
	skyVS->SetMatrix4x4("view", camera->GetView());
	skyVS->SetMatrix4x4("projection", camera->GetProjection());
	skyVS->CopyAllBufferData();

	// Send the proper resources to the pixel shader
	skyPS->SetShaderResourceView("skyTexture", skySRV);
	skyPS->SetSamplerState("samplerOptions", samplerOptions);

	// Set mesh buffers and draw
	skyMesh->SetBuffersAndDraw(context);

	// Reset my rasterizer state to the default
	context->RSSetState(0); // Null (or 0) puts back the defaults
	context->OMSetDepthStencilState(0, 0);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetIrradianceMap()
{
	return irradianceIBL;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetSpecularMap()
{
	return specularIBL;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetBRDFLookUpTexture()
{
	return brdfMap;
}

int Sky::GetTotalSpecularIBLMipsLevels()
{
	return totalSpecIBLMipLevels;
}

void Sky::InitRenderStates()
{
	// Rasterizer to reverse the cull mode
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.CullMode = D3D11_CULL_FRONT; // Draw the inside instead of the outside!
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&rastDesc, skyRasterState.GetAddressOf());

	// Depth state so that we ACCEPT pixels with a depth == 1
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	device->CreateDepthStencilState(&depthDesc, skyDepthState.GetAddressOf());
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::CreateCubemap(
	const wchar_t* right,
	const wchar_t* left,
	const wchar_t* up,
	const wchar_t* down,
	const wchar_t* front,
	const wchar_t* back)
{
	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not the SHADER RESOURCE VIEWS!
	// - Specifically NOT generating mipmaps, as we don't need them for the sky!
	// - Order matters here!  +X, -X, +Y, -Y, +Z, -Z
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textures[6] = {};
	CreateWICTextureFromFile(device.Get(), right, 0, textures[0].GetAddressOf());
	CreateWICTextureFromFile(device.Get(), left, 0, textures[1].GetAddressOf());
	CreateWICTextureFromFile(device.Get(), up, 0, textures[2].GetAddressOf());
	CreateWICTextureFromFile(device.Get(), down, 0, textures[3].GetAddressOf());
	CreateWICTextureFromFile(device.Get(), front, 0, textures[4].GetAddressOf());
	CreateWICTextureFromFile(device.Get(), back, 0, textures[5].GetAddressOf());

	// Send back the SRV, which is what we need for our shaders
	return CreateCubemap(textures[0], textures[1], textures[2], textures[3], textures[4], textures[5]);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::CreateCubemap(
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> right,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> left,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> up,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> down,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> front,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> back)
{
	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not the SHADER RESOURCE VIEWS!
	// - Order matters here!  +X, -X, +Y, -Y, +Z, -Z
	ID3D11Resource* textures[6] = {};
	right.Get()->GetResource(&textures[0]);
	left.Get()->GetResource(&textures[1]);
	up.Get()->GetResource(&textures[2]);
	down.Get()->GetResource(&textures[3]);
	front.Get()->GetResource(&textures[4]);
	back.Get()->GetResource(&textures[5]);

	// We'll assume all of the textures are the same color format and resolution,
	// so get the description of the first shader resource view
	D3D11_TEXTURE2D_DESC faceDesc = {};
	((ID3D11Texture2D*)textures[0])->GetDesc(&faceDesc);

	// Describe the resource for the cube map, which is simply 
	// a "texture 2d array".  This is a special GPU resource format, 
	// NOT just a C++ array of textures!!!
	D3D11_TEXTURE2D_DESC cubeDesc = {};
	cubeDesc.ArraySize = 6; // Cube map!
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We'll be using as a texture in a shader
	cubeDesc.CPUAccessFlags = 0; // No read back
	cubeDesc.Format = faceDesc.Format; // Match the loaded texture's color format
	cubeDesc.Width = faceDesc.Width;  // Match the size
	cubeDesc.Height = faceDesc.Height; // Match the size
	cubeDesc.MipLevels = 1; // Only need 1
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // This should be treated as a CUBE, not 6 separate textures
	cubeDesc.Usage = D3D11_USAGE_DEFAULT; // Standard usage
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the actual texture resource
	Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeMapTexture;
	device->CreateTexture2D(&cubeDesc, 0, &cubeMapTexture);

	// Loop through the individual face textures and copy them,
	// one at a time, to the cube map texure
	for (int i = 0; i < 6; i++)
	{
		// Calculate the subresource position to copy into
		unsigned int subresource = D3D11CalcSubresource(
			0,	// Which mip (zero, since there's only one)
			i,	// Which array element?
			1); // How many mip levels are in the texture?

		// Copy from one resource (texture) to another
		context->CopySubresourceRegion(
			cubeMapTexture.Get(), // Destination resource
			subresource,		// Dest subresource index (one of the array elements)
			0, 0, 0,			// XYZ location of copy
			textures[i],		// Source resource
			0,					// Source subresource index (we're assuming there's only one)
			0);					// Source subresource "box" of data to copy (zero means the whole thing)
	}

	// At this point, all of the faces have been copied into the 
	// cube map texture, so we can describe a shader resource view for it
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format; // Same format as texture
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	srvDesc.TextureCube.MipLevels = 1;	// Only need access to 1 mip
	srvDesc.TextureCube.MostDetailedMip = 0; // Index of the first mip we want to see

	// Make the SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeSRV;
	device->CreateShaderResourceView(cubeMapTexture.Get(), &srvDesc, cubeSRV.GetAddressOf());

	// Clean up our extra texture refs
	for (int i = 0; i < 6; i++)
		textures[i]->Release();

	// Send back the SRV, which is what we need for our shaders
	return cubeSRV;
}

void Sky::IBLCreateIrradianceMap(int cubeFaceSize)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> irrMapTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = cubeFaceSize;
	texDesc.Height = cubeFaceSize;
	texDesc.ArraySize = 6; // Cube map is 6 textures
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.MipLevels = 1; // No mip chain needed for irradiance map. Specular will need more!
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // It's a cube map, not just an array
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, irrMapTexture.GetAddressOf());

	// Create an SRV for the irradiance texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Treat this resource as a cube map
	srvDesc.TextureCube.MipLevels = 1; // Only 1 mip level
	srvDesc.TextureCube.MostDetailedMip = 0; // Accessing the first (and only) mip
	srvDesc.Format = texDesc.Format; // Same format as texture
	device->CreateShaderResourceView(irrMapTexture.Get(), &srvDesc, irradianceIBL.GetAddressOf());

	// Saves current render target, depth buffer, and viewport
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previousRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDSV;
	context->OMGetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.GetAddressOf());
	unsigned int viewCount = 1;
	D3D11_VIEWPORT previousViewport = {};
	context->RSGetViewports(&viewCount, &previousViewport);

	// Creates viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)cubeFaceSize;
	vp.Height = (float)cubeFaceSize;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Sets shaders
	std::shared_ptr<SimpleVertexShader> fullscreenVS = LoadShader(SimpleVertexShader, L"FullscreenVS.cso");
	std::shared_ptr<SimplePixelShader> irradiancePS = LoadShader(SimplePixelShader, L"IBLIrradianceMapPS.cso");
	fullscreenVS->SetShader();
	irradiancePS->SetShader();
	irradiancePS->SetShaderResourceView("EnvironmentMap", skySRV.Get());
	irradiancePS->SetSamplerState("BasicSampler", samplerOptions.Get());

	// Loops through each face
	for (int i = 0; i < 6; i++)
	{
		// Make a render target view for this face
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY; // This points to a Texture2D Array
		rtvDesc.Texture2DArray.ArraySize = 1; // How much of this array to do we want to access?
		rtvDesc.Texture2DArray.FirstArraySlice = i; // Which array index are we rendering into? 0-5
		rtvDesc.Texture2DArray.MipSlice = 0; // Which mip of that texture are we rendering into?
		rtvDesc.Format = texDesc.Format; // Same format as texture

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		device->CreateRenderTargetView(irrMapTexture.Get(), &rtvDesc, rtv.GetAddressOf());

		// Clears and sets render target
		float black[4] = {};
		context->ClearRenderTargetView(rtv.Get(), black);
		context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

		// Passes required data to pixel shader
		irradiancePS->SetInt("faceIndex", i);
		irradiancePS->SetFloat("sampleStepPhi", 0.025f);
		irradiancePS->SetFloat("sampleStepTheta", 0.025f);
		irradiancePS->CopyAllBufferData();

		// Draws 3 vertices
		context->Draw(3, 0);

		context->Flush();
	}

	// Restores old states
	context->OMSetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.Get());
	context->RSSetViewports(1, &previousViewport);
}

void Sky::IBLCreateConvolvedSpecularMap(int cubeFaceSize)
{
	totalSpecIBLMipLevels = max((int)(log2(cubeFaceSize)) + 1 - specIBLMipLevelsToSkip, 1);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> specuclarConvolvedTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = cubeFaceSize;
	texDesc.Height = cubeFaceSize;
	texDesc.ArraySize = 6; 
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.MipLevels = totalSpecIBLMipLevels;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
	texDesc.SampleDesc.Count = 1;
	device->CreateTexture2D(&texDesc, 0, specuclarConvolvedTexture.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = totalSpecIBLMipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.Format = texDesc.Format;
	device->CreateShaderResourceView(specuclarConvolvedTexture.Get(), &srvDesc, specularIBL.GetAddressOf());

	// Saves current render target, depth buffer, and viewport
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previousRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDSV;
	context->OMGetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.GetAddressOf());
	unsigned int viewCount = 1;
	D3D11_VIEWPORT previousViewport = {};
	context->RSGetViewports(&viewCount, &previousViewport);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Sets shaders
	std::shared_ptr<SimpleVertexShader> fullscreenVS = LoadShader(SimpleVertexShader, L"FullscreenVS.cso");
	std::shared_ptr<SimplePixelShader> specularConvolvedPS = LoadShader(SimplePixelShader, L"IBLSpecularConvolvedPS.cso");
	fullscreenVS->SetShader();
	specularConvolvedPS->SetShader();
	specularConvolvedPS->SetShaderResourceView("EnvironmentMap", skySRV.Get());
	specularConvolvedPS->SetSamplerState("BasicSampler", samplerOptions.Get());

	// Loops through each mip level
	for (int i = 0; i < totalSpecIBLMipLevels; i++)
	{
		// Loops through each face
		for (int j = 0; j < 6; j++)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.FirstArraySlice = j;
			rtvDesc.Texture2DArray.MipSlice = i;
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
			device->CreateRenderTargetView(specuclarConvolvedTexture.Get(), &rtvDesc, rtv.GetAddressOf());

			float black[4] = {};
			context->ClearRenderTargetView(rtv.Get(), black);
			context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

			D3D11_VIEWPORT vp = {};
			vp.Width = (float)pow(2, totalSpecIBLMipLevels + specIBLMipLevelsToSkip - 1 - i);
			vp.Height = vp.Width;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			context->RSSetViewports(1, &vp);

			specularConvolvedPS->SetFloat("roughness", i / (float)(totalSpecIBLMipLevels - 1));
			specularConvolvedPS->SetInt("faceIndex", j);
			specularConvolvedPS->SetInt("mipLevel", i);
			specularConvolvedPS->CopyAllBufferData();

			context->Draw(3, 0);

			context->Flush();
		}
	}

	context->OMSetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.Get());
	context->RSSetViewports(1, &previousViewport);
}

void Sky::IBLCreateBRDFLookUpTexture(int textureSize)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> environmentBRDFTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = 256;
	texDesc.Height = texDesc.Width;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R16G16_UNORM;
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1;
	device->CreateTexture2D(&texDesc, 0, environmentBRDFTexture.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Format = texDesc.Format;
	device->CreateShaderResourceView(environmentBRDFTexture.Get(), &srvDesc, brdfMap.GetAddressOf());

	// Save previous render target, depth buffer, and viewport
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> previousRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDSV;
	context->OMGetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.GetAddressOf());
	unsigned int viewCount = 1;
	D3D11_VIEWPORT previousViewport = {};
	context->RSGetViewports(&viewCount, &previousViewport);

	D3D11_VIEWPORT vp = {};
	vp.Width = 256.0f;
	vp.Height = vp.Width;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set up shaders
	std::shared_ptr<SimpleVertexShader> fullscreenVS = LoadShader(SimpleVertexShader, L"FullscreenVS.cso");
	std::shared_ptr<SimplePixelShader> environmentBRDFPS = LoadShader(SimplePixelShader, L"IBLBRDFLookUpTablePS.cso");
	fullscreenVS->SetShader();
	environmentBRDFPS->SetShader();

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Format = texDesc.Format;

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	device->CreateRenderTargetView(environmentBRDFTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Clear and set render target
	float black[4] = {};
	context->ClearRenderTargetView(rtv.Get(), black);
	context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

	context->Draw(3, 0);

	context->Flush();

	// Return to old render target and viewport
	context->OMSetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.Get());
	context->RSSetViewports(1, &previousViewport);
}