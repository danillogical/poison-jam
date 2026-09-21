/* Native D3D11 fixture, separate from guest/NV2A execution. No visible window. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <d3d11sdklayers.h>
#include <dxgi1_2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#ifdef JSRF_HAVE_RENDERDOC
#include <renderdoc_app.h>
#endif

#define RELEASE(p) do { if (p) { IUnknown_Release((IUnknown *)(p)); p=NULL; } } while (0)
#define HR(call) do { HRESULT h=(call); if (FAILED(h)) { fprintf(stderr,"%s failed: 0x%08lX\n",#call,(unsigned long)h); goto done; } } while (0)

int main(int argc, char **argv)
{
    IDXGIFactory1 *factory=NULL;
    IDXGIAdapter1 *adapter=NULL;
    ID3D11Device *device=NULL;
    ID3D11DeviceContext *context=NULL;
    ID3D11Texture2D *target=NULL, *staging=NULL;
    ID3D11RenderTargetView *view=NULL;
    ID3D11Query *query=NULL;
    ID3D11InfoQueue *info=NULL;
    ID3DUserDefinedAnnotation *annotation=NULL;
    DXGI_ADAPTER_DESC1 adapter_desc={0};
    D3D11_TEXTURE2D_DESC desc={0};
    D3D11_QUERY_DESC query_desc={D3D11_QUERY_EVENT,0};
    D3D11_MAPPED_SUBRESOURCE mapped;
    D3D_FEATURE_LEVEL level;
    int warp=argc>1 && !strcmp(argv[1],"--warp"), ok=0, capture=0;
    unsigned pixels=0, debug_errors=0;
    ULONGLONG started=GetTickCount64();
#ifdef JSRF_HAVE_RENDERDOC
    RENDERDOC_API_1_6_0 *rdoc=NULL;
    HMODULE module=GetModuleHandleA("renderdoc.dll");
    if (module) {
        pRENDERDOC_GetAPI get_api=(pRENDERDOC_GetAPI)GetProcAddress(module,"RENDERDOC_GetAPI");
        if (!get_api || !get_api(eRENDERDOC_API_Version_1_6_0,(void **)&rdoc)) return 2;
    }
#endif
    if (!warp) {
        HR(CreateDXGIFactory1(&IID_IDXGIFactory1,(void **)&factory));
        /* Explicit NVIDIA selection on hybrid laptops. No silent WARP fallback. */
        for (UINT i=0; IDXGIFactory1_EnumAdapters1(factory,i,&adapter)==S_OK; ++i) {
            IDXGIAdapter1_GetDesc1(adapter,&adapter_desc);
            if (adapter_desc.VendorId==0x10DE && !(adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
            RELEASE(adapter);
        }
        if (!adapter) { fprintf(stderr,"No NVIDIA hardware adapter; use --warp for explicit software reference\n"); goto done; }
    }
    HR(D3D11CreateDevice((IDXGIAdapter *)adapter,warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN,
                        NULL,D3D11_CREATE_DEVICE_DEBUG,NULL,0,D3D11_SDK_VERSION,&device,&level,&context));
    HR(ID3D11Device_QueryInterface(device,&IID_ID3D11InfoQueue,(void **)&info));
    HR(ID3D11DeviceContext_QueryInterface(context,&IID_ID3DUserDefinedAnnotation,(void **)&annotation));
#ifdef JSRF_HAVE_RENDERDOC
    if (rdoc) rdoc->StartFrameCapture(NULL,NULL);
#endif
    ID3DUserDefinedAnnotation_BeginEvent(annotation,L"JSRF harness: clear, GPU completion, readback");
    desc.Width=desc.Height=64; desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    HR(ID3D11Device_CreateTexture2D(device,&desc,NULL,&target));
    ID3D11Texture2D_SetPrivateData(target,&WKPDID_D3DDebugObjectName,25,"JSRF harness color target");
    HR(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource *)target,NULL,&view));
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    HR(ID3D11Device_CreateTexture2D(device,&desc,NULL,&staging));
    HR(ID3D11Device_CreateQuery(device,&query_desc,&query));
    const FLOAT color[4]={0.25f,0.5f,0.75f,1.0f};
    ID3D11DeviceContext_ClearRenderTargetView(context,view,color);
    ID3D11DeviceContext_CopyResource(context,(ID3D11Resource *)staging,(ID3D11Resource *)target);
    ID3D11DeviceContext_End(context,(ID3D11Asynchronous *)query);
    ID3D11DeviceContext_Flush(context);
    BOOL completed=FALSE;
    ULONGLONG deadline=GetTickCount64()+5000;
    while (!completed) {
        HRESULT h=ID3D11DeviceContext_GetData(context,(ID3D11Asynchronous *)query,&completed,sizeof(completed),0);
        if (FAILED(h) || GetTickCount64()>=deadline) {
            fprintf(stderr,"GPU completion failed/timed out, HRESULT=0x%08lX removed=0x%08lX\n",
                    (unsigned long)h,(unsigned long)ID3D11Device_GetDeviceRemovedReason(device)); goto done;
        }
        if (!completed) Sleep(1);
    }
    HR(ID3D11DeviceContext_Map(context,(ID3D11Resource *)staging,0,D3D11_MAP_READ,0,&mapped));
    unsigned bad=0;
    const int expected[4]={64,128,191,255};
    for (unsigned y=0;y<64;++y) for (unsigned x=0;x<64;++x) {
        const unsigned char *pixel=(const unsigned char *)mapped.pData+y*mapped.RowPitch+4*x;
        for (unsigned c=0;c<4;++c) if (abs(pixel[c]-expected[c])>1) ++bad;
        ++pixels;
    }
    ID3D11DeviceContext_Unmap(context,(ID3D11Resource *)staging,0);
    ID3DUserDefinedAnnotation_EndEvent(annotation);
#ifdef JSRF_HAVE_RENDERDOC
    if (rdoc) {
        capture=(int)rdoc->EndFrameCapture(NULL,NULL);
        if (!capture) { fprintf(stderr,"RenderDoc capture did not complete\n"); goto done; }
    }
#endif
    for (UINT64 i=0;i<ID3D11InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(info);++i) {
        SIZE_T size=0;
        ID3D11InfoQueue_GetMessage(info,i,NULL,&size);
        D3D11_MESSAGE *message=malloc(size);
        if (!message) goto done;
        if (SUCCEEDED(ID3D11InfoQueue_GetMessage(info,i,message,&size)) &&
            message->Severity<=D3D11_MESSAGE_SEVERITY_ERROR) {
            ++debug_errors; fprintf(stderr,"D3D11 validation: %s\n",message->pDescription);
        }
        free(message);
    }
    if (bad || debug_errors) { fprintf(stderr,"Pixel mismatches=%u, debug errors=%u\n",bad,debug_errors); goto done; }
    ok=1;
done:
    printf("{\"outcome\":\"%s\",\"warp\":%s,\"adapter\":\"",ok?"pass":"fail",warp?"true":"false");
    if (warp) fputs("WARP software reference",stdout);
    else for (unsigned i=0;adapter_desc.Description[i];++i) {
        unsigned c=adapter_desc.Description[i];
        putchar(c>=32 && c<127 && c!='"' && c!='\\' ? (int)c : '?');
    }
    printf("\",\"vendor_id\":%u,\"feature_level\":%u,\"pixels_verified\":%u,\"debug_errors\":%u,"
           "\"renderdoc_capture\":%s,\"elapsed_ms\":%llu}\n",adapter_desc.VendorId,
           device ? (unsigned)ID3D11Device_GetFeatureLevel(device) : 0,pixels,debug_errors,
           capture?"true":"false",(unsigned long long)(GetTickCount64()-started));
    RELEASE(annotation); RELEASE(info); RELEASE(query); RELEASE(view);
    RELEASE(staging); RELEASE(target); RELEASE(context); RELEASE(device); RELEASE(adapter); RELEASE(factory);
    return ok?0:1;
}
