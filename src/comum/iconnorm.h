#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cmath>
// Shared premultiplied BGRA normalizer. A 10% transparent margin and a
// consistent continuous-corner body; never stretch or recolor the emblem.
namespace iconnorm {
inline bool Normalize(UINT& w, UINT& h, std::vector<BYTE>& p, bool dark, bool trusted=false, UINT size=0) {
    if(!w||!h||p.size()!=(size_t)w*h*4)return false;
    int l=(int)w,t=(int)h,r=-1,b=-1;
    for(UINT y=0;y<h;y++)for(UINT x=0;x<w;x++)if(p[((size_t)y*w+x)*4+3]>40){
        l=std::min(l,(int)x);t=std::min(t,(int)y);r=std::max(r,(int)x);b=std::max(b,(int)y);
    }
    if(r<l||b<t)return false;
    int bw=r-l+1,bh=b-t+1;
    auto alpha=[&](float x,float y){return p[((size_t)(t+(int)(y*(bh-1)))*w+l+(int)(x*(bw-1)))*4+3];};
    bool tile=bw>=.9f*bh&&bh>=.9f*bw;
    for(float v:{.07f,.93f})tile=tile&&alpha(.5f,v)>200&&alpha(v,.5f)>200;
    // Circle/diamond and irregular art do not pass the diagonal test.
    tile=tile&&alpha(.12f,.12f)>180&&alpha(.88f,.12f)>180&&alpha(.12f,.88f)>180&&alpha(.88f,.88f)>180;
    UINT n=size?size:std::clamp(std::max(w,h),96u,256u);
    float body=.8f*n, left=(n-body)/2;
    std::vector<BYTE> out((size_t)n*n*4,0);
    // Median of eight mirrored corners in supplied 1024px GoldenGate Dark
    // STM32CubeIDE master, alpha > 128, opaque body 824px. Only geometry
    // is retained, no master artwork. Sample x = .5*(i/64)^2.
    static constexpr float contour[65]={
        .305825f,.305825f,.305825f,.279126f,.252427f,.239078f,.228155f,.219660f,
        .211165f,.197816f,.186893f,.178398f,.169903f,.160194f,.150485f,.141990f,
        .134709f,.128034f,.120146f,.114078f,.106796f,.100728f,.093447f,.087379f,
        .081311f,.075243f,.069175f,.063107f,.057039f,.052184f,.047330f,.041262f,
        .037621f,.032767f,.029126f,.025485f,.021845f,.018204f,.015777f,.013350f,
        .010922f,.008495f,.007282f,.006068f,.004854f,.003641f,.002427f,.002427f,
        .001214f,.001214f,.001214f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f
    };
    auto sd=[&](float x,float y){
        float dx=.5f-std::fabs(x-n*.5f)/body,dy=.5f-std::fabs(y-n*.5f)/body;
        float u=std::max(dx,dy),v=std::min(dx,dy);
        float q=64.f*std::sqrt(std::clamp(2*u,0.f,1.f));
        int i=std::min(63,(int)q);
        float x0=.5f*(i/64.f)*(i/64.f),x1=.5f*((i+1)/64.f)*((i+1)/64.f);
        float slope=(contour[i+1]-contour[i])/(x1-x0);
        float height=contour[i]+(q-i)*(contour[i+1]-contour[i]);
        return (height-v)*body/std::sqrt(1+slope*slope);
    };
    bool plate=!tile;
    float art=plate?body*.72f:body;
    float scale=art/std::max(bw,bh),ox=(n-bw*scale)/2,oy=(n-bh*scale)/2;
    for(UINT y=0;y<n;y++)for(UINT x=0;x<n;x++){
        float d=sd(x+.5f,y+.5f),cov=std::clamp(.5f-d,0.f,1.f);
        if(cov<=0)continue;
        BYTE* o=&out[((size_t)y*n+x)*4];
        if(plate){
            float v=std::clamp((y-left)/body,0.f,1.f),u=std::clamp((x-left)/body,0.f,1.f);
            float c=dark?.19f-.095f*v+.025f*(1-u):.94f-.13f*v;
            if(d> -body*.018f)c+= (dark?.11f:.04f)*(1-v)-.035f*v;
            o[3]=(BYTE)std::lround(255*cov);
            o[0]=o[1]=o[2]=(BYTE)std::lround(std::clamp(c,0.f,1.f)*o[3]);
        }
        float sx=(x+.5f-ox)/scale+l-.5f,sy=(y+.5f-oy)/scale+t-.5f;
        if(sx<l-1||sy<t-1||sx>r+1||sy>b+1)continue;
        int ix=(int)std::floor(sx),iy=(int)std::floor(sy);
        float fx=sx-ix,fy=sy-iy,a[4]={};
        for(int k=0;k<4;k++){
            int xx=ix+(k&1),yy=iy+(k>>1);
            if(xx<l||xx>r||yy<t||yy>b)continue;
            float wt=((k&1)?fx:1-fx)*((k&2)?fy:1-fy);
            const BYTE* q=&p[((size_t)yy*w+xx)*4];
            for(int c=0;c<4;c++)a[c]+=q[c]*wt;
        }
        float sa=a[3]/255.f;
        for(int c=0;c<4;c++)o[c]=(BYTE)std::clamp(std::lround(a[c]*cov+o[c]*(1-sa)),0L,255L);
    }
    w=h=n;p.swap(out);return true;
}
}
