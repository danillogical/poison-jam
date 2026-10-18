/* The persisted Vulkan pipeline cache on a real driver: what the back end saves, whether it
 * reloads, and whether the driver uses what it reloads.
 *
 * It installs the production back end (like jsrf_vk_sync_paths_gate: it #includes the toolkit's
 * src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so install, vb_pipeline, vb_flip and
 * vb_create_pipeline_objects are the production bodies) with RECOMP_VK_PIPELINE_CACHE naming a
 * private scratch file, on the device RECOMP_VK_DEVICE picks. Every vkCreateGraphicsPipelines the
 * production makes goes through a forwarder that chains a VkPipelineCreationFeedback, so each
 * pipeline reports whether the driver found it in the cache. The driver's own
 * vkGetPipelineCacheData answer is also read directly and judged by this file's own header check
 * (version one, this device, as long as its headerSize).
 *
 *   D1 cold      no file: a quiet cold start; with a pipeline built and a flip, no save is made
 *                before 10 s have passed since install; the pipelines built on the empty cache do
 *                not ALL report the cache-hit bit; the same six keys built again on the warm cache
 *                are the CONTROL that shows whether the driver reports hits at all
 *   A driver whose blob is well-formed (a hardware GPU):
 *   D2 file      the flip writes the specification's layout around the driver's blob (length,
 *                independent CRC-32, a header naming this device), no temporary left
 *   D3 reload    the cache is destroyed and the production pipeline-object creation runs again:
 *                cache_loaded_bytes equals the saved blob's length and nothing is rejected
 *   D4 used      the six pipelines rebuilt after the reload ALL report
 *                VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT
 *   A driver whose blob is malformed (the software ICD returns 20 bytes of a 32-byte header):
 *   D2 refusal   the save refuses to write it: no file and no temporary, exactly one counted save
 *                failure, saving stopped for the run, the back end healthy. The round trip is then
 *                NOT demonstrated, and the case prints that verdict instead of claiming it.
 *
 * `--hw` is the hardware case (jsrf_vk_pcache_device_hw): it skips (77) when the selected device is
 * a CPU device or none can be installed, and otherwise FAILS unless the driver returns a
 * well-formed blob and reports hits, so it always decides D2-D4 on a real GPU. Without `--hw` it
 * is the software-ICD case (jsrf_vk_pcache_device) and takes whichever branch the driver forces.
 * The five toolkit symbols the translation unit cannot link against are defined at the bottom.
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
#define pc_mkdir(p) CreateDirectoryA((p), NULL)
#define pc_rmdir _rmdir
#else
#include <unistd.h>
#define PC_SEP "/"
#define pc_mkdir(p) mkdir((p), 0777)
#define pc_rmdir rmdir
#endif

#define PC_HDR 20u
#define NKEYS  6u

static int g_checks, g_check_failures;

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

static void section(const char *title)
{
    printf("\n== %s ==\n", title);
    fflush(stdout);
}

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

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
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

static unsigned long cur_pid(void)
{
#if defined(_WIN32)
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f)
        fclose(f);
    return f != NULL;
}

/* ── The feedback forwarder ───────────────────────────────────────────────────────────── */

static PFN_vkCreateGraphicsPipelines real_cgp;
static VkPipelineCreationFeedback fb_pipe;
static VkPipelineCreationFeedback fb_stage[2];
static unsigned fb_valid, fb_hit, fb_calls;     /* since fb_reset(): calls, valid answers, hits */

static void fb_reset(void)
{
    fb_valid = fb_hit = fb_calls = 0;
}

static VkResult VKAPI_CALL fwd_cgp(VkDevice d, VkPipelineCache c, uint32_t n,
                                   const VkGraphicsPipelineCreateInfo *ci,
                                   const VkAllocationCallbacks *a, VkPipeline *o)
{
    VkGraphicsPipelineCreateInfo copy = *ci;
    VkPipelineCreationFeedbackCreateInfo fi = { VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO };
    VkResult r;

    memset(&fb_pipe, 0, sizeof fb_pipe);
    memset(fb_stage, 0, sizeof fb_stage);
    fi.pPipelineCreationFeedback = &fb_pipe;
    fi.pipelineStageCreationFeedbackCount = ci->stageCount < 2u ? ci->stageCount : 2u;
    fi.pPipelineStageCreationFeedbacks = fb_stage;
    fi.pNext = copy.pNext;
    copy.pNext = &fi;
    r = real_cgp(d, c, n, &copy, a, o);
    fb_calls++;
    if (r == VK_SUCCESS && (fb_pipe.flags & VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT)) {
        fb_valid++;
        if (fb_pipe.flags & VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT)
            fb_hit++;
    }
    return r;
}

/* Six distinct pipelines, from state combinations the back end really bakes in. */
static VbPipeKey key_n(unsigned i)
{
    VbPipeKey k;

    memset(&k, 0, sizeof k);
    k.wmask = 0xF;
    k.cull = i % 3u;
    k.ccw = (i / 3u) & 1u;
    k.blend = (i & 1u) ? 1u : 0u;
    if (k.blend) {
        k.src = 0x302;
        k.dst = 0x303;
        k.eq = 0x8006;
    }
    k.z_enable = 1;
    k.z_func = 0x203;
    return k;
}

static unsigned make_pipes(unsigned from, unsigned to)
{
    unsigned i, ok = 0;

    for (i = from; i < to; i++) {
        VbPipeKey k = key_n(i);

        if (vb_pipeline(&k, NULL) != VK_NULL_HANDLE)
            ok++;
    }
    return ok;
}

static void elapse(void)
{
    g.pc_last_ns = vb_now_ns() - 11ull * 1000000000ull;
}

int main(int argc, char **argv)
{
    const char *tmp;
    char dir[600], path[700], tmpp[720];
    VkPhysicalDeviceProperties props;
    Nv2aVkPipelineStats ps;
    PFN_vkDestroyPipelineCache destroy_cache;
    PFN_vkDeviceWaitIdle wait_idle;
    uint8_t *file = NULL;
    size_t fn = 0, blob_len = 0;
    uint64_t t_after_first;
    int rc, blob_ok = 0, hw = 0;
    size_t drv_len = 0;
    char pidtmp[760];

    if (argc > 1 && strcmp(argv[1], "--hw") == 0)
        hw = 1;
    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: vk_pcache_device -- %s\n", argv[2]);
        return 77;
    }
    printf("vk_pcache_device: the persisted pipeline cache on a real Vulkan device, decided from the "
           "driver's own\nblob and its own pipeline creation feedback, through the toolkit's "
           "nv2a_vk_backend.c\n");

    tmp = getenv("TEMP");
    if (!tmp || !*tmp)
        tmp = getenv("TMPDIR");
    if (!tmp || !*tmp)
        tmp = "/tmp";
    snprintf(dir, sizeof dir, "%s" PC_SEP "vk-pcache-dev-%lu", tmp,
#if defined(_WIN32)
             (unsigned long)GetCurrentProcessId());
#else
             (unsigned long)getpid());
#endif
    pc_mkdir(dir);
    snprintf(path, sizeof path, "%s" PC_SEP "cache.bin", dir);
    snprintf(tmpp, sizeof tmpp, "%s.tmp", path);
    remove(path);
    remove(tmpp);
#if defined(_WIN32)
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", path);
#else
    setenv("RECOMP_VK_PIPELINE_CACHE", path, 1);
#endif

    rc = nv2a_vk_backend_install();
    if (rc != 0) {
        printf("SKIP: vk_pcache_device -- the Vulkan back end could not be installed on the "
               "configured software device\n"
               "      (no Vulkan loader, no ICD, or an elevated loader that ignores "
               "VK_DRIVER_FILES / VK_ICD_FILENAMES; RECOMP_VK_DEVICE=llvmpipe names the software "
               "device)\n");
        pc_rmdir(dir);
        return 77;
    }
    vkGetPhysicalDeviceProperties(g.pd, &props);
    printf("device: %s (vendor 0x%04X, device 0x%04X, type %d)\n", props.deviceName, props.vendorID,
           props.deviceID, (int)props.deviceType);
    if (hw && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        printf("SKIP: vk_pcache_device --hw -- the selected device is a CPU device (%s); the "
               "hardware case needs a GPU (RECOMP_VK_DEVICE names one)\n", props.deviceName);
        pc_rmdir(dir);
        return 77;
    }
    destroy_cache = (PFN_vkDestroyPipelineCache)vkGetDeviceProcAddr(g.dev, "vkDestroyPipelineCache");
    wait_idle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(g.dev, "vkDeviceWaitIdle");
    if (!destroy_cache || !wait_idle) {
        fprintf(stderr, "FAIL: vk_pcache_device: the device lacks vkDestroyPipelineCache or "
                        "vkDeviceWaitIdle\n");
        return 1;
    }
    real_cgp = vkCreateGraphicsPipelines;
    vkCreateGraphicsPipelines = fwd_cgp;

    section("D1 cold start, and no save before 10 s have passed since install");
    nv2a_vk_backend_pipeline_stats(&ps);
    check(ps.cache_enabled == 1u && ps.cache_loaded_bytes == 0u && ps.cache_rejects == 0u,
          "no file: enabled, nothing loaded, no reject (a quiet cold start)",
          "enabled=%u loaded=%llu rejects=%llu", ps.cache_enabled,
          (unsigned long long)ps.cache_loaded_bytes, (unsigned long long)ps.cache_rejects);
    fb_reset();
    check(make_pipes(0, 1) == 1u, "one pipeline is built by the real driver", "");
    vb_flip();
    t_after_first = vb_now_ns() - g.pc_last_ns;
    nv2a_vk_backend_pipeline_stats(&ps);
    if (t_after_first < 9ull * 1000000000ull) {
        check(ps.cache_saves == 0u && !exists(path),
              "a pipeline exists and a flip happened, but install was under 10 s ago: no save",
              "%.2f s after install", (double)t_after_first / 1e9);
    } else {
        printf("      note: the first pipeline took %.1f s, so the 'no save within 10 s of install' "
               "case is not decidable on this machine\n", (double)t_after_first / 1e9);
    }
    check(make_pipes(1, NKEYS) == NKEYS - 1u, "the other five pipelines are built", "");
    /* A driver may serve a pipeline from a cache of its own (an identical pipeline made a moment
     * ago, an on-disk shader cache), so the cold baseline is "not all six", not "none". */
    check(fb_calls == NKEYS && fb_hit < NKEYS,
          "cold, on the empty cache: the pipelines do not ALL report the pipeline-cache hit bit",
          "creates=%u valid=%u hits=%u", fb_calls, fb_valid, fb_hit);

    /* D4's control: the same keys, built again on the same warm cache. */
    g.npipes = 0;
    fb_reset();
    check(make_pipes(0, NKEYS) == NKEYS, "the same six keys are built again on the warm cache", "");
    printf("      note: control on the warm cache: creates=%u valid=%u hits=%u\n", fb_calls, fb_valid,
           fb_hit);
    {
        int driver_reports = fb_hit == NKEYS;

        printf("      note: %s\n", driver_reports
               ? "the driver reports pipeline-cache hits, so D4 is decided by the hit bit"
               : "the driver reports NO hits even on the warm cache, so the hit bit cannot decide "
                 "whether a reload is used on this driver");

        /* What does THIS driver hand out of vkGetPipelineCacheData? Asked directly, and judged by
         * this file's own header check (version one, this device, as long as its headerSize). */
        {
            PFN_vkGetPipelineCacheData gd =
                (PFN_vkGetPipelineCacheData)vkGetDeviceProcAddr(g.dev, "vkGetPipelineCacheData");
            size_t sz = 0, got, q;
            static uint8_t buf[1u << 22];

            (void)gd(g.dev, g.pcache, &sz, NULL);
            got = sz < sizeof buf ? sz : sizeof buf;
            (void)gd(g.dev, g.pcache, &got, buf);
            drv_len = got;
            blob_ok = got >= 32u && get32(buf) >= 32u && get32(buf) <= got
                   && get32(buf + 4) == (uint32_t)VK_PIPELINE_CACHE_HEADER_VERSION_ONE
                   && get32(buf + 8) == props.vendorID && get32(buf + 12) == props.deviceID
                   && memcmp(buf + 16, props.pipelineCacheUUID, VK_UUID_SIZE) == 0;
            printf("      note: the driver's size query says %zu bytes and its data call returns %zu:",
                   sz, got);
            for (q = 0; q < got && q < 24u; q++)
                printf(" %02X", buf[q]);
            printf("\n      note: that blob %s a complete VkPipelineCacheHeaderVersionOne for this "
                   "device\n", blob_ok ? "carries" : "does NOT carry");
        }
        if (hw)
            check(blob_ok && driver_reports,
                  "hardware case: the driver returns a well-formed blob and reports pipeline-cache hits",
                  "blob_ok=%d reports=%d", blob_ok, driver_reports);

        elapse();
        vb_flip();
        nv2a_vk_backend_pipeline_stats(&ps);
        snprintf(pidtmp, sizeof pidtmp, "%s.%lu.tmp", path, cur_pid());

        if (!blob_ok) {
            /* The driver's own bytes are not a cache the loader would take, so the save refuses to
             * write them: no file, one counted failure, saving stopped for the run, back end fine. */
            section("D2 a driver that returns a malformed blob: the save refuses it");
            check(ps.cache_saves == 0u && ps.cache_save_failures == 1u && !exists(path) && !exists(pidtmp)
                  && g.ok == 1,
                  "dirty + elapsed: nothing is written (no file, no temporary), one save failure is "
                  "counted, the back end is healthy", "saves=%llu failures=%llu",
                  (unsigned long long)ps.cache_saves, (unsigned long long)ps.cache_save_failures);
            g.npipes = 0;
            check(make_pipes(0, NKEYS) == NKEYS, "pipelines can still be built", "");
            elapse();
            vb_flip();
            nv2a_vk_backend_pipeline_stats(&ps);
            check(ps.cache_saves == 0u && ps.cache_save_failures == 1u && !exists(path) && g.ok == 1,
                  "saving is stopped for the run: another dirty interval writes nothing and counts "
                  "no further failure", "saves=%llu failures=%llu", (unsigned long long)ps.cache_saves,
                  (unsigned long long)ps.cache_save_failures);
            printf("VERDICT: on this driver (%s) the persisted cache is REFUSED, not round-tripped: "
                   "its blob is malformed, so no file is written. That the saved bytes reload and are "
                   "USED by a driver is NOT demonstrated here; jsrf_vk_pcache_device_hw decides it "
                   "on hardware.\n", props.deviceName);
        } else {
            check(ps.cache_saves == 1u && ps.cache_save_failures == 0u && exists(path),
                  "once 10 s have passed the flip writes the file", "saves=%llu failures=%llu",
                  (unsigned long long)ps.cache_saves, (unsigned long long)ps.cache_save_failures);
            check(!exists(tmpp) && !exists(pidtmp), "and leaves no temporary file", "");

            section("D2 the file is the specification's layout around the driver's blob");
            file = read_file(path, &fn);
            check(file != NULL && fn > PC_HDR, "the file is readable and longer than the header",
                  "%zu bytes", fn);
            if (file && fn > PC_HDR) {
                const uint8_t *b = file + PC_HDR;

                blob_len = fn - PC_HDR;
                check(get32(file + 12) == blob_len, "the length field is the file size minus 20",
                      "%u vs %zu", get32(file + 12), blob_len);
                check(get32(file + 16) == ref_crc32(b, blob_len),
                      "the CRC-32 field is the independent CRC-32 of the blob", "0x%08X",
                      get32(file + 16));
                check(get32(b + 4) == (uint32_t)VK_PIPELINE_CACHE_HEADER_VERSION_ONE
                      && get32(b + 8) == props.vendorID && get32(b + 12) == props.deviceID
                      && memcmp(b + 16, props.pipelineCacheUUID, VK_UUID_SIZE) == 0,
                      "the blob's own header names this device", "headerSize=%u", get32(b));
                check(blob_len <= (64u << 20), "the blob is within the 64 MiB limit", "%zu bytes",
                      blob_len);
            }

            section("D3/D4 destroy the cache, run the production creation again, and look at what it loaded");
            (void)wait_idle(g.dev);
            destroy_cache(g.dev, g.pcache, NULL);
            g.pcache = VK_NULL_HANDLE;
            /* The counters the reload must set itself. */
            g.st.pcache_loaded_bytes = 0;
            g.st.pcache_rejects = 0;
            rc = vb_create_pipeline_objects();
            nv2a_vk_backend_pipeline_stats(&ps);
            check(rc == 0 && g.pcache != VK_NULL_HANDLE,
                  "the pipeline objects and the cache are created again", "rc=%d", rc);
            check(blob_len != 0u && ps.cache_loaded_bytes == blob_len,
                  "cache_loaded_bytes equals the saved blob's length", "%llu vs %zu",
                  (unsigned long long)ps.cache_loaded_bytes, blob_len);
            check(ps.cache_rejects == 0u, "nothing was rejected and the driver took the data",
                  "rejects=%llu", (unsigned long long)ps.cache_rejects);

            g.npipes = 0;
            fb_reset();
            check(make_pipes(0, NKEYS) == NKEYS, "the six pipelines are built against the reloaded cache",
                  "");
            printf("      note: after the reload from the file: creates=%u valid=%u hits=%u\n",
                   fb_calls, fb_valid, fb_hit);
            if (driver_reports) {
                check(fb_calls == NKEYS && fb_hit == NKEYS,
                      "USED: every pipeline built after the reload reports "
                      "VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT",
                      "hits=%u of %u", fb_hit, fb_calls);
                if (fb_hit == NKEYS && fb_calls == NKEYS)
                    printf("VERDICT: the saved cache reloads from the file and the driver uses it "
                           "(hit bit on %u of %u rebuilt pipelines; loaded == %zu bytes, 0 rejects).\n",
                           fb_hit, fb_calls, blob_len);
                else
                    printf("VERDICT: the reload from the file is NOT shown to be used (hit bit on %u of "
                           "%u rebuilt pipelines).\n", fb_hit, fb_calls);
            } else {
                printf("VERDICT: the saved cache reloads (loaded == %zu bytes, 0 rejects), but this "
                       "driver reports no pipeline-cache hits, so that it is USED is not "
                       "demonstrated.\n", blob_len);
            }
        }
    }
    free(file);

    remove(path);
    remove(tmpp);
    pc_rmdir(dir);
    printf("\n");
    if (g_check_failures) {
        fprintf(stderr, "FAIL: vk_pcache_device: %d of %d checks failed\n", g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_pcache_device: %d checks -- %s\n", g_checks,
           blob_ok ? "the driver's saved cache reloads from the file and the driver uses it (when "
                     "the driver reports hits)"
                   : "this driver's malformed blob was refused before writing (the round trip is "
                     "NOT demonstrated on it)");
    return 0;
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
