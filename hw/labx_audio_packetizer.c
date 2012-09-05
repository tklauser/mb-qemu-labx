
/*
 * QEMU model of the LabX audio packetizer.
 *
 * Copyright (c) 2010 Lab X Technologies, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "sysbus.h"
#include "sysemu.h"

#define min_bits qemu_fls
#define RAM_INDEX(addr, size) (((addr)>>2)&((1<<min_bits((size)-1))-1))

struct clock_domain_info
{
    uint32_t tsInterval;
    uint32_t domainEnabled;
};

struct audio_packetizer
{
    SysBusDevice busdev;

    MemoryRegion  mmio_packetizer;
    MemoryRegion  mmio_clock_domain;
    MemoryRegion  mmio_template;
    MemoryRegion  mmio_microcode;

    /* Device Configuration */
    uint32_t baseAddress;
    uint32_t clockDomains;
    uint32_t cacheDataWords;
    uint32_t templateWords;
    uint32_t microcodeWords;
    uint32_t shaperFractionBits;
    uint32_t maxStreamSlots;
    uint32_t dualOutput;

    /* IRQ */
    qemu_irq irq;

    /* Values set by drivers */
    uint32_t tsOffset;
    uint32_t sendSlope;
    uint32_t idleSlope;

    /* Microcode buffer */
    uint32_t* microcodeRam;

    /* Template buffer */
    uint32_t* templateRam;

    /* Clock domain information */
    struct clock_domain_info *clockDomainInfo;
};

/*
 * Packetizer registers
 */
static uint64_t packetizer_regs_read(void *opaque, target_phys_addr_t addr, unsigned int size)
{
    struct audio_packetizer *p = opaque;

    uint32_t retval = 0;
    
    switch ((addr>>2) & 0xFF)
    {
        case 0x00: // control
            break;

        case 0x01: // start vector
            break;

        case 0x02: // ts offset
            break;

        case 0x03: // irq mask
            break;

        case 0x04: // irq flags
            break;

        case 0x05: // sync reg
            break;

        case 0x06: // send slope
            break;

        case 0x07: // idle slope
            break;

        case 0xFD: // capabilities a
            retval = (p->maxStreamSlots & 0x7F) | ((p->dualOutput) ? 0x80 : 0x00);
            break;

        case 0xFE: // capabilities b
            retval = ((p->shaperFractionBits & 0x7F) << 24) |
                     ((p->clockDomains & 0xFF) << 16) |
                     ((min_bits(p->templateWords-1) & 0xFF) << 8) |
                     ((min_bits(p->microcodeWords-1) & 0xFF));
            break;

        case 0xFF: // revision
            retval = 0x00000013;
            break;

        default:
            printf("labx-audio-packetizer: Read of unknown register %08X\n", addr);
            break;
    }

    return retval;
}

static void packetizer_regs_write(void *opaque, target_phys_addr_t addr, uint64_t val64, unsigned int size)
{
    struct audio_packetizer *p = opaque;
    uint32_t value = val64;

    switch ((addr>>2) & 0xFF)
    {
        case 0x00: // control
            break;

        case 0x01: // start vector
            break;

        case 0x02: // ts offset
	    p->tsOffset = value;
            break;

        case 0x03: // irq mask
            break;

        case 0x04: // irq flags
            break;

        case 0x05: // sync reg
            break;

        case 0x06: // send slope
	    p->sendSlope = value;
            break;

        case 0x07: // idle slope
	    p->idleSlope = value;
            break;

        case 0xFD: // capabilities a
            break;

        case 0xFE: // capabilities b
            break;

        case 0xFF: // revision
            break;

        default:
            printf("labx-audio-packetizer: Write of unknown register %08X = %08X\n", addr, value);
            break;
    }
}

static const MemoryRegionOps packetizer_regs_ops = {
    .read = packetizer_regs_read,
    .write = packetizer_regs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};


/*
 * Clock domain registers
 */
static uint64_t clock_domain_regs_read(void *opaque, target_phys_addr_t addr, unsigned int size)
{
    struct audio_packetizer *p = opaque;

    uint32_t retval = 0;
    int domain = (addr>>3) & ((1<<min_bits(p->clockDomains-1))-1);

    switch ((addr>>2)&0x01)
    {
        case 0x00: // ts interval
            retval = p->clockDomainInfo[domain].tsInterval;
            break;

        case 0x01: // domain enable
            retval = p->clockDomainInfo[domain].domainEnabled;
            break;

        default:
            break;
    }

    return retval;
}

static void clock_domain_regs_write(void *opaque, target_phys_addr_t addr, uint64_t val64, unsigned int size)
{
    struct audio_packetizer *p = opaque;
    uint32_t value = val64;

    int domain = (addr>>3) & ((1<<min_bits(p->clockDomains-1))-1);

    switch ((addr>>2)&0x01)
    {
        case 0x00: // ts interval
            p->clockDomainInfo[domain].tsInterval = value;
            break;

        case 0x01: // domain enable
            p->clockDomainInfo[domain].domainEnabled = value;
            break;

        default:
            break;
    }
}

static const MemoryRegionOps clock_domain_regs_ops = {
    .read = clock_domain_regs_read,
    .write = clock_domain_regs_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};


/*
 * Template RAM
 */
static uint64_t template_ram_read(void *opaque, target_phys_addr_t addr, unsigned int size) {
    struct audio_packetizer *p = opaque;

    return p->templateRam[RAM_INDEX(addr, p->templateWords)];
}

static void template_ram_write(void *opaque, target_phys_addr_t addr, uint64_t val64, unsigned int size) {
    struct audio_packetizer *p = opaque;
    uint32_t value = val64;

    p->templateRam[RAM_INDEX(addr, p->templateWords)] = value;
}

static const MemoryRegionOps template_ram_ops = {
    .read = template_ram_read,
    .write = template_ram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};


/*
 * Microcode RAM
 */
static uint64_t microcode_ram_read(void *opaque, target_phys_addr_t addr, unsigned int size) {
    struct audio_packetizer *p = opaque;

    return p->microcodeRam[RAM_INDEX(addr, p->microcodeWords)];
}

static void microcode_ram_write(void *opaque, target_phys_addr_t addr, uint64_t val64, unsigned int size) {
    struct audio_packetizer *p = opaque;
    uint32_t value = val64;

    p->microcodeRam[RAM_INDEX(addr, p->microcodeWords)] = value;
}

static const MemoryRegionOps microcode_ram_ops = {
    .read = microcode_ram_read,
    .write = microcode_ram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static int labx_audio_packetizer_init(SysBusDevice *dev) {
    struct audio_packetizer *p = FROM_SYSBUS(typeof (*p), dev);

    /* Initialize defaults */
    p->tsOffset = 0x00000000;
    p->sendSlope = 0x00000000;
    p->idleSlope = 0x00000000;
    p->templateRam = g_malloc0(p->templateWords*4);
    p->microcodeRam = g_malloc0(p->microcodeWords*4);
    p->clockDomainInfo = g_malloc0(sizeof(struct clock_domain_info)*p->clockDomains);

    /* Set up the IRQ */
    sysbus_init_irq(dev, &p->irq);

    /* Set up memory regions */
    memory_region_init_io(&p->mmio_packetizer,   &packetizer_regs_ops,   p, "labx,audio-packetizer-regs",      0x100 * 4);
    memory_region_init_io(&p->mmio_clock_domain, &clock_domain_regs_ops, p, "labx,audio-packetizer-cd-regs",   2 * 4 * p->clockDomains);
    memory_region_init_io(&p->mmio_template,     &template_ram_ops,      p, "labx,audio-packetizer-template",  4 * p->templateWords);
    memory_region_init_io(&p->mmio_microcode,    &microcode_ram_ops,     p, "labx,audio-packetizer-microcode", 4 * p->microcodeWords);

    sysbus_init_mmio(dev, &p->mmio_packetizer);
    sysbus_init_mmio(dev, &p->mmio_clock_domain);
    sysbus_init_mmio(dev, &p->mmio_template);
    sysbus_init_mmio(dev, &p->mmio_microcode);

    sysbus_mmio_map(dev, 0, p->baseAddress);
    sysbus_mmio_map(dev, 1, p->baseAddress + (1 << (min_bits(p->microcodeWords-1)+2)));
    sysbus_mmio_map(dev, 2, p->baseAddress + (2 << (min_bits(p->microcodeWords-1)+2)));
    sysbus_mmio_map(dev, 3, p->baseAddress + (3 << (min_bits(p->microcodeWords-1)+2)));

    return 0;
}

static Property labx_audio_packetizer_properties[] = {
    DEFINE_PROP_UINT32("baseAddress",        struct audio_packetizer, baseAddress,        0),
    DEFINE_PROP_UINT32("clockDomains",       struct audio_packetizer, clockDomains,       1),
    DEFINE_PROP_UINT32("cacheDataWords",     struct audio_packetizer, cacheDataWords,     1024),
    DEFINE_PROP_UINT32("templateWords",      struct audio_packetizer, templateWords,      1024),
    DEFINE_PROP_UINT32("microcodeWords",     struct audio_packetizer, microcodeWords,     1024),
    DEFINE_PROP_UINT32("shaperFractionBits", struct audio_packetizer, shaperFractionBits, 16),
    DEFINE_PROP_UINT32("maxStreamSlots",     struct audio_packetizer, maxStreamSlots,     32),
    DEFINE_PROP_UINT32("dualOutput",         struct audio_packetizer, dualOutput,         1),
    DEFINE_PROP_END_OF_LIST(),
};

static void labx_audio_packetizer_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = labx_audio_packetizer_init;
    dc->props = labx_audio_packetizer_properties;
}

static TypeInfo labx_audio_packetizer_info = {
    .name          = "labx,audio-packetizer",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(struct audio_packetizer),
    .class_init    = labx_audio_packetizer_class_init,
};

static void labx_audio_packetizer_register(void) {
    type_register_static(&labx_audio_packetizer_info);
}

type_init(labx_audio_packetizer_register)

