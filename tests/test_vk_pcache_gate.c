/* B3's gate for the persisted Vulkan pipeline cache and the pipeline table's eviction counter,
 * decided from the SPECIFICATION (the plan step B2 was given), not from what the code does.
 *
 * What it decides
 * ---------------
 * src/video/nv2a_vk_backend.c persists the VkPipelineCache across runs. RECOMP_VK_PIPELINE_CACHE
 * names the file (unset: %LOCALAPPDATA%\xboxrecomp\vk-pipeline-cache.bin on Windows, disabled
 * elsewhere; `off` in any case or `0` disables it; any other value is the path). The file is a
 * header -- 8-byte magic, u32 version, u32 blob length, u32 CRC-32 of the blob -- and then the
 * driver's vkGetPipelineCacheData blob.
 *
 *   P1 format    a file the back end SAVES has exactly that layout: the length field is the blob's
 *                length, the CRC-32 is an independent CRC-32 (this file carries its own, checked
 *                against the standard check value) of the blob, and the blob is what the driver
 *                returned. The magic and the version are the one thing the specification does not
 *                spell out, so those two constants are read from the production source.
 *   P2 load      a missing file is a quiet cold start (no reject); a valid file is handed to
 *                vkCreatePipelineCache as initial data (size, pointer and bytes) and counted in
 *                cache_loaded_bytes; a blob of exactly 64 MiB is accepted.
 *   P3 reject    every rejection reason in the specification, each on a file that violates THAT
 *                rule only (the length and CRC are recomputed around every other mutation): shorter
 *                than the header, wrong magic, wrong version, length over 64 MiB, length unequal to
 *                the file size minus the header, CRC mismatch, a malformed
 *                VkPipelineCacheHeaderVersionOne, and a vendorID / deviceID / pipelineCacheUUID that
 *                differ from the device. Each is counted once, passes NO initial data, leaves the
 *                back end working (a pipeline can still be created) and is healed by the next save.
 *   P4 refusal   a driver that refuses valid initial data gets an empty cache instead, counted as a
 *                reject, and the back end does not fail.
 *   P5 knob      off / OFF / Off / 0 read nothing and write nothing: no file is opened, no
 *                vkGetPipelineCacheData call is made, and a file of that very name in the working
 *                directory is neither loaded nor touched; unset follows the platform's default
 *                path; any other value is the path.
 *   P6 saving    vb_flip saves only when enabled, only when pipelines were created since the last
 *                good save, and no sooner than 10 s after the previous attempt; one save is one
 *                write that replaces the target (and leaves no .tmp behind).
 *   P7 failure   an unwritable path and a failing vkGetPipelineCacheData each count a save failure,
 *                never fail the back end, are not retried before the interval and ARE retried
 *                after it.
 *   P8 eviction  256 distinct pipeline keys evict nothing; the 257th evicts one and evictions goes
 *                0 -> 1; the table's replacement is the least recently used one.
 *   P9 counters  nv2a_vk_backend_pipeline_stats() and the [GPUVK] line carry the five counters.
 *
 * What it does not do
 * -------------------
 * It compiles NO COPY of the algorithm: the translation unit #includes the toolkit's
 * src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, and drives the production callers --
 * vb_create_pipeline_objects (the load and the vkCreatePipelineCache call), vb_pipeline (creation
 * and eviction) and vb_flip (-> vb_pcache_maybe_save) -- against stubbed Vulkan entry points. The
 * toolkit is not linked; its five undefined names are defined at the bottom. Whether a REAL driver
 * accepts and uses the saved bytes is jsrf_vk_pcache_device's question, not this one's.
 *
 * Install itself (nv2a_vk_backend_install) needs a loader and a device, so the fixture stands in
 * for the two lines of it that matter here: g.ok = 1 and g.pc_last_ns = now. The "first save is
 * at least 10 s after install" clause is decided on the real device, where install runs.
 *
 * No GPU, no ICD, no skip path. The scratch directory is under TEMP and is removed at the end.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>
#define PC_SEP "\\"
#define pc_chdir _chdir
#define pc_mkdir(p) CreateDirectoryA((p), NULL)
#define pc_rmdir _rmdir
#else
#include <unistd.h>
#define PC_SEP "/"
#define pc_chdir chdir
#define pc_mkdir(p) mkdir((p), 0777)
#define pc_rmdir rmdir
#endif

/* ── The stub device ─────────────────────────────────────────────────────────────────── */

#define DEV_VENDOR 0x10DEu
#define DEV_ID     0x2684u
#define STUB_BLOB  4096u                        /* what the stub driver's cache data weighs */
#define PC_HDR     20u                          /* the specification's header */
#define PC_MAX     (64u << 20)                  /* the specification's blob limit */

static uint32_t cur_vendor = DEV_VENDOR, cur_device = DEV_ID;   /* what the stub device reports */

static const uint8_t dev_uuid[VK_UUID_SIZE] = { 0xA0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                                14, 0xAF };

static uintptr_t s_h;                           /* the next made-up handle */
static uint8_t s_arena[1u << 16];               /* what vkMapMemory hands back */

static struct {
    unsigned cache_creates;                     /* vkCreatePipelineCache calls */
    size_t init_size[4];                        /* the initialDataSize of each, in order */
    const void *init_ptr[4];
    uint32_t init_crc[4];                       /* CRC-32 of the bytes it was given */
    VkResult refuse_initial;                    /* returned when initial data is passed */
    unsigned data_queries, data_fetches;        /* vkGetPipelineCacheData: size / data calls */
    int data_fail;                              /* the data call fails */
    uint32_t blob_gen;                          /* the generation of the newest data handed out */
    unsigned gfx_creates, destroyed;
    VkDeviceSize buf_size;
    /* How the stub driver answers vkGetPipelineCacheData. */
    int query_zero;                             /* the size query reports 0 */
    int second;                                 /* 0 normal, 1 smaller, 2 INCOMPLETE, 3 larger, 4 zero */
    int bad;                                    /* 0 well formed, 1 truncated header (20 bytes),
                                                 * 2 another vendor, 3 another pipelineCacheUUID */
    /* Atomic replace, observed from inside every vkGetPipelineCacheData call. */
    int watch;
    const char *watch_path;
    const uint8_t *watch_prev;                  /* the complete blob the target must still hold, or
                                                 * NULL: the target must not exist yet */
    size_t watch_prev_n;
    unsigned watch_calls, watch_tmp_missing, watch_target_bad;
} S;

static int file_is(const char *path, const uint8_t *blob, size_t blob_n);
static int exists(const char *path);

static unsigned long cur_pid(void)
{
#if defined(_WIN32)
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

#define HNEW(T) ((T)(uintptr_t)(++s_h))

/* ── Independent reference: CRC-32, the files and the blobs ──────────────────────────── */

static uint32_t ref_crc32(const uint8_t *p, size_t n)
{
    static uint32_t tab[256];
    uint32_t c = 0xFFFFFFFFu;
    size_t i;

    if (!tab[255]) {
        uint32_t k, j, v;
        for (k = 0; k < 256; k++) {
            v = k;
            for (j = 0; j < 8; j++)
                v = (v >> 1) ^ ((v & 1u) ? 0xEDB88320u : 0u);
            tab[k] = v;
        }
    }
    for (i = 0; i < n; i++)
        c = (c >> 8) ^ tab[(c ^ p[i]) & 0xFFu];
    return c ^ 0xFFFFFFFFu;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A VkPipelineCacheHeaderVersionOne for the stub device, then a payload that depends on `seed`. */
static void make_blob(uint8_t *b, size_t n, uint32_t seed)
{
    size_t i;

    memset(b, 0, n < 32u ? n : 32u);
    if (n >= 32u) {
        put32(b, 32u);
        put32(b + 4, (uint32_t)VK_PIPELINE_CACHE_HEADER_VERSION_ONE);
        put32(b + 8, cur_vendor);
        put32(b + 12, cur_device);
        memcpy(b + 16, dev_uuid, VK_UUID_SIZE);
    }
    for (i = 32; i < n; i++)
        b[i] = (uint8_t)(i * 31u + seed);
}

/* Write the specification's header in front of the blob already at file + PC_HDR. The magic and
 * the version are the production's: the specification gives their widths, not their values. */
static void seal(uint8_t *file, size_t n)
{
    memcpy(file, vb_pc_magic, 8);
    put32(file + 8, VB_PCACHE_VER);
    put32(file + 12, (uint32_t)n);
    put32(file + 16, ref_crc32(file + PC_HDR, n));
}

static uint8_t *g_big;                          /* PC_MAX + 1 + PC_HDR bytes: the file builder */
static char g_dir[600];
static char g_log[700];

static void path_in(char *out, size_t cap, const char *name)
{
    snprintf(out, cap, "%s" PC_SEP "%s", g_dir, name);
}

static int write_file(const char *path, const uint8_t *buf, size_t n)
{
    FILE *f = fopen(path, "wb");
    size_t w;

    if (!f)
        return 0;
    w = fwrite(buf, 1, n, f);
    return fclose(f) == 0 && w == n;
}

static uint8_t *read_file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long sz;

    *n = 0;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    b = (uint8_t *)malloc(sz ? (size_t)sz : 1u);
    if (b && fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        free(b);
        b = NULL;
    }
    fclose(f);
    if (b)
        *n = (size_t)sz;
    return b;
}

static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f)
        fclose(f);
    return f != NULL;
}

static void set_env(const char *k, const char *v)
{
#if defined(_WIN32)
    _putenv_s(k, v ? v : "");
#else
    if (v)
        setenv(k, v, 1);
    else
        unsetenv(k);
#endif
}

/* ── The Vulkan entry points the driven paths reach ──────────────────────────────────── */

static void VKAPI_CALL st_props(VkPhysicalDevice pd, VkPhysicalDeviceProperties *p)
{
    (void)pd;
    memset(p, 0, sizeof *p);
    p->vendorID = cur_vendor;
    p->deviceID = cur_device;
    memcpy(p->pipelineCacheUUID, dev_uuid, VK_UUID_SIZE);
    p->limits.nonCoherentAtomSize = 64;
}

static VkResult VKAPI_CALL st_pool(VkDevice d, const VkCommandPoolCreateInfo *i,
                                   const VkAllocationCallbacks *a, VkCommandPool *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkCommandPool);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_cmds(VkDevice d, const VkCommandBufferAllocateInfo *i,
                                   VkCommandBuffer *o)
{
    (void)d; (void)i;
    *o = HNEW(VkCommandBuffer);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_fence(VkDevice d, const VkFenceCreateInfo *i,
                                    const VkAllocationCallbacks *a, VkFence *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkFence);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_dpool(VkDevice d, const VkDescriptorPoolCreateInfo *i,
                                    const VkAllocationCallbacks *a, VkDescriptorPool *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkDescriptorPool);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_buffer(VkDevice d, const VkBufferCreateInfo *i,
                                     const VkAllocationCallbacks *a, VkBuffer *o)
{
    (void)d; (void)a;
    S.buf_size = i->size;
    *o = HNEW(VkBuffer);
    return VK_SUCCESS;
}

static void VKAPI_CALL st_buffer_req(VkDevice d, VkBuffer b, VkMemoryRequirements *r)
{
    (void)d; (void)b;
    r->size = S.buf_size;
    r->alignment = 256u;
    r->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL st_alloc(VkDevice d, const VkMemoryAllocateInfo *i,
                                    const VkAllocationCallbacks *a, VkDeviceMemory *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkDeviceMemory);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_bind(VkDevice d, VkBuffer b, VkDeviceMemory m, VkDeviceSize off)
{
    (void)d; (void)b; (void)m; (void)off;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_map(VkDevice d, VkDeviceMemory m, VkDeviceSize off,
                                  VkDeviceSize size, VkMemoryMapFlags f, void **pp)
{
    (void)d; (void)m; (void)off; (void)size; (void)f;
    *pp = s_arena;
    return VK_SUCCESS;
}

static void VKAPI_CALL st_free(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *a)
{
    (void)d; (void)m; (void)a;
}

static void VKAPI_CALL st_buffer_destroy(VkDevice d, VkBuffer b, const VkAllocationCallbacks *a)
{
    (void)d; (void)b; (void)a;
}

static VkResult VKAPI_CALL st_begin(VkCommandBuffer c, const VkCommandBufferBeginInfo *i)
{
    (void)c; (void)i;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_rpass(VkDevice d, const VkRenderPassCreateInfo *i,
                                    const VkAllocationCallbacks *a, VkRenderPass *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkRenderPass);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_dsl(VkDevice d, const VkDescriptorSetLayoutCreateInfo *i,
                                  const VkAllocationCallbacks *a, VkDescriptorSetLayout *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkDescriptorSetLayout);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_layout(VkDevice d, const VkPipelineLayoutCreateInfo *i,
                                     const VkAllocationCallbacks *a, VkPipelineLayout *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkPipelineLayout);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_shader(VkDevice d, const VkShaderModuleCreateInfo *i,
                                     const VkAllocationCallbacks *a, VkShaderModule *o)
{
    (void)d; (void)i; (void)a;
    *o = HNEW(VkShaderModule);
    return VK_SUCCESS;
}

static void VKAPI_CALL st_shader_destroy(VkDevice d, VkShaderModule m,
                                         const VkAllocationCallbacks *a)
{
    (void)d; (void)m; (void)a;
}

static VkResult VKAPI_CALL st_compute(VkDevice d, VkPipelineCache c, uint32_t n,
                                      const VkComputePipelineCreateInfo *i,
                                      const VkAllocationCallbacks *a, VkPipeline *o)
{
    (void)d; (void)c; (void)n; (void)i; (void)a;
    *o = HNEW(VkPipeline);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL st_graphics(VkDevice d, VkPipelineCache c, uint32_t n,
                                       const VkGraphicsPipelineCreateInfo *i,
                                       const VkAllocationCallbacks *a, VkPipeline *o)
{
    (void)d; (void)c; (void)n; (void)i; (void)a;
    S.gfx_creates++;
    *o = HNEW(VkPipeline);
    return VK_SUCCESS;
}

static void VKAPI_CALL st_pipe_destroy(VkDevice d, VkPipeline p, const VkAllocationCallbacks *a)
{
    (void)d; (void)p; (void)a;
    S.destroyed++;
}

static VkResult VKAPI_CALL st_pcache(VkDevice d, const VkPipelineCacheCreateInfo *i,
                                     const VkAllocationCallbacks *a, VkPipelineCache *o)
{
    unsigned k = S.cache_creates < 4u ? S.cache_creates : 3u;

    (void)d; (void)a;
    S.cache_creates++;
    S.init_size[k] = i->initialDataSize;
    S.init_ptr[k] = i->pInitialData;
    S.init_crc[k] = i->initialDataSize
                  ? ref_crc32((const uint8_t *)i->pInitialData, i->initialDataSize) : 0u;
    if (i->initialDataSize && S.refuse_initial != VK_SUCCESS)
        return S.refuse_initial;
    *o = HNEW(VkPipelineCache);
    return VK_SUCCESS;
}

/* What the save must have done by the time the driver is asked for the data: the <path>.<pid>.tmp
 * file is already open, and the target still holds the previous COMPLETE file (or does not exist
 * yet) -- the new bytes are not written into it, they replace it afterwards. */
static void watch_probe(void)
{
    char tmp[1200];
    int ok;

    if (!S.watch)
        return;
    S.watch_calls++;
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", S.watch_path, cur_pid());
    if (!exists(tmp))
        S.watch_tmp_missing++;
    ok = S.watch_prev ? file_is(S.watch_path, S.watch_prev, S.watch_prev_n) : !exists(S.watch_path);
    if (!ok)
        S.watch_target_bad++;
}

/* The stub driver's cache data: a blob whose payload is the generation of the fetch, so two saves
 * are two different files. A size query does not advance it. */
static VkResult VKAPI_CALL st_pcache_data(VkDevice d, VkPipelineCache c, size_t *n, void *p)
{
    uint8_t *b = (uint8_t *)p;
    size_t cap;

    (void)d; (void)c;
    watch_probe();
    if (!p) {
        S.data_queries++;
        *n = S.query_zero ? 0u : STUB_BLOB;
        return VK_SUCCESS;
    }
    S.data_fetches++;
    if (S.data_fail)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    cap = *n;
    S.blob_gen++;
    if (S.bad == 1) {                           /* the shape a software ICD really returns */
        uint8_t full[STUB_BLOB];

        make_blob(full, STUB_BLOB, S.blob_gen);
        memcpy(b, full, 20);
        *n = 20;
        return VK_SUCCESS;
    }
    if (S.second == 1) {
        make_blob(b, 3000, S.blob_gen);
        *n = 3000;
        return VK_SUCCESS;
    }
    make_blob(b, cap < STUB_BLOB ? cap : STUB_BLOB, S.blob_gen);
    if (S.bad == 2)
        put32(b + 8, cur_vendor + 1u);
    if (S.bad == 3)
        b[16] ^= 1u;
    *n = STUB_BLOB;
    if (S.second == 2)
        return VK_INCOMPLETE;
    if (S.second == 3)
        *n = STUB_BLOB + 100u;
    if (S.second == 4)
        *n = 0;
    return VK_SUCCESS;
}

static void gate_wire(void)
{
    vkGetPhysicalDeviceProperties = st_props;
    vkCreateCommandPool = st_pool;
    vkAllocateCommandBuffers = st_cmds;
    vkCreateFence = st_fence;
    vkCreateDescriptorPool = st_dpool;
    vkCreateBuffer = st_buffer;
    vkGetBufferMemoryRequirements = st_buffer_req;
    vkAllocateMemory = st_alloc;
    vkBindBufferMemory = st_bind;
    vkMapMemory = st_map;
    vkFreeMemory = st_free;
    vkDestroyBuffer = st_buffer_destroy;
    vkBeginCommandBuffer = st_begin;
    vkCreateRenderPass = st_rpass;
    vkCreateDescriptorSetLayout = st_dsl;
    vkCreatePipelineLayout = st_layout;
    vkCreateShaderModule = st_shader;
    vkDestroyShaderModule = st_shader_destroy;
    vkCreateComputePipelines = st_compute;
    vkCreateGraphicsPipelines = st_graphics;
    vkDestroyPipeline = st_pipe_destroy;
    vkCreatePipelineCache = st_pcache;
    vkGetPipelineCacheData = st_pcache_data;
}

/* ── Reporting ───────────────────────────────────────────────────────────────────────── */

static int g_checks, g_check_failures, g_props, g_prop_failures;

static void section(const char *title)
{
    printf("\n== %s ==\n", title);
    fflush(stdout);
}

static void verdict(const char *prop, int checks_before, int failures_before)
{
    int bad = g_check_failures - failures_before;

    g_props++;
    if (bad)
        g_prop_failures++;
    printf("      %s: %s (%d check%s, %d failed)\n", bad ? "RED " : "GREEN", prop,
           g_checks - checks_before, (g_checks - checks_before) == 1 ? "" : "s", bad);
    fflush(stdout);
}

static void check(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;

    g_checks++;
    if (!cond)
        g_check_failures++;
    printf("      [%s] %s", cond ? "ok  " : "FAIL", what);
    if (fmt && *fmt) {
        printf(" -- ");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    printf("\n");
    fflush(stdout);
}

static void note(const char *fmt, ...)
{
    va_list ap;

    printf("      note: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

/* ── The world ───────────────────────────────────────────────────────────────────────── */

/* A fresh back end with the device chosen and nothing created, and the knob set. `env` NULL
 * leaves RECOMP_VK_PIPELINE_CACHE unset. */
static void world(const char *env)
{
    memset(&g, 0, sizeof g);
    memset(&S, 0, sizeof S);
    cur_vendor = DEV_VENDOR;
    cur_device = DEV_ID;
    g.dev = HNEW(VkDevice);
    g.pd = HNEW(VkPhysicalDevice);
    g.queue = HNEW(VkQueue);
    g.mp.memoryTypeCount = 1;
    g.mp.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    g.depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    g.atom = 64;
    set_env("RECOMP_VK_PIPELINE_CACHE", env);
}

/* The back end's own pipeline-object creation (the load and the cache's creation) and then the two
 * lines of nv2a_vk_backend_install that follow it. */
static int bring_up(void)
{
    int rc = vb_create_pipeline_objects();

    if (rc == 0) {
        g.ok = 1;
        g.pc_last_ns = vb_now_ns();
    }
    return rc;
}

static void elapse(void)
{
    g.pc_last_ns = vb_now_ns() - 11ull * 1000000000ull;
}

static VbPipeKey key_n(unsigned i)
{
    VbPipeKey k;

    memset(&k, 0, sizeof k);
    k.wmask = 1u + i;
    return k;
}

static VkPipeline make_pipe(unsigned i)
{
    VbPipeKey k = key_n(i);

    return vb_pipeline(&k, NULL);
}

static void stats(Nv2aVkPipelineStats *s)
{
    nv2a_vk_backend_pipeline_stats(s);
}

/* Does the file at `path` hold the specification's layout around exactly `blob`? */
static int file_is(const char *path, const uint8_t *blob, size_t blob_n)
{
    size_t n;
    uint8_t *b = read_file(path, &n);
    int ok;

    if (!b)
        return 0;
    ok = n == PC_HDR + blob_n && memcmp(b, vb_pc_magic, 8) == 0 && get32(b + 8) == VB_PCACHE_VER
      && get32(b + 12) == blob_n && get32(b + 16) == ref_crc32(b + PC_HDR, blob_n)
      && memcmp(b + PC_HDR, blob, blob_n) == 0;
    free(b);
    return ok;
}

static void expect_blob_gen(uint8_t *tmp, uint32_t gen)
{
    make_blob(tmp, STUB_BLOB, gen);
}

/* ── The captured stderr ─────────────────────────────────────────────────────────────── */

static size_t log_size(void)
{
    size_t n;
    uint8_t *b;

    fflush(stderr);
    b = read_file(g_log, &n);
    free(b);
    return n;
}

/* Everything written to stderr since `off`, NUL-terminated (malloc'd). */
static char *log_since(size_t off)
{
    size_t n;
    uint8_t *b;
    char *out;

    fflush(stderr);
    b = read_file(g_log, &n);
    if (!b)
        return NULL;
    out = (char *)malloc(n - off + 1u);
    if (out) {
        memcpy(out, b + off, n - off);
        out[n - off] = 0;
    }
    free(b);
    return out;
}

/* The first "[GPUVK] pipeline cache:" line of `seg`, without its line end (malloc'd). */
static char *first_pcache_line(const char *seg)
{
    const char *p = seg ? strstr(seg, "[GPUVK] pipeline cache:") : NULL;
    size_t len;
    char *out;

    if (!p)
        return NULL;
    len = strcspn(p, "\r\n");
    out = (char *)malloc(len + 1u);
    if (out) {
        memcpy(out, p, len);
        out[len] = 0;
    }
    return out;
}

static unsigned count_substr(const char *seg, const char *what)
{
    unsigned n = 0;
    const char *p = seg;

    while (p && (p = strstr(p, what)) != NULL) {
        n++;
        p += strlen(what);
    }
    return n;
}

/* Bring-up with the one decision line captured: returns bring_up()'s result, the line in *line. */
static int bring_up_logged(char **line)
{
    size_t off = log_size();
    int rc = bring_up();
    char *seg = log_since(off);

    *line = first_pcache_line(seg);
    free(seg);
    return rc;
}

static int line_is(const char *line, const char *fmt, ...)
{
    char want[1400];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(want, sizeof want, fmt, ap);
    va_end(ap);
    return line && strcmp(line, want) == 0;
}

static int line_starts(const char *line, const char *fmt, ...)
{
    char want[1400];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(want, sizeof want, fmt, ap);
    va_end(ap);
    return line && strncmp(line, want, strlen(want)) == 0;
}

/* ── P1 / P6: the format, and the saving gate ────────────────────────────────────────── */

static void prop_saving(void)
{
    char path[700], tmpp[720], pidtmp[740], *line;
    uint8_t want[STUB_BLOB], prev[STUB_BLOB];
    Nv2aVkPipelineStats ps;
    unsigned frame;
    int c0 = g_checks, f0 = g_check_failures;

    section("P1/P6 the file format and the saving gate (through vb_flip)");
    check(ref_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u,
          "the fixture's own CRC-32 matches the standard check value", "0xCBF43926");
    path_in(path, sizeof path, "save.bin");
    snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
    snprintf(pidtmp, sizeof pidtmp, "%s.%lu.tmp", path, cur_pid());
    remove(path);
    remove(tmpp);
    remove(pidtmp);

    world(path);
    check(bring_up_logged(&line) == 0, "bring-up with an enabled cache and no file succeeds", "");
    check(line_is(line, "[GPUVK] pipeline cache: %s cold", path),
          "the one load line says: <path> cold", "%s", line ? line : "(none)");
    free(line);
    stats(&ps);
    check(ps.cache_enabled == 1u && ps.cache_rejects == 0u && ps.cache_loaded_bytes == 0u,
          "a missing file is a quiet cold start: enabled, no reject, nothing loaded",
          "enabled=%u rejects=%llu loaded=%llu", ps.cache_enabled,
          (unsigned long long)ps.cache_rejects, (unsigned long long)ps.cache_loaded_bytes);
    check(S.cache_creates == 1u && S.init_size[0] == 0u && S.init_ptr[0] == NULL,
          "the cache is created once, with no initial data", "creates=%u size=%zu", S.cache_creates,
          S.init_size[0]);

    /* A: nothing created yet, interval long elapsed. */
    elapse();
    frame = g.frame;
    vb_flip();
    check(g.frame == frame + 1u, "the flip itself advances the frame", "");
    check(S.data_fetches == 0u && !exists(path), "not dirty: no vkGetPipelineCacheData data call, no file",
          "fetches=%u", S.data_fetches);

    /* B: dirty but the interval has not elapsed (right after install; and 9 s after an attempt).
     * A moved the clock for the not-dirty case; put it back where install leaves it. */
    g.pc_last_ns = vb_now_ns();
    check(make_pipe(0) != VK_NULL_HANDLE, "a pipeline is created", "");
    vb_flip();
    check(S.data_fetches == 0u && !exists(path),
          "dirty, interval not elapsed (just installed): no write", "fetches=%u", S.data_fetches);
    g.pc_last_ns = vb_now_ns() - 9ull * 1000000000ull;
    vb_flip();
    check(S.data_fetches == 0u && !exists(path),
          "dirty, 9 s after the last attempt: still no write", "fetches=%u", S.data_fetches);

    /* C: dirty and elapsed: exactly one write, which re-parses. While the driver is asked for the
     * data the <path>.<pid>.tmp file is already open and the target does not exist yet. */
    S.watch = 1;
    S.watch_path = path;
    S.watch_prev = NULL;
    elapse();
    vb_flip();
    stats(&ps);
    expect_blob_gen(want, 1);
    check(S.data_fetches == 1u && ps.cache_saves == 1u && ps.cache_save_failures == 0u,
          "dirty + elapsed: exactly one data fetch and one counted save", "fetches=%u saves=%llu",
          S.data_fetches, (unsigned long long)ps.cache_saves);
    check(S.watch_calls == 2u && S.watch_tmp_missing == 0u && S.watch_target_bad == 0u,
          "during the size query and the fetch <path>.<pid>.tmp is already open and the target "
          "does not exist yet", "calls=%u tmp_missing=%u target_bad=%u", S.watch_calls,
          S.watch_tmp_missing, S.watch_target_bad);
    check(file_is(path, want, STUB_BLOB),
          "the saved file is header (magic, version, blob length, independent CRC-32) + the driver's "
          "blob, byte for byte", "");
    check(!exists(tmpp) && !exists(pidtmp), "neither <path>.tmp nor <path>.<pid>.tmp is left behind", "");
    memcpy(prev, want, sizeof prev);

    /* C2: a new pipeline right after the save, with no clock manipulation: the production's own
     * timestamp of the attempt must hold the next one off. */
    check(make_pipe(1) != VK_NULL_HANDLE, "a second pipeline is created", "");
    vb_flip();
    check(S.data_fetches == 1u, "dirty again but within 10 s of the last attempt: no second write",
          "fetches=%u", S.data_fetches);

    /* D: elapsed: the second write REPLACES the first -- while the new image is produced and
     * written, the target still holds the previous complete file, never a partial one. */
    S.watch_prev = prev;
    S.watch_prev_n = STUB_BLOB;
    S.watch_calls = S.watch_tmp_missing = S.watch_target_bad = 0;
    elapse();
    vb_flip();
    stats(&ps);
    expect_blob_gen(want, 2);
    check(S.data_fetches == 2u && ps.cache_saves == 2u,
          "dirty + elapsed again: the second save", "fetches=%u saves=%llu", S.data_fetches,
          (unsigned long long)ps.cache_saves);
    check(S.watch_calls == 2u && S.watch_tmp_missing == 0u && S.watch_target_bad == 0u,
          "while the second image is fetched the target still holds the first complete file and "
          "the .tmp is open", "calls=%u tmp_missing=%u target_bad=%u", S.watch_calls,
          S.watch_tmp_missing, S.watch_target_bad);
    check(file_is(path, want, STUB_BLOB),
          "afterwards the target holds the second blob, whole", "");
    check(!exists(tmpp) && !exists(pidtmp), "no temporary file after the replacement either", "");
    S.watch = 0;

    /* D2: nothing new: elapsed alone does not write. */
    elapse();
    vb_flip();
    stats(&ps);
    check(S.data_fetches == 2u && ps.cache_saves == 2u && file_is(path, want, STUB_BLOB),
          "not dirty since the last good save, interval elapsed: no write and the file is unchanged",
          "fetches=%u", S.data_fetches);
    check(ps.created == 2u, "created counts the two pipelines the driver built", "created=%llu",
          (unsigned long long)ps.created);
    verdict("P1/P6", c0, f0);
    remove(path);
    remove(tmpp);
    remove(pidtmp);
}

/* ── P2 / P3: load and reject ────────────────────────────────────────────────────────── */

/* Load `file` (n bytes) through the production load path and judge it. `accept`: the blob at
 * file + PC_HDR (blob_n bytes) must reach vkCreatePipelineCache as initial data; otherwise it must
 * be rejected: counted once, no initial data, and the back end must keep working. The one load
 * line is checked either way. */
static void load_case(const char *what, const uint8_t *file, size_t n, int accept, size_t blob_n)
{
    char path[700], *line;
    Nv2aVkPipelineStats ps;
    int rc;

    path_in(path, sizeof path, "load.bin");
    remove(path);
    if (!write_file(path, file, n)) {
        check(0, what, "could not write the scratch file");
        return;
    }
    world(path);
    rc = bring_up_logged(&line);
    stats(&ps);
    if (accept) {
        check(rc == 0 && ps.cache_rejects == 0u && ps.cache_loaded_bytes == blob_n
              && S.cache_creates == 1u && S.init_size[0] == blob_n && S.init_ptr[0] != NULL
              && S.init_crc[0] == ref_crc32(file + PC_HDR, blob_n)
              && line_is(line, "[GPUVK] pipeline cache: %s loaded=%zu", path, blob_n),
              what, "accepted: loaded=%llu init=%zu creates=%u rejects=%llu line=%s",
              (unsigned long long)ps.cache_loaded_bytes, S.init_size[0], S.cache_creates,
              (unsigned long long)ps.cache_rejects, line ? line : "(none)");
    } else {
        int ok = rc == 0 && ps.cache_rejects == 1u && ps.cache_loaded_bytes == 0u
              && S.cache_creates == 1u && S.init_size[0] == 0u && S.init_ptr[0] == NULL
              && g.pcache != VK_NULL_HANDLE && ps.cache_enabled == 1u
              && line_starts(line, "[GPUVK] pipeline cache: %s rejected(", path);
        VkPipeline p = VK_NULL_HANDLE;

        check(ok, what, "rejected: rc=%d rejects=%llu loaded=%llu init=%zu creates=%u line=%s", rc,
              (unsigned long long)ps.cache_rejects, (unsigned long long)ps.cache_loaded_bytes,
              S.init_size[0], S.cache_creates, line ? line : "(none)");
        if (rc == 0)
            p = make_pipe(0);
        if (!ok || !p) {
            check(p != VK_NULL_HANDLE, "  the back end still creates a pipeline after the reject", "");
        } else {
            uint8_t want[STUB_BLOB];

            /* The next save replaces the rejected file with a good one. */
            elapse();
            vb_flip();
            stats(&ps);
            expect_blob_gen(want, 1);
            check(g.ok == 1 && ps.cache_saves == 1u && file_is(path, want, STUB_BLOB),
                  "  the back end keeps working and the next save heals the file", "saves=%llu",
                  (unsigned long long)ps.cache_saves);
        }
    }
    free(line);
    remove(path);
}

static void prop_load(void)
{
    uint8_t *f = g_big;
    char path[700];
    Nv2aVkPipelineStats ps;
    size_t n;
    int c0 = g_checks, f0 = g_check_failures;

    section("P2 loading: a missing file, a valid file, the 64 MiB boundary");
    path_in(path, sizeof path, "load.bin");
    remove(path);
    world(path);
    check(bring_up() == 0, "bring-up with a missing file", "");
    stats(&ps);
    check(ps.cache_rejects == 0u && ps.cache_loaded_bytes == 0u && S.init_size[0] == 0u,
          "missing file: cold start, not a reject", "rejects=%llu", (unsigned long long)ps.cache_rejects);

    n = 4096;
    make_blob(f + PC_HDR, n, 7);
    seal(f, n);
    load_case("a valid 4096-byte file is given to vkCreatePipelineCache as initial data", f,
              PC_HDR + n, 1, n);

    n = 32;
    make_blob(f + PC_HDR, n, 7);
    seal(f, n);
    load_case("a blob that is exactly one pipeline cache header (32 bytes) is valid", f, PC_HDR + n,
              1, n);

    n = PC_MAX;
    make_blob(f + PC_HDR, n, 9);
    seal(f, n);
    load_case("a blob of exactly 64 MiB is accepted (only length > 64 MiB is rejected)", f,
              PC_HDR + n, 1, n);
    verdict("P2", c0, f0);
}

static void prop_reject(void)
{
    uint8_t *f = g_big;
    const size_t n = 4096;
    uint8_t blob_ok[4096];
    int c0 = g_checks, f0 = g_check_failures;

    section("P3 every rejection reason, each on a file that breaks that rule only");
    make_blob(blob_ok, n, 3);

    /* 1: shorter than the header. */
    memcpy(f + PC_HDR, blob_ok, n);
    seal(f, n);
    load_case("shorter than the header: 0 bytes", f, 0, 0, 0);
    load_case("shorter than the header: 1 byte", f, 1, 0, 0);
    load_case("shorter than the header: 19 bytes", f, PC_HDR - 1u, 0, 0);

    /* 2: wrong magic. */
    seal(f, n);
    f[0] ^= 1u;
    load_case("wrong magic (first byte)", f, PC_HDR + n, 0, 0);
    seal(f, n);
    f[7] ^= 0x80u;
    load_case("wrong magic (last byte)", f, PC_HDR + n, 0, 0);

    /* 3: wrong version. */
    seal(f, n);
    put32(f + 8, VB_PCACHE_VER + 1u);
    load_case("wrong version (+1)", f, PC_HDR + n, 0, 0);
    seal(f, n);
    put32(f + 8, 0u);
    load_case("wrong version (0)", f, PC_HDR + n, 0, 0);

    /* 4: length over 64 MiB, on a file whose length field, size and CRC all agree. */
    {
        size_t big = (size_t)PC_MAX + 1u;
        make_blob(f + PC_HDR, big, 5);
        seal(f, big);
        load_case("blob length 64 MiB + 1, file size and CRC consistent: over the limit", f,
                  PC_HDR + big, 0, 0);
    }

    /* 5: length unequal to the file size minus the header. */
    make_blob(f + PC_HDR, n, 3);
    seal(f, n);
    put32(f + 12, (uint32_t)n + 1u);
    load_case("length field one more than the blob", f, PC_HDR + n, 0, 0);
    seal(f, n);
    put32(f + 12, (uint32_t)n - 1u);
    load_case("length field one less than the blob", f, PC_HDR + n, 0, 0);
    seal(f, n);
    f[PC_HDR + n] = 0;
    load_case("one trailing byte after the blob", f, PC_HDR + n + 1u, 0, 0);
    seal(f, n);
    load_case("file truncated by one byte", f, PC_HDR + n - 1u, 0, 0);
    seal(f, n);
    put32(f + 12, 0xFFFFFFFFu);
    load_case("length field 0xFFFFFFFF", f, PC_HDR + n, 0, 0);

    /* 6: CRC mismatch (blob damaged, or the stored CRC damaged). */
    seal(f, n);
    f[PC_HDR + 100] ^= 1u;
    load_case("CRC mismatch: one payload bit flipped", f, PC_HDR + n, 0, 0);
    seal(f, n);
    f[16] ^= 1u;
    load_case("CRC mismatch: the stored CRC differs", f, PC_HDR + n, 0, 0);

    /* 7: the blob's own VkPipelineCacheHeaderVersionOne is malformed (CRC and length are right):
     * the Vulkan header check is 32 <= headerSize <= blob length, version one. */
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, 16u);
    seal(f, n);
    load_case("blob header: headerSize 16 (smaller than the 32-byte header)", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, 31u);
    seal(f, n);
    load_case("blob header: headerSize 31", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, 0u);
    seal(f, n);
    load_case("blob header: headerSize 0", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, (uint32_t)n + 1u);
    seal(f, n);
    load_case("blob header: headerSize one more than the blob", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, (uint32_t)n + 1000u);
    seal(f, n);
    load_case("blob header: headerSize 1000 more than the blob", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, 0xFFFFFFFFu);
    seal(f, n);
    load_case("blob header: headerSize 0xFFFFFFFF", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR + 4, 2u);
    seal(f, n);
    load_case("blob header: headerVersion 2", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR + 4, 0u);
    seal(f, n);
    load_case("blob header: headerVersion 0", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, 31, 3);
    seal(f, 31);
    load_case("blob header: the blob is 31 bytes of zeros, shorter than the header", f,
              PC_HDR + 31u, 0, 0);
    {
        /* A 31-byte blob whose first 31 bytes are a plausible header (headerSize 32, version 1,
         * this device's vendor and deviceID, all but the last byte of the UUID). */
        uint8_t full[32];

        make_blob(full, 32, 3);
        memcpy(f + PC_HDR, full, 31);
        seal(f, 31);
        load_case("blob header: 31 bytes with a plausible header (headerSize 32, version 1, matching "
                  "vendor and device)", f, PC_HDR + 31u, 0, 0);
    }
    seal(f, 0);
    load_case("blob header: an empty blob", f, PC_HDR, 0, 0);

    /* 8: another device or driver. */
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR + 8, DEV_VENDOR + 1u);
    seal(f, n);
    load_case("another device: vendorID differs", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR + 12, DEV_ID + 1u);
    seal(f, n);
    load_case("another device: deviceID differs", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    f[PC_HDR + 16] ^= 1u;
    seal(f, n);
    load_case("another driver: pipelineCacheUUID differs (first byte)", f, PC_HDR + n, 0, 0);
    make_blob(f + PC_HDR, n, 3);
    f[PC_HDR + 16 + VK_UUID_SIZE - 1u] ^= 1u;
    seal(f, n);
    load_case("another driver: pipelineCacheUUID differs (last byte)", f, PC_HDR + n, 0, 0);

    /* The controls: files with nothing broken are accepted, so none of the above was rejected
     * merely for being built by this fixture; a headerSize equal to the blob length is the edge
     * of the accepted range. */
    make_blob(f + PC_HDR, n, 3);
    seal(f, n);
    load_case("control: the same file with nothing broken is accepted", f, PC_HDR + n, 1, n);
    make_blob(f + PC_HDR, n, 3);
    put32(f + PC_HDR, (uint32_t)n);
    seal(f, n);
    load_case("control: headerSize equal to the blob length is accepted", f, PC_HDR + n, 1, n);
    verdict("P3", c0, f0);
}

/* ── P4: the driver refuses valid data ───────────────────────────────────────────────── */

static void prop_refusal(void)
{
    uint8_t *f = g_big;
    const size_t n = 4096;
    char path[700], *line;
    Nv2aVkPipelineStats ps;
    int rc;
    int c0 = g_checks, f0 = g_check_failures;

    section("P4 a driver that refuses valid initial data");
    path_in(path, sizeof path, "refuse.bin");
    make_blob(f + PC_HDR, n, 3);
    seal(f, n);
    write_file(path, f, PC_HDR + n);
    world(path);
    S.refuse_initial = VK_ERROR_INITIALIZATION_FAILED;
    rc = bring_up_logged(&line);
    stats(&ps);
    check(rc == 0, "the back end does not fail: bring-up succeeds", "rc=%d", rc);
    check(S.cache_creates == 2u && S.init_size[0] == n && S.init_size[1] == 0u
          && S.init_ptr[1] == NULL,
          "the cache is created twice: with the data, then empty", "creates=%u sizes=%zu,%zu",
          S.cache_creates, S.init_size[0], S.init_size[1]);
    check(ps.cache_rejects == 1u && ps.cache_loaded_bytes == 0u && g.pcache != VK_NULL_HANDLE,
          "the refusal is one counted reject, nothing is counted as loaded, a cache exists",
          "rejects=%llu loaded=%llu", (unsigned long long)ps.cache_rejects,
          (unsigned long long)ps.cache_loaded_bytes);
    check(line_starts(line, "[GPUVK] pipeline cache: %s rejected(", path),
          "the one load line says rejected(...), not loaded=", "%s", line ? line : "(none)");
    free(line);
    check(make_pipe(0) != VK_NULL_HANDLE, "a pipeline can still be created", "");
    remove(path);
    verdict("P4", c0, f0);
}

/* ── P5: the knob ────────────────────────────────────────────────────────────────────── */

static void prop_knob(void)
{
    static const char *const off[] = { "off", "OFF", "Off", "oFf", "0" };
    char sub[700], tmpn[100], pidn[120], *line;
    uint8_t blob[4096], file[PC_HDR + 4096];
    unsigned i;
    Nv2aVkPipelineStats ps;
    int c0 = g_checks, f0 = g_check_failures;

    section("P5 RECOMP_VK_PIPELINE_CACHE: off / OFF / Off / 0, unset, a path, a path too long");
    path_in(sub, sizeof sub, "knob");
    pc_mkdir(sub);
    make_blob(file + PC_HDR, 4096, 3);
    memcpy(blob, file + PC_HDR, sizeof blob);
    seal(file, 4096);
    check(pc_chdir(sub) == 0, "the working directory is the scratch directory", "");

    for (i = 0; i < sizeof off / sizeof off[0]; i++) {
        const char *v = off[i];

        snprintf(tmpn, sizeof tmpn, "%s.tmp", v);
        snprintf(pidn, sizeof pidn, "%s.%lu.tmp", v, cur_pid());
        /* A: no such file; nothing may be created either. */
        remove(v);
        remove(tmpn);
        remove(pidn);
        world(v);
        check(bring_up_logged(&line) == 0, "bring-up", "RECOMP_VK_PIPELINE_CACHE=%s", v);
        check(line_is(line, "[GPUVK] pipeline cache: off"), "the one load line says: off", "=%s: %s", v,
              line ? line : "(none)");
        free(line);
        make_pipe(0);
        elapse();
        vb_flip();
        stats(&ps);
        check(ps.cache_enabled == 0u && ps.cache_saves == 0u && ps.cache_save_failures == 0u
              && ps.cache_rejects == 0u && ps.cache_loaded_bytes == 0u,
              "disabled: enabled=0 and every cache counter stays 0", "=%s", v);
        check(S.data_queries == 0u && S.data_fetches == 0u,
              "disabled: the driver is never asked for the cache data (nothing written)", "=%s", v);
        check(!exists(v) && !exists(tmpn) && !exists(pidn),
              "disabled: the file does not exist afterwards (neither the name nor a temporary)", "=%s", v);
        check(S.cache_creates == 1u && S.init_size[0] == 0u,
              "disabled: the cache is still created, empty", "=%s", v);

        /* B: a valid file of that very name is there; it must be neither read nor touched. */
        write_file(v, file, sizeof file);
        world(v);
        check(bring_up() == 0, "bring-up with a valid file of that name present", "=%s", v);
        make_pipe(0);
        elapse();
        vb_flip();
        stats(&ps);
        check(S.init_size[0] == 0u && S.init_ptr[0] == NULL && ps.cache_loaded_bytes == 0u
              && ps.cache_rejects == 0u,
              "disabled: a file of that name is not read (no initial data, no load, no reject)",
              "=%s", v);
        check(S.data_fetches == 0u && ps.cache_saves == 0u && file_is(v, blob, 4096)
              && !exists(tmpn) && !exists(pidn),
              "disabled: that file is not rewritten either", "=%s", v);
        remove(v);
    }

    /* A path that does not fit the back end's 1024-byte buffer is off, with its own wording; one
     * byte shorter fits. The name is never opened by a disabled cache. */
    {
        char longp[1200];

        memset(longp, 'x', sizeof longp);
        longp[1024] = 0;
        world(longp);
        check(bring_up_logged(&line) == 0, "bring-up with a 1024-byte path", "");
        check(line_is(line, "[GPUVK] pipeline cache: off (path too long)"),
              "a path that does not fit is: off (path too long)", "%s", line ? line : "(none)");
        free(line);
        make_pipe(0);
        elapse();
        vb_flip();
        stats(&ps);
        check(ps.cache_enabled == 0u && S.data_queries == 0u && S.data_fetches == 0u
              && ps.cache_saves == 0u && ps.cache_save_failures == 0u && S.cache_creates == 1u
              && S.init_size[0] == 0u,
              "a path too long is disabled: nothing read, nothing written, the cache still created",
              "enabled=%u", ps.cache_enabled);
        longp[1023] = 0;
        world(longp);
        check(bring_up_logged(&line) == 0, "bring-up with a 1023-byte path", "");
        stats(&ps);
        check(ps.cache_enabled == 1u && ps.cache_rejects == 0u
              && line_starts(line, "[GPUVK] pipeline cache: xxxx") && strstr(line, " cold") != NULL,
              "a path one byte shorter fits: the cache is enabled and the name is used as given", "");
        free(line);
    }

    /* Unset. */
    {
        char app[700], dir[740], defp[900], oldp[800];
        uint8_t want[STUB_BLOB];

        path_in(app, sizeof app, "appdata");
        snprintf(dir, sizeof dir, "%s" PC_SEP "xboxrecomp", app);
        snprintf(defp, sizeof defp, "%s" PC_SEP "vk-pipeline-cache-000010de-00002684.bin", dir);
        snprintf(oldp, sizeof oldp, "%s" PC_SEP "vk-pipeline-cache.bin", dir);
        pc_mkdir(app);
        pc_mkdir(dir);
        write_file(defp, file, sizeof file);
        write_file(oldp, file, sizeof file);
        set_env("LOCALAPPDATA", app);
#if !defined(_WIN32)
        /* Off Windows the default folder is $XDG_CACHE_HOME (else ~/Library/Caches or ~/.cache),
         * so the same device-specific file is looked up under it. */
        set_env("XDG_CACHE_HOME", app);
#endif
        world(NULL);
        check(bring_up_logged(&line) == 0, "bring-up with the knob unset", "");
        stats(&ps);
        check(ps.cache_enabled == 1u && ps.cache_loaded_bytes == 4096u && S.init_size[0] == 4096u
              && line_is(line, "[GPUVK] pipeline cache: %s loaded=4096", defp),
              "unset on Windows: %LOCALAPPDATA%\\xboxrecomp\\vk-pipeline-cache-000010de-00002684.bin "
              "(lowercase hex vendorID, deviceID, 8 digits each) is loaded",
              "enabled=%u loaded=%llu line=%s", ps.cache_enabled,
              (unsigned long long)ps.cache_loaded_bytes, line ? line : "(none)");
        free(line);
        remove(defp);
        make_pipe(0);
        elapse();
        vb_flip();
        expect_blob_gen(want, 1);
        check(file_is(defp, want, STUB_BLOB),
              "unset on Windows: a save writes that same device-specific path", "");
        check(file_is(oldp, blob, 4096), "the old device-less default name is neither read nor touched",
              "");

        /* Another device gets its own file: the first device's file is not loaded for it (it
         * would be rejected as another device's), and a save goes to the second name. */
        {
            char defp2[900];
            uint8_t before[PC_HDR + 4096];
            size_t bn;
            uint8_t *b1;

            snprintf(defp2, sizeof defp2, "%s" PC_SEP "vk-pipeline-cache-00001002-000073bf.bin", dir);
            remove(defp2);
            b1 = read_file(defp, &bn);
            memcpy(before, b1, bn < sizeof before ? bn : sizeof before);
            world(NULL);
            cur_vendor = 0x1002u;
            cur_device = 0x73BFu;
            check(bring_up_logged(&line) == 0, "bring-up as a second device (1002:73bf)", "");
            stats(&ps);
            check(ps.cache_rejects == 0u && ps.cache_loaded_bytes == 0u
                  && line_is(line, "[GPUVK] pipeline cache: %s cold", defp2),
                  "a second device starts cold on ITS OWN default name (the first device's file is "
                  "not looked at)", "rejects=%llu line=%s", (unsigned long long)ps.cache_rejects,
                  line ? line : "(none)");
            free(line);
            make_pipe(0);
            elapse();
            vb_flip();
            check(exists(defp2) && file_is(defp, b1 + PC_HDR, bn - PC_HDR),
                  "its save lands on the second name and the first device's file is untouched", "");
            free(b1);
            remove(defp2);
        }
        remove(defp);
        remove(oldp);
        pc_rmdir(dir);
        pc_rmdir(app);
    }
    verdict("P5", c0, f0);
}

/* ── P7: save failures, the latch, the log cap, the driver's answers ─────────────────── */

static void prop_failure(void)
{
    char blocker[700], path[800], sub[800], pidtmp[900];
    Nv2aVkPipelineStats ps;
    unsigned i;
    int c0 = g_checks, f0 = g_check_failures;

    section("P7 a failed save: counted, never fatal, retried at the next interval, latched off after 3");
    path_in(blocker, sizeof blocker, "blocker");
    remove(blocker);
    write_file(blocker, (const uint8_t *)"x", 1);          /* a FILE where the directory must go */
    snprintf(path, sizeof path, "%s" PC_SEP "sub" PC_SEP "cache.bin", blocker);
    snprintf(sub, sizeof sub, "%s" PC_SEP "sub", blocker);

    world(path);
    check(bring_up() == 0, "bring-up with an unwritable cache path", "");
    stats(&ps);
    check(ps.cache_rejects == 0u && ps.cache_enabled == 1u,
          "the unreadable path is a cold start, not a reject", "");
    make_pipe(0);
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_save_failures == 1u && ps.cache_saves == 0u,
          "dirty + elapsed + unwritable: one failure counted, no save", "failures=%llu saves=%llu",
          (unsigned long long)ps.cache_save_failures, (unsigned long long)ps.cache_saves);
    check(S.data_queries == 0u && S.data_fetches == 0u,
          "the temporary file is opened BEFORE the driver is asked for the data: the failed open "
          "means the driver was never asked", "queries=%u fetches=%u", S.data_queries, S.data_fetches);
    check(g.ok == 1 && !g.failed && g.frame == 1u,
          "the back end is not failed by it (g.ok, no stop, the frame advanced)", "ok=%d failed=%d",
          g.ok, g.failed);
    vb_flip();
    stats(&ps);
    check(ps.cache_save_failures == 1u && g.ok == 1,
          "the failure is not retried within the interval", "failures=%llu",
          (unsigned long long)ps.cache_save_failures);
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_save_failures == 2u && ps.cache_saves == 0u && g.ok == 1,
          "after the interval it IS retried (and fails again: failures 2)", "failures=%llu",
          (unsigned long long)ps.cache_save_failures);
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_save_failures == 3u && g.ok == 1, "the third consecutive failure is counted",
          "failures=%llu", (unsigned long long)ps.cache_save_failures);

    /* The obstacle goes away, but three consecutive failures latched saving off for the run. */
    remove(blocker);
    pc_mkdir(blocker);
    make_pipe(1);
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_saves == 0u && ps.cache_save_failures == 3u && S.data_queries == 0u
          && !exists(path),
          "three consecutive failures latch saving off: with the path writable again and a new "
          "pipeline, nothing is written or even attempted", "saves=%llu failures=%llu",
          (unsigned long long)ps.cache_saves, (unsigned long long)ps.cache_save_failures);
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_saves == 0u && ps.cache_save_failures == 3u && g.ok == 1,
          "and it stays off at later intervals", "");
    remove(path);
    pc_rmdir(sub);
    pc_rmdir(blocker);

    /* A success resets the count: fail, fail, ok, fail, fail, ok never latches. */
    path_in(path, sizeof path, "reset.bin");
    snprintf(pidtmp, sizeof pidtmp, "%s.%lu.tmp", path, cur_pid());
    remove(path);
    world(path);
    bring_up();
    for (i = 0; i < 6u; i++) {
        S.data_fail = (i % 3u) != 2u;
        make_pipe(i);
        elapse();
        vb_flip();
    }
    stats(&ps);
    check(ps.cache_save_failures == 4u && ps.cache_saves == 2u && g.ok == 1,
          "fail, fail, ok, fail, fail, ok: a success resets the consecutive count, so saving is "
          "never latched and both successes are saved", "failures=%llu saves=%llu",
          (unsigned long long)ps.cache_save_failures, (unsigned long long)ps.cache_saves);
    check(!exists(pidtmp), "a failed fetch leaves no temporary file behind", "");
    S.data_fail = 0;
    remove(path);
    remove(pidtmp);

    /* The driver cannot produce the data. */
    path_in(path, sizeof path, "dfail.bin");
    remove(path);
    world(path);
    bring_up();
    make_pipe(0);
    S.data_fail = 1;
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_save_failures == 1u && ps.cache_saves == 0u && g.ok == 1 && !exists(path),
          "vkGetPipelineCacheData failing: a failure is counted, nothing written, back end fine",
          "failures=%llu", (unsigned long long)ps.cache_save_failures);
    S.data_fail = 0;
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_saves == 1u && ps.cache_save_failures == 1u && exists(path),
          "and the next interval saves", "saves=%llu", (unsigned long long)ps.cache_saves);
    remove(path);
    verdict("P7", c0, f0);
}

/* The second vkGetPipelineCacheData call's answer: its returned size is what is written. */
static void prop_size(void)
{
    static const struct { int mode; const char *what; int saves; } cases[] = {
        { 1, "a SMALLER size than the query (3000 of 4096): saved with the returned size", 1 },
        { 2, "VK_INCOMPLETE: a failure, nothing written", 0 },
        { 3, "a size LARGER than the query: a failure, nothing written", 0 },
        { 4, "a size of 0: a failure, nothing written", 0 },
    };
    char path[700], pidtmp[800];
    uint8_t want[STUB_BLOB];
    Nv2aVkPipelineStats ps;
    unsigned i;
    int c0 = g_checks, f0 = g_check_failures;

    section("P7b the driver's answers to the size query and to the data call");
    path_in(path, sizeof path, "size.bin");
    snprintf(pidtmp, sizeof pidtmp, "%s.%lu.tmp", path, cur_pid());
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        remove(path);
        world(path);
        bring_up();
        make_pipe(0);
        S.second = cases[i].mode;
        elapse();
        vb_flip();
        stats(&ps);
        if (cases[i].saves) {
            make_blob(want, 3000, S.blob_gen);
            check(ps.cache_saves == 1u && ps.cache_save_failures == 0u && file_is(path, want, 3000),
                  cases[i].what, "saves=%llu failures=%llu", (unsigned long long)ps.cache_saves,
                  (unsigned long long)ps.cache_save_failures);
        } else {
            check(ps.cache_saves == 0u && ps.cache_save_failures == 1u && !exists(path)
                  && !exists(pidtmp) && g.ok == 1,
                  cases[i].what, "saves=%llu failures=%llu", (unsigned long long)ps.cache_saves,
                  (unsigned long long)ps.cache_save_failures);
            /* One failure is not a latch: the next interval, with a sane driver, saves. */
            S.second = 0;
            elapse();
            vb_flip();
            stats(&ps);
            check(ps.cache_saves == 1u && exists(path), "  and the next interval saves (one failure does "
                  "not latch)", "saves=%llu", (unsigned long long)ps.cache_saves);
        }
        remove(path);
    }
    /* The size query itself reporting 0. */
    remove(path);
    world(path);
    bring_up();
    make_pipe(0);
    S.query_zero = 1;
    elapse();
    vb_flip();
    stats(&ps);
    check(ps.cache_saves == 0u && ps.cache_save_failures == 1u && S.data_fetches == 0u
          && !exists(path) && !exists(pidtmp),
          "a size query reporting 0: a failure, no data call, nothing written", "failures=%llu",
          (unsigned long long)ps.cache_save_failures);
    remove(path);
    verdict("P7b", c0, f0);
}

/* A blob our own loader would reject is never written. */
static void prop_prewrite(void)
{
    static const struct { int bad; const char *what; } cases[] = {
        { 1, "the truncated blob a software ICD returns (20 bytes of a 32-byte header)" },
        { 2, "a blob whose vendorID is another device's" },
        { 3, "a blob whose pipelineCacheUUID is another driver's" },
    };
    uint8_t *f = g_big;
    uint8_t keep[4096];
    char path[700], pidtmp[800];
    Nv2aVkPipelineStats ps;
    unsigned i, fetches;
    int c0 = g_checks, f0 = g_check_failures;

    section("P7c a fetched image the loader would reject is not written, and saving stops");
    path_in(path, sizeof path, "prewrite.bin");
    snprintf(pidtmp, sizeof pidtmp, "%s.%lu.tmp", path, cur_pid());
    make_blob(keep, 4096, 77);
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        memcpy(f + PC_HDR, keep, 4096);
        seal(f, 4096);
        write_file(path, f, PC_HDR + 4096);
        world(path);
        bring_up();
        stats(&ps);
        check(ps.cache_loaded_bytes == 4096u, "an existing valid file is loaded", "");
        make_pipe(0);
        S.bad = cases[i].bad;
        elapse();
        vb_flip();
        stats(&ps);
        check(ps.cache_saves == 0u && ps.cache_save_failures == 1u && g.ok == 1,
              cases[i].what, "saves=%llu failures=%llu", (unsigned long long)ps.cache_saves,
              (unsigned long long)ps.cache_save_failures);
        check(file_is(path, keep, 4096) && !exists(pidtmp),
              "  the existing file is untouched and no temporary is left", "");
        fetches = S.data_fetches;
        S.bad = 0;
        make_pipe(1);
        elapse();
        vb_flip();
        elapse();
        vb_flip();
        stats(&ps);
        check(S.data_fetches == fetches && ps.cache_saves == 0u && ps.cache_save_failures == 1u
              && file_is(path, keep, 4096),
              "  saving is latched off for the run: later intervals do not even ask the driver",
              "fetches %u -> %u", fetches, S.data_fetches);
        remove(path);
    }
    verdict("P7c", c0, f0);
}

/* The log is capped, the counters are not. */
static void prop_logcap(void)
{
    char path[700], *seg;
    Nv2aVkPipelineStats ps;
    size_t off;
    unsigned i;
    int c0 = g_checks, f0 = g_check_failures;

    section("P7d the pipeline cache's log lines are capped at 8; the counters are not");
    path_in(path, sizeof path, "logcap.bin");
    remove(path);
    world(path);
    bring_up();
    off = log_size();
    for (i = 0; i < 18u; i++) {
        S.data_fail = (i % 3u) != 2u;
        make_pipe(i);
        elapse();
        vb_flip();
    }
    seg = log_since(off);
    stats(&ps);
    check(ps.cache_save_failures == 12u && ps.cache_saves == 6u,
          "12 failures and 6 saves are all counted", "failures=%llu saves=%llu",
          (unsigned long long)ps.cache_save_failures, (unsigned long long)ps.cache_saves);
    check(count_substr(seg, "[GPUVK] pipeline cache:") == 8u,
          "but exactly 8 pipeline-cache log lines were printed", "%u",
          count_substr(seg, "[GPUVK] pipeline cache:"));
    free(seg);
    S.data_fail = 0;
    remove(path);
    verdict("P7d", c0, f0);
}

/* ── P8: the table's evictions ───────────────────────────────────────────────────────── */

static void prop_evict(void)
{
    Nv2aVkPipelineStats ps;
    unsigned i;
    int ok = 1;
    int c0 = g_checks, f0 = g_check_failures;

    section("P8 pipeline table evictions (VB_MAX_PIPES = 256)");
    check(VB_MAX_PIPES == 256, "the table holds 256 pipelines", "VB_MAX_PIPES=%d", VB_MAX_PIPES);
    world("off");
    check(bring_up() == 0, "bring-up", "");
    for (i = 0; i < 256u; i++)
        if (!make_pipe(i))
            ok = 0;
    stats(&ps);
    check(ok && ps.created == 256u && ps.evictions == 0u && g.npipes == 256u,
          "256 distinct keys fill the table and evict nothing", "created=%llu evictions=%llu",
          (unsigned long long)ps.created, (unsigned long long)ps.evictions);
    make_pipe(0);
    stats(&ps);
    check(ps.created == 256u && ps.evictions == 0u,
          "asking for a key already in the full table creates and evicts nothing",
          "created=%llu evictions=%llu", (unsigned long long)ps.created,
          (unsigned long long)ps.evictions);
    check(make_pipe(256) != VK_NULL_HANDLE, "the 257th distinct key yields a pipeline", "");
    stats(&ps);
    check(ps.created == 257u && ps.evictions == 1u && g.npipes == 256u,
          "the 257th distinct key evicts exactly one: evictions 0 -> 1", "created=%llu evictions=%llu",
          (unsigned long long)ps.created, (unsigned long long)ps.evictions);
    note("the evicted pipeline was destroyed: %u vkDestroyPipeline call(s)", S.destroyed);

    /* LRU: key 0 was touched after key 1, so the 257th evicted key 1; key 2 is next. */
    make_pipe(1);
    stats(&ps);
    check(ps.created == 258u && ps.evictions == 2u,
          "the least recently used key (1) was the one evicted: asking for it again creates and evicts",
          "created=%llu evictions=%llu", (unsigned long long)ps.created,
          (unsigned long long)ps.evictions);
    make_pipe(0);
    stats(&ps);
    check(ps.created == 258u && ps.evictions == 2u,
          "the key that was touched (0) survived: a hit, no creation, no eviction",
          "created=%llu evictions=%llu", (unsigned long long)ps.created,
          (unsigned long long)ps.evictions);
    make_pipe(2);
    stats(&ps);
    check(ps.created == 259u && ps.evictions == 3u,
          "the next least recently used key (2) was evicted by the one before: one eviction per miss",
          "created=%llu evictions=%llu", (unsigned long long)ps.created,
          (unsigned long long)ps.evictions);
    verdict("P8", c0, f0);
}

/* ── P9: the [GPUVK] line ────────────────────────────────────────────────────────────── */

static char *last_stats_line(void)
{
    size_t n;
    uint8_t *b;
    char *p, *line = NULL, *q;

    fflush(stderr);
    b = read_file(g_log, &n);
    if (!b)
        return NULL;
    p = (char *)realloc(b, n + 1u);
    if (!p)
        return NULL;
    p[n] = 0;
    for (q = strstr(p, "[GPUVK] flips="); q; q = strstr(q + 1, "[GPUVK] flips="))
        line = q;
    if (line) {
        char *e = strchr(line, '\n');
        size_t len = e ? (size_t)(e - line) : strlen(line);
        char *copy = (char *)malloc(len + 1u);

        if (copy) {
            memcpy(copy, line, len);
            copy[len] = 0;
        }
        line = copy;
    }
    free(p);
    return line;
}

static void prop_counters(void)
{
    Nv2aVkPipelineStats ps;
    char *line;
    unsigned long long ev = 0, ld = 0, sv = 0, sf = 0, rj = 0;
    const char *at;
    int c0 = g_checks, f0 = g_check_failures;

    section("P9 the counters: nv2a_vk_backend_pipeline_stats() and the [GPUVK] line");
    nv2a_vk_backend_pipeline_stats(NULL);
    check(1, "a NULL out is accepted", "");
    world("off");
    bring_up();
    /* Distinct sentinels, so a counter printed under the wrong name cannot pass. */
    g.st.pipe_evictions = 11;
    g.st.pcache_loaded_bytes = 22;
    g.st.pcache_saves = 33;
    g.st.pcache_save_failures = 44;
    g.st.pcache_rejects = 55;
    g.st.pipelines = 66;
    stats(&ps);
    check(ps.evictions == 11u && ps.cache_loaded_bytes == 22u && ps.cache_saves == 33u
          && ps.cache_save_failures == 44u && ps.cache_rejects == 55u && ps.created == 66u
          && ps.cache_enabled == 0u,
          "the accessor reports each counter under its own field", "");
    vb_stats(1);
    line = last_stats_line();
    check(line != NULL, "a [GPUVK] flips= line was printed", "");
    at = line ? strstr(line, "pipe_evictions=") : NULL;
    check(at != NULL
          && sscanf(at, "pipe_evictions=%llu pcache_loaded=%llu pcache_saves=%llu "
                        "pcache_save_failures=%llu pcache_rejects=%llu", &ev, &ld, &sv, &sf, &rj) == 5,
          "the line carries pipe_evictions= pcache_loaded= pcache_saves= pcache_save_failures= "
          "pcache_rejects= in that order", "%s", at ? at : "(absent)");
    check(ev == 11u && ld == 22u && sv == 33u && sf == 44u && rj == 55u,
          "and each prints its own counter", "%llu %llu %llu %llu %llu", ev, ld, sv, sf, rj);
    free(line);
    verdict("P9", c0, f0);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ───────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return 0;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    (void)backend;
}

int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    (void)tex; (void)argb_out;
    return 0;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    return tex ? tex->pitch * tex->height : 0u;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* ── The run ─────────────────────────────────────────────────────────────────────────── */

static void dump_log(void)
{
    size_t n;
    uint8_t *b;

    fflush(stderr);
    b = read_file(g_log, &n);
    if (b) {
        char *p = (char *)b, *e;

        printf("---- captured stderr (the long [GPUVK] flips= lines are left out) ----\n");
        while (p < (char *)b + n) {
            e = memchr(p, '\n', (size_t)((char *)b + n - p));
            if (!e)
                e = (char *)b + n;
            if (strncmp(p, "[GPUVK] flips=", 14) != 0)
                printf("%.*s\n", (int)(e - p), p);
            p = e + 1;
        }
        printf("----\n");
        free(b);
    }
}

int main(void)
{
    const char *tmp = getenv("TEMP");

    if (!tmp || !*tmp)
        tmp = getenv("TMPDIR");
    if (!tmp || !*tmp)
        tmp = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s" PC_SEP "vk-pcache-gate-%lu", tmp,
#if defined(_WIN32)
             (unsigned long)GetCurrentProcessId());
#else
             (unsigned long)getpid());
#endif
    pc_mkdir(g_dir);
    snprintf(g_log, sizeof g_log, "%s" PC_SEP "stderr.log", g_dir);

    printf("vk_pcache_gate: the persisted VkPipelineCache and the pipeline table's evictions, decided "
           "against the\nspecification, through the toolkit's own nv2a_vk_backend.c with stubbed "
           "Vulkan\n");
    if (!freopen(g_log, "w", stderr)) {
        fprintf(stderr, "FAIL: vk_pcache_gate: stderr could not be captured\n");
        return 1;
    }
    g_big = (uint8_t *)malloc((size_t)PC_MAX + 1u + PC_HDR + 64u);
    if (!g_big) {
        printf("FAIL: vk_pcache_gate: the fixture could not be allocated\n");
        return 1;
    }
    gate_wire();

    prop_saving();
    prop_load();
    prop_reject();
    prop_refusal();
    prop_knob();
    prop_failure();
    prop_size();
    prop_prewrite();
    prop_logcap();
    prop_evict();
    prop_counters();

    /* Leave the scratch directory as found. */
    {
        char p[800];

        (void)pc_chdir(tmp);
        path_in(p, sizeof p, "knob");
        pc_rmdir(p);
        if (!g_check_failures) {
            remove(g_log);
            pc_rmdir(g_dir);
        }
    }
    free(g_big);

    printf("\n");
    if (g_prop_failures) {
        dump_log();
        printf("FAIL: vk_pcache_gate: %d of %d properties failed (%d of %d checks failed)\n",
               g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_pcache_gate: %d properties, %d checks -- a valid cache file seeds the driver's "
           "cache, every damaged or foreign file is rejected and counted, off writes and reads "
           "nothing, saves are gated on dirtiness and the interval and survive failures, and the "
           "257th pipeline evicts exactly one\n", g_props, g_checks);
    return 0;
}
