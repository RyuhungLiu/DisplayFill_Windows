#include "HDRDriver.h"
#include <algorithm>
#include <cstdio>

namespace hdr_driver
{

namespace
{
struct RenderConstants
{
    float resolution[2];
    float whiteLevel;
    float cornerRadius;
    float holeRect[4];
    float featherPixels;
    float brightnessScale;
    float padding[2];
};

constexpr char kVertexShaderSource[] = R"(
struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(uint vertexId : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    VSOut output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = positions[vertexId] * float2(0.5, -0.5) + 0.5;
    return output;
}
)";

constexpr char kPixelShaderSource[] = R"(
cbuffer RenderConstants : register(b0)
{
    float2 resolution;
    float whiteLevel;
    float cornerRadius;
    float4 holeRect;
    float featherPixels;
    float brightnessScale;
    float2 padding;
};

float roundedRectSdf(float2 p, float4 rect, float radius)
{
    float2 center = 0.5 * (rect.xy + rect.zw);
    float2 halfSize = 0.5 * (rect.zw - rect.xy);
    float2 q = abs(p - center) - (halfSize - radius);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float2 p = uv * resolution;
    float distanceToHole = roundedRectSdf(p, holeRect, cornerRadius);

    // 1 outside the rounded hole, 0 inside it, smoothly fading across the edge.
    float frameMask = smoothstep(-featherPixels, featherPixels, distanceToHole);
    float brightness = whiteLevel * brightnessScale * frameMask;
    return float4(brightness, brightness, brightness, 1.0);
}
)";
}

//=============================================================================
// Utility Functions
//=============================================================================

void PrintLastError(const char* message, HRESULT hr)
{
    std::printf("%s failed. HRESULT=0x%08X\n", message, static_cast<unsigned int>(hr));
}

void EnsureConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        AllocConsole();
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}

//=============================================================================
// Renderer Implementation
//=============================================================================

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
    Shutdown();
}

bool Renderer::Initialize(HWND hwnd, int width, int height)
{
    m_hwnd = hwnd;
    m_startTick = GetTickCount64();

    if (!CreateDeviceAndSwapChain(hwnd, width, height))
    {
        return false;
    }

    if (!CreateRenderTarget())
    {
        return false;
    }

    if (!CreateRenderPipeline())
    {
        return false;
    }

    std::printf("Renderer initialized: %dx%d, FP16 scRGB, D3D11.1\n", width, height);
    return true;
}

void Renderer::Shutdown()
{
    ReleaseRenderTarget();

    if (m_context)
    {
        m_context->Flush();
    }

    m_swapChain3.Reset();
    m_swapChain.Reset();
    m_constantBuffer.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_context1.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool Renderer::CreateDeviceAndSwapChain(HWND hwnd, int width, int height)
{
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL createdFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        &createdFeatureLevel,
        &m_context);

#if defined(_DEBUG)
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &m_device,
            &createdFeatureLevel,
            &m_context);
    }
#endif

    if (FAILED(hr))
    {
        PrintLastError("D3D11CreateDevice", hr);
        return false;
    }

    m_context.As(&m_context1);

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr))
    {
        PrintLastError("Query IDXGIDevice", hr);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr))
    {
        PrintLastError("IDXGIDevice::GetAdapter", hr);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        PrintLastError("IDXGIAdapter::GetParent", hr);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = 0;

    hr = factory->CreateSwapChainForHwnd(
        m_device.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &m_swapChain);

    if (FAILED(hr))
    {
        PrintLastError("IDXGIFactory2::CreateSwapChainForHwnd", hr);
        return false;
    }

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    hr = m_swapChain.As(&m_swapChain3);
    if (FAILED(hr))
    {
        PrintLastError("Query IDXGISwapChain3", hr);
        return false;
    }

    // Enable HDR
    hr = m_swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
    if (FAILED(hr))
    {
        PrintLastError("IDXGISwapChain3::SetColorSpace1(scRGB)", hr);
        m_hdrActive = false;
    }
    else
    {
        // Verify HDR support
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        hr = m_swapChain->GetContainingOutput(&output);
        if (SUCCEEDED(hr))
        {
            Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
            hr = output.As(&output6);
            if (SUCCEEDED(hr))
            {
                UINT colorSpaceSupport = 0;
                hr = m_swapChain3->CheckColorSpaceSupport(
                    DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,
                    &colorSpaceSupport);

                if (SUCCEEDED(hr))
                {
                    DXGI_OUTPUT_DESC1 outputDesc = {};
                    hr = output6->GetDesc1(&outputDesc);
                    if (SUCCEEDED(hr))
                    {
                        bool scRgbSupported = (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0;
                        bool windowsHdrEnabled = outputDesc.ColorSpace != DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

                        m_hdrActive = scRgbSupported && windowsHdrEnabled;
                        if (!m_hdrActive)
                        {
                            std::printf("System HDR support: scRGB=%d, WindowsHDR=%d. Falling back to SDR.\n",
                                scRgbSupported, windowsHdrEnabled);
                        }
                        else
                        {
                            std::printf("HDR activated. Max luminance: %.1f nits\n", outputDesc.MaxFullFrameLuminance);
                        }
                    }
                }
            }
        }

        if (!m_hdrActive)
        {
            m_hdrActive = false;
        }
    }

    std::printf("D3D Feature Level: 0x%X\n", static_cast<unsigned int>(createdFeatureLevel));

    return true;
}

bool Renderer::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr))
    {
        PrintLastError("IDXGISwapChain::GetBuffer", hr);
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr))
    {
        PrintLastError("ID3D11Device::CreateRenderTargetView", hr);
        return false;
    }

    return true;
}

bool Renderer::CreateRenderPipeline()
{
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompile(
        kVertexShaderSource,
        sizeof(kVertexShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        compileFlags,
        0,
        &vertexShaderBlob,
        &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::printf("Vertex shader compile error: %s\n", static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        PrintLastError("D3DCompile(vertex shader)", hr);
        return false;
    }

    errorBlob.Reset();
    hr = D3DCompile(
        kPixelShaderSource,
        sizeof(kPixelShaderSource) - 1,
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        compileFlags,
        0,
        &pixelShaderBlob,
        &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::printf("Pixel shader compile error: %s\n", static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        PrintLastError("D3DCompile(pixel shader)", hr);
        return false;
    }

    hr = m_device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr))
    {
        PrintLastError("ID3D11Device::CreateVertexShader", hr);
        return false;
    }

    hr = m_device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr))
    {
        PrintLastError("ID3D11Device::CreatePixelShader", hr);
        return false;
    }

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(RenderConstants);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&bufferDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr))
    {
        PrintLastError("ID3D11Device::CreateBuffer(constants)", hr);
        return false;
    }

    return true;
}

void Renderer::ReleaseRenderTarget()
{
    if (m_context)
    {
        ID3D11RenderTargetView* nullViews[] = { nullptr };
        m_context->OMSetRenderTargets(1, nullViews, nullptr);
    }

    m_renderTargetView.Reset();
}

void Renderer::Resize(int width, int height)
{
    if (!m_swapChain || width == 0 || height == 0)
    {
        return;
    }

    ReleaseRenderTarget();

    HRESULT hr = m_swapChain->ResizeBuffers(
        0,
        width,
        height,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        0);

    if (FAILED(hr))
    {
        PrintLastError("IDXGISwapChain::ResizeBuffers", hr);
        return;
    }

    if (m_swapChain3)
    {
        hr = m_swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
        if (FAILED(hr))
        {
            PrintLastError("IDXGISwapChain3::SetColorSpace1 after resize", hr);
        }
    }

    CreateRenderTarget();
}

float Renderer::GetStartupBrightnessScale(const AppState& state) const
{
    const ULONGLONG elapsedMs = GetTickCount64() - m_startTick;
    const float elapsedSeconds = static_cast<float>(elapsedMs) / 1000.0f;
    if (state.settings.startupBreathDurationSeconds <= 0.0f || elapsedSeconds >= state.settings.startupBreathDurationSeconds)
    {
        return state.settings.startupBreathMaxBrightness;
    }

    const float t = std::clamp(elapsedSeconds / state.settings.startupBreathDurationSeconds, 0.0f, 1.0f);
    const float smoothT = t * t * (3.0f - 2.0f * t);
    return state.settings.startupBreathMinBrightness +
        (state.settings.startupBreathMaxBrightness - state.settings.startupBreathMinBrightness) * smoothT;
}

bool Renderer::IsStartupBreathingActive(const AppState& state) const
{
    if (state.settings.startupBreathDurationSeconds <= 0.0f)
    {
        return false;
    }

    const ULONGLONG elapsedMs = GetTickCount64() - m_startTick;
    return elapsedMs < static_cast<ULONGLONG>(state.settings.startupBreathDurationSeconds * 1000.0f);
}

void Renderer::Render(const AppState& state)
{
    if (!m_context || !m_swapChain || !m_renderTargetView || !m_vertexShader || !m_pixelShader || !m_constantBuffer)
    {
        return;
    }

    float clearColor[4] = {};
    state.GetClearColor(clearColor);
    clearColor[0] = 0.0f;
    clearColor[1] = 0.0f;
    clearColor[2] = 0.0f;

    ID3D11RenderTargetView* renderTargetView = m_renderTargetView.Get();
    m_context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    // Clear the whole FP16 backbuffer first. The HWND region handles the physical
    // cut-out, and full clearing prevents undefined/black pixels from appearing
    // around rounded region edges.
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        PrintLastError("ID3D11DeviceContext::Map(constants)", hr);
        return;
    }

    RenderConstants* constants = static_cast<RenderConstants*>(mappedResource.pData);
    constants->resolution[0] = static_cast<float>(state.clientWidth);
    constants->resolution[1] = static_cast<float>(state.clientHeight);
    constants->whiteLevel = state.GetWhiteLevel();
    constants->cornerRadius = static_cast<float>(state.settings.holeCornerRadius);
    constants->holeRect[0] = static_cast<float>(state.holeRect.left);
    constants->holeRect[1] = static_cast<float>(state.holeRect.top);
    constants->holeRect[2] = static_cast<float>(state.holeRect.right);
    constants->holeRect[3] = static_cast<float>(state.holeRect.bottom);
    constants->featherPixels = state.settings.visualCornerFeatherPixels;
    constants->brightnessScale = GetStartupBrightnessScale(state);
    constants->padding[0] = 0.0f;
    constants->padding[1] = 0.0f;
    m_context->Unmap(m_constantBuffer.Get(), 0);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(state.clientWidth);
    viewport.Height = static_cast<float>(state.clientHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &viewport);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* constantBuffers[] = { m_constantBuffer.Get() };
    m_context->PSSetConstantBuffers(0, 1, constantBuffers);
    m_context->Draw(3, 0);

    hr = m_swapChain->Present(1, 0);
    if (FAILED(hr))
    {
        PrintLastError("IDXGISwapChain::Present", hr);
    }
}

} // namespace hdr_driver
