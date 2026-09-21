/* Included by the external collector after its remote read/symbol helpers.
 * Call only at a debug event: all target threads are stopped. No live polling. */
static unsigned gpu_snapshot_count, gpu_snapshot_dropped;
static DWORD64 gpu_mmio_shadow;
static int gpu_mmio_published;
static LONG gpu_mmio_generation;

static void gpu_word(FILE *file, const char *name, uint32_t va, int comma)
{
    uint32_t word;
    fprintf(file, "%s\"%s\":", comma ? "," : "", name);
    int mmio = va >= 0xFD000000u && va < 0xFE000000u;
    if (mmio && gpu_mmio_shadow && gpu_mmio_published &&
        read_remote(gpu_mmio_shadow + (va - 0xFD000000u), &word, 4))
        fprintf(file, "%u", word);
    else if (!mmio && guest_offset && read_remote(guest_offset + va, &word, 4))
        fprintf(file, "%u", word);
    else fputs("null", file);
}

static void capture_gpu_snapshot(const char *reason, DWORD tid, ULONG_PTR tag)
{
    static const struct { const char *name; uint32_t offset; } registers[] = {
        {"PMC_BOOT_0",0x000000}, {"PMC_INTR_0",0x000100}, {"PMC_INTR_EN_0",0x000140},
        {"PCI_ID",0x001800}, {"PCI_COMMAND",0x001804}, {"PCI_REVISION",0x001808},
        {"PFIFO_INTR_0",0x002100}, {"PFIFO_DMA_PUT",0x003240}, {"PFIFO_DMA_GET",0x003244},
        {"PTIMER_NUMERATOR",0x009200}, {"PTIMER_DENOMINATOR",0x009210},
        {"PTIMER_TIME_0",0x009400}, {"PTIMER_TIME_1",0x009410},
        {"PFB_CSTATUS",0x10020C}, {"PFB_WBC",0x100410},
        {"PGRAPH_INTR",0x400100}, {"PCRTC_INTR",0x600100},
        {"PCRTC_START",0x600800}, {"USER_DMA_PUT",0x800040}, {"USER_DMA_GET",0x800044}
    };
    char path[MAX_PATH];
    uint32_t fixture = 0, begin = 0, end = 0, word;
    LONG available = 0;
    DWORD64 address;
    FILE *file;
    if (gpu_snapshot_count >= 64) { ++gpu_snapshot_dropped; return; }
    snprintf(path, sizeof(path), "%s\\gpu-snapshots.jsonl", out_dir);
    file = fopen(path, "a");
    if (!file) { ++gpu_snapshot_dropped; return; }
    address = symbol_address("g_jsrf_gpu_fixture");
    if (address) read_remote(address, &fixture, 4);
    fprintf(file, "{\"version\":1,\"index\":%u,\"reason\":\"%s\",\"tag\":%llu,"
            "\"tick_ms\":%llu,\"tid\":%lu,\"guest_offset\":%llu,\"fixture\":%s,\"ack_enabled\":",
            gpu_snapshot_count++, reason, (unsigned long long)tag,
            (unsigned long long)GetTickCount64(), tid, (unsigned long long)guest_offset,
            fixture ? "true" : "false");
    address = symbol_address("g_nv2a_ack_enabled");
    if (address && read_remote(address, &word, 4)) fprintf(file, "%s", word ? "true" : "false");
    else fputs("null", file);
    gpu_mmio_shadow = symbol_address("g_nv2a_mmio_snapshot");
    address = symbol_address("g_nv2a_mmio_snapshot_available");
    if (address) read_remote(address, &available, sizeof(available));
    address = symbol_address("g_nv2a_mmio_snapshot_generation");
    gpu_mmio_generation = 1;
    if (address) read_remote(address, &gpu_mmio_generation, sizeof(gpu_mmio_generation));
    gpu_mmio_published = available && !(gpu_mmio_generation & 1);
    fprintf(file, ",\"model_snapshot_available\":%s,\"model_snapshot_generation\":%ld",
            gpu_mmio_published ? "true" : "false", gpu_mmio_generation);
    fputs(",\"registers\":{", file);
    for (unsigned i=0; i<sizeof(registers)/sizeof(registers[0]); ++i)
        gpu_word(file, registers[i].name, 0xFD000000u+registers[i].offset, i != 0);
    fputs("},\"device\":{\"va\":1683968", file); /* 0x0019B200 */
    gpu_word(file, "write_cursor", 0x19B200, 1);
    gpu_word(file, "push_begin", 0x19B224, 1);
    gpu_word(file, "push_end", 0x19B228, 1);
    fputs("},\"push_preview\":[", file);
    if (guest_offset && read_remote(guest_offset+0x19B224, &begin, 4) &&
        read_remote(guest_offset+0x19B228, &end, 4) && !(begin & 3) &&
        begin >= 0x80000000u && begin < end && end <= 0x84000000u) {
        for (unsigned i=0; i<64 && (uint64_t)begin+4*i+4<=end; ++i) {
            if (i) fputc(',', file);
            if (read_remote(guest_offset+begin+4*i, &word, 4)) fprintf(file, "%u", word);
            else fputs("null", file);
        }
    }
    fputs("]}\n", file);
    fclose(file);
}

static void capture_gpu_event(DWORD tid, ULONG_PTR tag)
{
    DWORD64 address;
    uint64_t offset;
    if (!SymInitialize(process, out_dir, TRUE)) { ++gpu_snapshot_dropped; return; }
    address = symbol_address("g_xbox_mem_offset");
    if (address && read_remote(address, &offset, sizeof(offset))) guest_offset = offset;
    capture_gpu_snapshot("checkpoint", tid, tag);
    SymCleanup(process);
}
