#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>

#include <esp_log.h>
#include <esp_partition.h>

#include "usb/UsbMassStorageRuntime.h"

namespace {

constexpr uint32_t kUsbMscBlockSize = 512;
constexpr uint32_t kSectorBytes = 4096;
constexpr char kUsbMscPartitionLabel[] = "usbmsc";

constexpr char kUsbMscVendorId[] = "ESP32";
constexpr char kUsbMscProductId[] = "USB_MSC";
constexpr char kUsbMscProductRevision[] = "1.0";
constexpr char kUsbMscLogTag[] = "USB_MSC";

USBMSC g_usb_msc;
const esp_partition_t* g_msc_partition = nullptr;
uint32_t g_msc_blocks = 0;
bool g_msc_ready = false;

uint32_t alignDown(uint32_t value, uint32_t align) {
    return value & ~(align - 1U);
}

uint32_t alignUp(uint32_t value, uint32_t align) {
    return (value + align - 1U) & ~(align - 1U);
}

bool isInRange(uint32_t offset, uint32_t size) {
    if (g_msc_partition == nullptr) {
        return false;
    }

    if (offset > g_msc_partition->size) {
        return false;
    }

    return (static_cast<uint64_t>(offset) + size) <= g_msc_partition->size;
}

bool eraseRange(uint32_t offset, uint32_t size) {
    if (!isInRange(offset, size)) {
        return false;
    }

    const uint32_t aligned_offset = alignDown(offset, kSectorBytes);
    const uint32_t aligned_size = alignUp(size, kSectorBytes);

    const uint64_t partition_end = g_msc_partition->size;
    if (aligned_offset >= partition_end) {
        return false;
    }

    if (aligned_offset + aligned_size > partition_end) {
        return false;
    }

    const esp_err_t err = esp_partition_erase_range(g_msc_partition, aligned_offset, aligned_size);
    if (err != ESP_OK) {
        ESP_LOGE(kUsbMscLogTag, "erase_range failed off=%lu size=%lu err=%s", aligned_offset, aligned_size, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    (void)power_condition;
    g_msc_ready = start || !load_eject;
    ESP_LOGI(kUsbMscLogTag, "start_stop power_condition=%u start=%d eject=%d", power_condition, static_cast<int>(start),
             static_cast<int>(load_eject));
    return true;
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!g_msc_ready || g_msc_partition == nullptr) {
        return 0;
    }

    if (offset >= kUsbMscBlockSize) {
        return 0;
    }

    const uint64_t block_offset = static_cast<uint64_t>(lba) * kUsbMscBlockSize;
    const uint64_t bytes_offset_64 = block_offset + offset;
    const uint64_t max_writable = g_msc_partition->size;
    if (bytes_offset_64 >= max_writable) {
        return 0;
    }

    const uint32_t bytes_offset = static_cast<uint32_t>(bytes_offset_64);
    const uint32_t max_chunk = static_cast<uint32_t>(max_writable - bytes_offset);
    const uint32_t write_size = (bufsize > max_chunk) ? max_chunk : bufsize;
    if (!isInRange(bytes_offset, write_size)) {
        return 0;
    }

    uint32_t written = 0;
    while (written < write_size) {
        const uint32_t dst_offset = bytes_offset + written;
        const uint32_t sector_start = alignDown(dst_offset, kSectorBytes);
        const uint32_t sector_end = sector_start + kSectorBytes;
        const uint32_t chunk_end = (bytes_offset + write_size < sector_end) ? (bytes_offset + write_size) : sector_end;
        const uint32_t copy_len = chunk_end - dst_offset;
        const uint32_t sector_pos = dst_offset - sector_start;

        uint8_t sector[kSectorBytes];
        const esp_err_t read_err = esp_partition_read(g_msc_partition, sector_start, sector, kSectorBytes);
        if (read_err != ESP_OK) {
            ESP_LOGE(kUsbMscLogTag, "sector read failed addr=%lu err=%s", sector_start, esp_err_to_name(read_err));
            return 0;
        }

        memcpy(sector + sector_pos, buffer + written, copy_len);

        if (!eraseRange(sector_start, kSectorBytes)) {
            return 0;
        }
        const esp_err_t write_err = esp_partition_write(g_msc_partition, sector_start, sector, kSectorBytes);
        if (write_err != ESP_OK) {
            ESP_LOGE(kUsbMscLogTag,
                     "write failed lba=%lu offset=%lu size=%lu err=%s",
                     lba,
                     offset,
                     write_size,
                     esp_err_to_name(write_err));
            return static_cast<int32_t>(written);
        }

        written += copy_len;
    }

    return static_cast<int32_t>(written);
}

int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!g_msc_ready || g_msc_partition == nullptr) {
        return 0;
    }

    if (offset >= kUsbMscBlockSize) {
        return 0;
    }

    const uint64_t block_offset = static_cast<uint64_t>(lba) * kUsbMscBlockSize;
    const uint64_t bytes_offset_64 = block_offset + offset;
    if (bytes_offset_64 >= g_msc_partition->size) {
        return 0;
    }

    const uint32_t bytes_offset = static_cast<uint32_t>(bytes_offset_64);
    const uint32_t max_chunk = static_cast<uint32_t>(g_msc_partition->size - bytes_offset);
    const uint32_t read_size = (bufsize > max_chunk) ? max_chunk : bufsize;

    if (!isInRange(bytes_offset, read_size)) {
        return 0;
    }

    const esp_err_t err = esp_partition_read(g_msc_partition, bytes_offset, buffer, read_size);
    if (err != ESP_OK) {
        ESP_LOGE(kUsbMscLogTag, "read failed lba=%lu offset=%lu size=%lu err=%s", lba, offset, read_size, esp_err_to_name(err));
        return 0;
    }
    return static_cast<int32_t>(read_size);
}

void onUsbEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    (void)arg;
    (void)event_data;

    if (event_base != ARDUINO_USB_EVENTS) {
        return;
    }

    switch (event_id) {
        case ARDUINO_USB_STARTED_EVENT:
            ESP_LOGI(kUsbMscLogTag, "USB plugged");
            break;
        case ARDUINO_USB_STOPPED_EVENT:
            ESP_LOGI(kUsbMscLogTag, "USB unplugged");
            break;
        default:
            break;
    }
}

}  // namespace

namespace usb_msc_runtime {

bool beginUsbMassStorage() {
    g_msc_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, kUsbMscPartitionLabel);
    if (g_msc_partition == nullptr) {
        ESP_LOGE(kUsbMscLogTag, "partition '%s' not found", kUsbMscPartitionLabel);
        return false;
    }

    g_msc_blocks = static_cast<uint32_t>(g_msc_partition->size / kUsbMscBlockSize);
    g_usb_msc.onStartStop(onStartStop);
    g_usb_msc.onRead(onRead);
    g_usb_msc.onWrite(onWrite);
    g_usb_msc.vendorID(kUsbMscVendorId);
    g_usb_msc.productID(kUsbMscProductId);
    g_usb_msc.productRevision(kUsbMscProductRevision);
    g_usb_msc.mediaPresent(true);
    if (!g_usb_msc.begin(g_msc_blocks, kUsbMscBlockSize)) {
        ESP_LOGE(kUsbMscLogTag, "USBMSC begin failed");
        g_msc_partition = nullptr;
        return false;
    }

    USB.onEvent(onUsbEvent);
    USB.begin();
    g_msc_ready = true;
    ESP_LOGI(kUsbMscLogTag,
             "started: blocks=%lu size=%luKB label=%s",
             g_msc_blocks,
             g_msc_partition->size / 1024U,
             g_msc_partition->label);
    return true;
}

}  // namespace usb_msc_runtime
