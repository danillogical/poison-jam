/* Runs actual recovered guest code against loaded XBE tables. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <xbox/xboxrecomp.h>
extern RECOMP_TLS uint32_t g_eax,g_esp,g_ebx,g_esi,g_edi,g_ebp;
extern ptrdiff_t g_xbox_mem_offset;
extern void sub_00199830(void),sub_001998D0(void),sub_00199A30(void);
extern void sub_00199A60(void),sub_00199AE0(void);
extern void recomp_guest_cache_flush(void);
#define WORD(va) (*(uint32_t*)(g_xbox_mem_offset+(uint32_t)(va)))
static int failed;
static uint32_t invoke(void (*function)(void),const uint32_t *args,unsigned count)
{
    uint32_t sp=g_esp,bx=g_ebx,si=g_esi,di=g_edi,bp=g_ebp;
    for(unsigned i=count;i;i--) {g_esp-=4;WORD(g_esp)=args[i-1];}
    g_esp-=4;WORD(g_esp)=0x00F00D10;
    function();
    if(g_esp!=sp || g_ebx!=bx || g_esi!=si || g_edi!=di || g_ebp!=bp) {
        fprintf(stderr,"VIDEO ABI FAILURE sp=%08X expected=%08X\n",g_esp,sp);failed=1;
    }
    return g_eax;
}
#define CHECK(condition) do {if(!(condition)){fprintf(stderr,"VIDEO check failed line=%d\n",__LINE__);return 1;}}while(0)
int jsrf_probe_video(void)
{
    uint32_t storage=xbox_HeapAlloc(256,16);CHECK(storage);
    /* Exercise the real host barrier (the lifter fixture checks its call/flags).
     * This is not a weak-memory stress test or a GPU-completion test. */
    WORD(storage)=0x12345678;
    recomp_guest_cache_flush();
    CHECK(WORD(storage)==0x12345678);
    memset((void*)(g_xbox_mem_offset+storage),0xCD,256);
    uint32_t caps[]={0,1,storage+4};
    CHECK(invoke(sub_00199A30,caps,3)==0);
    CHECK(!memcmp((void*)(g_xbox_mem_offset+storage+4),(void*)(g_xbox_mem_offset+0x22E480),0xD4));
    CHECK(WORD(storage)==0xCDCDCDCD && WORD(storage+0xD8)==0xCDCDCDCD);
    caps[0]=1;CHECK(invoke(sub_00199A30,caps,3)==0x8876086Cu);
    caps[0]=0;caps[1]=2;CHECK(invoke(sub_00199A30,caps,3)==0x8876086Bu);
    uint32_t format[]={0,1,6,0,0,6};
    CHECK(invoke(sub_00199A60,format,6)==0);
    format[0]=1;CHECK(invoke(sub_00199A60,format,6)==0x8876086Cu);
    format[0]=0;format[1]=2;CHECK(invoke(sub_00199A60,format,6)==0x8876086Bu);
    format[1]=1;format[5]=0xA;CHECK(invoke(sub_00199A60,format,6)==0x8876086Au);
    uint32_t depth[]={0,1,0,6,0x2A};
    CHECK(invoke(sub_00199AE0,depth,5)==0);
    depth[4]=6;CHECK(invoke(sub_00199AE0,depth,5)==0x8876086Au);
    uint32_t adapter=0,count=invoke(sub_00199830,&adapter,1);
    CHECK(count && count<=1024 && count%4==0);
    static const uint32_t formats[]={0x1E,0x11,0x1C,0x12};
    uint32_t previous[4]={0};
    for(unsigned i=0;i<count;i++) {
        uint32_t args[]={0,i,storage+4};
        memset((void*)(g_xbox_mem_offset+storage),0xCD,256);
        CHECK(invoke(sub_001998D0,args,3)==0);
        CHECK(WORD(storage)==0xCDCDCDCD && WORD(storage+24)==0xCDCDCDCD);
        CHECK(WORD(storage+4)>0 && WORD(storage+8)>0);
        CHECK(WORD(storage+12)==50 || WORD(storage+12)==60);
        CHECK(WORD(storage+20)==formats[i%4]);
        if(i%4==0) memcpy(previous,(void*)(g_xbox_mem_offset+storage+4),16);
        else CHECK(!memcmp(previous,(void*)(g_xbox_mem_offset+storage+4),16));
    }
    uint32_t past_end[]={0,count,storage+4};
    CHECK(invoke(sub_001998D0,past_end,3)==0x8876086Cu);
    CHECK(!failed);
    fprintf(stderr,"[VIDEO] modes=%u caps_bytes=212 ABI=passed\n",count);
    fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_video\n",
        (unsigned long long)GetTickCount64(),GetCurrentThreadId());
    return 0;
}
