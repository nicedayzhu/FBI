#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <3ds.h>
#include <zlib.h>

#include "resources.h"
#include "section.h"
#include "../core/core.h"

#define PATCH_CONTROL_PORT 5002
#define PATCH_DISCOVERY_PORT 5001
#define PATCH_DISCOVERY_REQUEST "CIA_BRIDGE_DISCOVER_V1"
#define PATCH_DISCOVERY_RESPONSE "CIA_BRIDGE_FBI_V2;cia=5000;patch=5002;codecs=raw"
#define PATCH_MAGIC "CBPATCH1"
#define PATCH_HEADER_SIZE 72
#define PATCH_RECORD_SIZE 24
#define PATCH_MAX_PATH 1024
#define PATCH_CACHE_MAGIC "CBCACHE1"
#define PATCH_CACHE_SIZE 40
#define PATCH_HTTP_BUFFER_SIZE (512 * 1024)
#define PATCH_COMPARE_BUFFER_SIZE (128 * 1024)
#define PATCH_QUEUE_MAX_SLOTS 4
#define PATCH_QUEUE_MIN_SLOTS 2

typedef enum {
    PATCH_PARSE_HEADER,
    PATCH_PARSE_RECORD,
    PATCH_PARSE_PATH,
    PATCH_PARSE_DATA,
    PATCH_PARSE_COMPLETE,
} patch_parse_state;

typedef enum {
    PATCH_RECEIVING,
    PATCH_FINISHING_FILES,
} patch_phase;

typedef struct {
    patch_phase phase;
    u64 downloaded;
    u64 downloadTotal;
    u64 rawProcessed;
    u64 rawWritten;
    u64 rawTotal;
    u64 bytesSkipped;
    u32 filesDone;
    u32 fileCount;
    u32 filesSkipped;
} patch_progress;

typedef struct {
    int serverSocket;
    int clientSocket;
    int discoverySocket;
    char* url;

    Handle cancelEvent;
    bool finished;
    Result result;
    LightLock progressLock;
    patch_progress progress;
    u64 downloadMs;
    u64 finishFilesMs;
    u64 fileSetupMs;
    u64 fileCompareMs;
    u64 fileWriteMs;
    u64 fileCloseMs;
    u64 cacheMs;
    u32 writeCalls;
    u32 sdClusterSize;
    bool sdHighSpeed;
    bool sdHighestClock;
    bool sdInfoReady;
    char error[128];

    u64 transferStartMs;
    u64 downloadBase;
    bool forceVerify;
    bool cacheHit;
    bool cacheSaved;

    FS_Archive archive;
    patch_parse_state state;
    u8 pending[PATCH_MAX_PATH + 1];
    size_t pendingHave;
    size_t pendingNeed;

    u64 titleId;
    u32 fileCount;
    u64 totalRaw;
    u64 totalStored;
    u8 bundleId[16];
    u32 headerCrc;
    bool cacheAllowed;
    bool targetRomfsExisted;
    u32 filesDone;
    u32 filesSkipped;
    u64 rawDone;
    u64 storedDone;
    u64 bytesSkipped;

    u16 pathLength;
    u64 fileRawSize;
    u64 fileStoredSize;
    u32 fileCrcExpected;
    u64 fileStoredRead;
    u64 fileRawWritten;
    u64 fileExistingSize;
    u32 fileCrc;
    bool fileSkipped;
    char relativePath[PATCH_MAX_PATH + 1];
    char previousPath[PATCH_MAX_PATH + 1];
    char targetRoot[FILE_PATH_MAX];
    char targetRomfs[FILE_PATH_MAX];
    char cachePath[FILE_PATH_MAX];
    Handle file;
    u8* compareBuffer;
    u8* writeBuffer;
    u8* queueStorage;
    size_t queueSizes[PATCH_QUEUE_MAX_SLOTS];
    u32 queueSlotCount;
    u32 queueReadIndex;
    u32 queueWriteIndex;
    u32 queuedCount;
    u32 inUseCount;
    LightLock queueLock;
    CondVar queueNotEmpty;
    CondVar queueNotFull;
    bool asyncEnabled;
    bool producerDone;
    bool abortWriter;
    Result writerResult;
    Thread writerThread;
} patchinstall_data;

static Result patch_http_check_running(void* userData);

static u16 patch_read_u16(const u8* data) {
    return (u16) data[0] | ((u16) data[1] << 8);
}

static u32 patch_read_u32(const u8* data) {
    return (u32) data[0] | ((u32) data[1] << 8) | ((u32) data[2] << 16) | ((u32) data[3] << 24);
}

static u64 patch_read_u64(const u8* data) {
    return (u64) patch_read_u32(data) | ((u64) patch_read_u32(data + 4) << 32);
}

static void patch_write_u32(u8* data, u32 value) {
    data[0] = (u8) value;
    data[1] = (u8) (value >> 8);
    data[2] = (u8) (value >> 16);
    data[3] = (u8) (value >> 24);
}

static void patch_write_u64(u8* data, u64 value) {
    patch_write_u32(data, (u32) value);
    patch_write_u32(data + 4, (u32) (value >> 32));
}

static void patch_set_error(patchinstall_data* data, const char* message) {
    if(data->error[0] == '\0') {
        snprintf(data->error, sizeof(data->error), "%s", message);
    }
}

static void patch_set_phase(patchinstall_data* data, patch_phase phase) {
    LightLock_Lock(&data->progressLock);
    data->progress.phase = phase;
    LightLock_Unlock(&data->progressLock);
}

static Result patch_make_path(FS_Path* fsPath, u16* utf16, size_t utf16Capacity, const char* path) {
    size_t utf8Length = strlen(path);
    if(utf8Length + 1 > utf16Capacity) {
        return R_APP_OUT_OF_RANGE;
    }

    ssize_t utf16Length = utf8_to_utf16(utf16, (const u8*) path, utf8Length);
    if(utf16Length < 0 || (size_t) utf16Length + 1 > utf16Capacity) {
        return R_APP_BAD_DATA;
    }
    utf16[utf16Length] = 0;
    fsPath->type = PATH_UTF16;
    fsPath->size = ((u32) utf16Length + 1) * sizeof(u16);
    fsPath->data = utf16;
    return 0;
}

static Result patch_open_path(Handle* file, FS_Archive archive, const char* path, u32 openFlags) {
    u16 utf16[FILE_PATH_MAX];
    FS_Path fsPath;
    Result res = patch_make_path(&fsPath, utf16, FILE_PATH_MAX, path);
    if(R_SUCCEEDED(res)) {
        res = FSUSER_OpenFile(file, archive, fsPath, openFlags, 0);
    }
    return res;
}

static Result patch_read_cache(patchinstall_data* data, bool* matches) {
    *matches = false;
    u64 start = osGetTime();
    Handle file = 0;
    Result res = patch_open_path(&file, data->archive, data->cachePath, FS_OPEN_READ);
    if(R_SUCCEEDED(res)) {
        u64 size = 0;
        res = FSFILE_GetSize(file, &size);
        u8 marker[PATCH_CACHE_SIZE] = {0};
        u32 read = 0;
        if(R_SUCCEEDED(res) && size == sizeof(marker)) {
            res = FSFILE_Read(file, &read, 0, marker, sizeof(marker));
        } else if(R_SUCCEEDED(res)) {
            res = R_APP_BAD_DATA;
        }
        Result closeRes = FSFILE_Close(file);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
        if(R_SUCCEEDED(res) && read == sizeof(marker)
           && memcmp(marker, PATCH_CACHE_MAGIC, 8) == 0
           && patch_read_u64(marker + 8) == data->titleId
           && memcmp(marker + 16, data->bundleId, sizeof(data->bundleId)) == 0
           && patch_read_u32(marker + 32) == data->headerCrc
           && patch_read_u32(marker + 36) == crc32(0, marker, 36)) {
            *matches = true;
        }
    }

    // A missing, truncated, or corrupt marker is a normal cache miss. The
    // mandatory invalidation below still proves that the marker is writable
    // before any target file can be changed.
    data->cacheMs += osGetTime() - start;
    return 0;
}

static Result patch_invalidate_cache(patchinstall_data* data) {
    u64 start = osGetTime();
    Handle file = 0;
    Result res = patch_open_path(&file, data->archive, data->cachePath,
                                 FS_OPEN_WRITE | FS_OPEN_CREATE);
    if(R_SUCCEEDED(res)) {
        res = FSFILE_SetSize(file, 0);
        if(R_SUCCEEDED(res)) {
            res = FSFILE_Flush(file);
        }
        Result closeRes = FSFILE_Close(file);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }
    data->cacheMs += osGetTime() - start;
    if(R_FAILED(res)) {
        patch_set_error(data, "Failed to invalidate the patch cache marker.");
    }
    return res;
}

static Result patch_store_cache(patchinstall_data* data) {
    u8 marker[PATCH_CACHE_SIZE] = {0};
    memcpy(marker, PATCH_CACHE_MAGIC, 8);
    patch_write_u64(marker + 8, data->titleId);
    memcpy(marker + 16, data->bundleId, sizeof(data->bundleId));
    patch_write_u32(marker + 32, data->headerCrc);
    patch_write_u32(marker + 36, crc32(0, marker, 36));

    u64 start = osGetTime();
    Handle file = 0;
    Result res = patch_open_path(&file, data->archive, data->cachePath,
                                 FS_OPEN_WRITE | FS_OPEN_CREATE);
    if(R_SUCCEEDED(res)) {
        res = FSFILE_SetSize(file, 0);
        u32 written = 0;
        if(R_SUCCEEDED(res)) {
            res = FSFILE_Write(file, &written, 0, marker, sizeof(marker), FS_WRITE_FLUSH);
            if(R_SUCCEEDED(res) && written != sizeof(marker)) {
                res = R_APP_BAD_DATA;
            }
        }
        Result closeRes = FSFILE_Close(file);
        if(R_SUCCEEDED(res)) {
            res = closeRes;
        }
    }
    data->cacheMs += osGetTime() - start;
    return res;
}

static Result patch_ensure_directory(FS_Archive archive, const char* path) {
    u16 utf16[FILE_PATH_MAX];
    FS_Path fsPath;
    Result res = patch_make_path(&fsPath, utf16, FILE_PATH_MAX, path);
    if(R_FAILED(res)) {
        return res;
    }

    // Existing directories are the common case for updates. Checking first
    // avoids a guaranteed failed CreateDirectory IPC for every reused parent.
    Handle directory = 0;
    if(R_SUCCEEDED(FSUSER_OpenDirectory(&directory, archive, fsPath))) {
        FSDIR_Close(directory);
        return 0;
    }

    return FSUSER_CreateDirectory(archive, fsPath, 0);
}

static bool patch_directory_exists(FS_Archive archive, const char* path) {
    u16 utf16[FILE_PATH_MAX];
    FS_Path fsPath;
    if(R_FAILED(patch_make_path(&fsPath, utf16, FILE_PATH_MAX, path))) {
        return false;
    }
    Handle directory = 0;
    if(R_FAILED(FSUSER_OpenDirectory(&directory, archive, fsPath))) {
        return false;
    }
    FSDIR_Close(directory);
    return true;
}

static Result patch_ensure_parents(FS_Archive archive, char* destination, char* relativePath,
                                   const char* previousPath) {
    // Sorted records usually share parents. Skip the directories already
    // verified for the previous record, comparing only whole path segments.
    char* newParents = relativePath;
    for(size_t i = 0; relativePath[i] != '\0' && relativePath[i] == previousPath[i]; i++) {
        if(relativePath[i] == '/') {
            newParents = relativePath + i + 1;
        }
    }

    Result res = 0;
    for(char* cursor = newParents; *cursor != '\0' && R_SUCCEEDED(res); cursor++) {
        if(*cursor == '/') {
            *cursor = '\0';
            res = patch_ensure_directory(archive, destination);
            *cursor = '/';
        }
    }
    return res;
}

static bool patch_valid_relative_path(const char* path, size_t length) {
    if(length == 0 || path[0] == '/' || path[length - 1] == '/' || strchr(path, '\\') != NULL) {
        return false;
    }

    size_t segmentLength = 0;
    const char* segment = path;
    for(size_t i = 0; i <= length; i++) {
        if(i == length || path[i] == '/') {
            if(segmentLength == 0
               || (segmentLength == 1 && segment[0] == '.')
               || (segmentLength == 2 && segment[0] == '.' && segment[1] == '.')) {
                return false;
            }
            segment = path + i + 1;
            segmentLength = 0;
        } else {
            if((u8) path[i] < 0x20) {
                return false;
            }
            segmentLength++;
        }
    }
    return true;
}

static Result patch_write_file(patchinstall_data* data, const u8* buffer, size_t size) {
    if(size == 0) {
        return 0;
    }
    if(data->fileRawWritten > data->fileRawSize
       || size > data->fileRawSize - data->fileRawWritten) {
        return R_APP_BAD_DATA;
    }

    u32 written = (u32) size;
    if(!data->fileSkipped) {
        // Do not force a physical SD flush for every file. LayeredFS patches
        // often contain thousands of small files, and FS_WRITE_FLUSH
        // serializes each call all the way to the card. LayeredFS does not
        // consume file timestamps, so avoid that redundant update too.
        written = 0;
        const u8* writeSource = buffer;
        if(data->writeBuffer != NULL) {
            // Record headers and paths make the payload cursor inside a queue
            // slot unaligned even though the slot itself is page-aligned.
            // Stage it at a page boundary before the FS IPC mapping.
            memcpy(data->writeBuffer, buffer, size);
            writeSource = data->writeBuffer;
        }
        u64 start = osGetTime();
        Result res = FSFILE_Write(data->file, &written, data->fileRawWritten, writeSource, size, 0);
        data->fileWriteMs += osGetTime() - start;
        data->writeCalls++;
        if(R_FAILED(res) || written != size) {
            patch_set_error(data, "Failed to write a target patch file.");
            return R_FAILED(res) ? res : R_APP_BAD_DATA;
        }
    }

    data->fileRawWritten += written;
    data->fileCrc = crc32(data->fileCrc, buffer, written);
    LightLock_Lock(&data->progressLock);
    data->progress.rawProcessed += written;
    if(!data->fileSkipped) {
        data->progress.rawWritten += written;
    }
    LightLock_Unlock(&data->progressLock);
    return 0;
}

static Result patch_close_current_file(patchinstall_data* data, bool verify) {
    Result res = 0;
    if(data->file != 0) {
        u64 start = osGetTime();
        Result closeRes = FSFILE_Close(data->file);
        data->fileCloseMs += osGetTime() - start;
        data->file = 0;
        if(R_FAILED(closeRes)) {
            patch_set_error(data, "Failed to close a target patch file.");
            res = closeRes;
        }
    }
    if(verify && R_SUCCEEDED(res)) {
        if(data->fileStoredRead != data->fileStoredSize
           || data->fileRawWritten != data->fileRawSize
           || data->fileCrc != data->fileCrcExpected) {
            patch_set_error(data, "A target patch file failed verification.");
            res = R_APP_BAD_DATA;
        }
    }
    return res;
}

static Result patch_prepare_target(patchinstall_data* data) {
    snprintf(data->targetRoot, sizeof(data->targetRoot), "/luma/titles/%016llX", data->titleId);
    snprintf(data->targetRomfs, sizeof(data->targetRomfs), "%s/romfs", data->targetRoot);
    snprintf(data->cachePath, sizeof(data->cachePath), "%s/.cia-bridge-cache", data->targetRoot);
    data->targetRomfsExisted = patch_directory_exists(data->archive, data->targetRomfs);

    const char* directories[] = {
        "/luma",
        "/luma/titles",
        data->targetRoot,
        data->targetRomfs,
    };
    Result res = 0;
    for(u32 i = 0; i < sizeof(directories) / sizeof(directories[0]) && R_SUCCEEDED(res); i++) {
        res = patch_ensure_directory(data->archive, directories[i]);
    }
    if(R_FAILED(res)) {
        patch_set_error(data, "Failed to prepare the target patch directory.");
    }
    return res;
}

static Result patch_parse_header(patchinstall_data* data) {
    const u8* header = data->pending;
    if(memcmp(header, PATCH_MAGIC, 8) != 0
       || patch_read_u32(header + 8) != PATCH_HEADER_SIZE
       || patch_read_u32(header + 12) != 0
       || patch_read_u32(header + 28) != 0
       || patch_read_u32(header + 68) != 0
       || patch_read_u32(header + 64) != crc32(0, header, 64)) {
        patch_set_error(data, "Invalid CBPATCH1 header.");
        return R_APP_BAD_DATA;
    }

    data->titleId = patch_read_u64(header + 16);
    data->fileCount = patch_read_u32(header + 24);
    data->totalRaw = patch_read_u64(header + 32);
    data->totalStored = patch_read_u64(header + 40);
    memcpy(data->bundleId, header + 48, sizeof(data->bundleId));
    data->headerCrc = patch_read_u32(header + 64);
    data->cacheAllowed = false;
    for(u32 i = 0; i < sizeof(data->bundleId); i++) {
        if(data->bundleId[i] != 0) {
            data->cacheAllowed = true;
            break;
        }
    }
    if(data->titleId == 0 || data->fileCount == 0 || data->fileCount > 100000
       || data->totalStored != data->totalRaw) {
        patch_set_error(data, "Invalid title ID or file count.");
        return R_APP_BAD_DATA;
    }

    Result res = patch_prepare_target(data);
    if(R_SUCCEEDED(res)) {
        bool cacheMatches = false;
        if(!data->forceVerify && data->cacheAllowed && data->targetRomfsExisted) {
            res = patch_read_cache(data, &cacheMatches);
        }
        if(R_SUCCEEDED(res) && cacheMatches) {
            data->cacheHit = true;
            data->filesDone = data->fileCount;
            data->filesSkipped = data->fileCount;
            data->rawDone = data->totalRaw;
            data->storedDone = data->totalStored;
            data->bytesSkipped = data->totalRaw;
            data->state = PATCH_PARSE_COMPLETE;
            LightLock_Lock(&data->progressLock);
            data->progress.rawTotal = data->totalRaw;
            data->progress.rawProcessed = data->totalRaw;
            data->progress.fileCount = data->fileCount;
            data->progress.filesDone = data->fileCount;
            data->progress.filesSkipped = data->fileCount;
            data->progress.bytesSkipped = data->totalRaw;
            LightLock_Unlock(&data->progressLock);
            data->cacheSaved = true;
            return 0;
        }
        if(R_SUCCEEDED(res)) {
            // This flushed zero-length marker is the transaction boundary:
            // an interrupted install can never leave an older bundle marked
            // complete after some target files have already changed.
            res = patch_invalidate_cache(data);
        }
    }
    if(R_SUCCEEDED(res)) {
        LightLock_Lock(&data->progressLock);
        data->progress.rawTotal = data->totalRaw;
        data->progress.fileCount = data->fileCount;
        LightLock_Unlock(&data->progressLock);
        data->state = PATCH_PARSE_RECORD;
        data->pendingHave = 0;
        data->pendingNeed = PATCH_RECORD_SIZE;
    }
    return res;
}

static Result patch_parse_record(patchinstall_data* data) {
    data->pathLength = patch_read_u16(data->pending);
    data->fileRawSize = patch_read_u64(data->pending + 4);
    data->fileStoredSize = patch_read_u64(data->pending + 12);
    data->fileCrcExpected = patch_read_u32(data->pending + 20);
    if(data->pathLength == 0 || data->pathLength > PATCH_MAX_PATH
       || data->pending[2] != 0 || data->pending[3] != 0
       || data->fileStoredSize != data->fileRawSize) {
        patch_set_error(data, "Invalid file record.");
        return R_APP_BAD_DATA;
    }
    data->state = PATCH_PARSE_PATH;
    data->pendingHave = 0;
    data->pendingNeed = data->pathLength;
    return 0;
}

static Result patch_existing_file_matches(patchinstall_data* data, u64 existingSize, bool* matches) {
    *matches = false;
    if(existingSize != data->fileRawSize) {
        return 0;
    }

    u64 start = osGetTime();
    u64 offset = 0;
    u32 existingCrc = crc32(0, Z_NULL, 0);
    Result res = 0;
    while(offset < existingSize && R_SUCCEEDED(res)) {
        u32 requested = (u32) ((existingSize - offset) < PATCH_COMPARE_BUFFER_SIZE
                               ? existingSize - offset : PATCH_COMPARE_BUFFER_SIZE);
        u32 read = 0;
        res = FSFILE_Read(data->file, &read, offset, data->compareBuffer, requested);
        if(R_SUCCEEDED(res) && read != requested) {
            res = R_APP_BAD_DATA;
        }
        if(R_SUCCEEDED(res)) {
            existingCrc = crc32(existingCrc, data->compareBuffer, read);
            offset += read;
        }
    }
    data->fileCompareMs += osGetTime() - start;
    if(R_FAILED(res)) {
        patch_set_error(data, "Failed to compare an existing patch file.");
        return res;
    }

    *matches = existingCrc == data->fileCrcExpected;
    return 0;
}

static Result patch_open_record_file(patchinstall_data* data) {
    memcpy(data->relativePath, data->pending, data->pathLength);
    data->relativePath[data->pathLength] = '\0';
    if(!patch_valid_relative_path(data->relativePath, data->pathLength)) {
        patch_set_error(data, "Unsafe path in patch bundle.");
        return R_APP_BAD_DATA;
    }
    if(data->previousPath[0] != '\0' && strcasecmp(data->previousPath, data->relativePath) >= 0) {
        patch_set_error(data, "Patch paths are duplicated or not sorted.");
        return R_APP_BAD_DATA;
    }
    char target[FILE_PATH_MAX] = {'\0'};
    int targetLength = snprintf(target, sizeof(target), "%s/%s", data->targetRomfs, data->relativePath);
    if(targetLength < 0 || targetLength >= (int) sizeof(target)) {
        patch_set_error(data, "Patch path exceeds the 3DS filesystem limit.");
        return R_APP_OUT_OF_RANGE;
    }

    u64 setupStart = osGetTime();
    Result res = patch_ensure_parents(data->archive, target,
                                      target + strlen(data->targetRomfs) + 1,
                                      data->previousPath);
    if(R_FAILED(res)) {
        data->fileSetupMs += osGetTime() - setupStart;
        patch_set_error(data, "Failed to create a destination directory.");
        return res;
    }

    // Open for both reading and writing so an existing file can be compared
    // against the incoming size and CRC before any SD write. This is the
    // file-level equivalent of rsync's quick skip: identical files still have
    // their network payload verified, but are never rewritten.
    u16 utf16[FILE_PATH_MAX];
    FS_Path fsPath;
    res = patch_make_path(&fsPath, utf16, FILE_PATH_MAX, target);
    if(R_SUCCEEDED(res)) {
        res = FSUSER_OpenFile(&data->file, data->archive, fsPath,
                              FS_OPEN_READ | FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
    }
    u64 existingSize = 0;
    if(R_SUCCEEDED(res)) {
        res = FSFILE_GetSize(data->file, &existingSize);
    }
    data->fileSetupMs += osGetTime() - setupStart;
    if(R_FAILED(res)) {
        if(data->file != 0) {
            FSFILE_Close(data->file);
            data->file = 0;
        }
        patch_set_error(data, "Failed to open or inspect a target patch file.");
        return res;
    }

    bool matches = false;
    data->fileExistingSize = existingSize;
    res = patch_existing_file_matches(data, existingSize, &matches);
    if(R_FAILED(res)) {
        patch_close_current_file(data, false);
        return res;
    }

    data->fileSkipped = matches;
    if(matches) {
        res = patch_close_current_file(data, false);
    } else {
        // Match the priority used for the FS session, but keep this a hint:
        // older FS versions may reject per-file priority without affecting
        // the correctness of the transfer.
        FSFILE_SetPriority(data->file, (u32) -16);
    }
    if(R_FAILED(res)) {
        patch_close_current_file(data, false);
        patch_set_error(data, "Failed to prepare a target patch file.");
        return res;
    }

    memcpy(data->previousPath, data->relativePath, data->pathLength + 1);
    data->fileStoredRead = 0;
    data->fileRawWritten = 0;
    data->fileCrc = crc32(0, Z_NULL, 0);
    data->state = PATCH_PARSE_DATA;
    data->pendingHave = 0;
    data->pendingNeed = 0;
    return 0;
}

static Result patch_finish_record(patchinstall_data* data) {
    if(data->fileStoredRead != data->fileStoredSize
       || data->fileRawWritten != data->fileRawSize
       || data->fileCrc != data->fileCrcExpected) {
        patch_close_current_file(data, false);
        patch_set_error(data, "File size or CRC32 verification failed.");
        return R_APP_BAD_DATA;
    }

    Result res = 0;
    if(!data->fileSkipped && data->file != 0) {
        // Let sequential writes grow a new or short file naturally. Calling
        // SetSize up front made FAT allocate the same clusters in a separate
        // 60+ second pass on the reference card. Only truncate an old tail,
        // after the complete incoming file has passed its CRC check.
        // Every successful Write reports its exact byte count, and the record
        // CRC plus fileRawWritten validate the complete incoming payload. A
        // post-write GetSize therefore only repeats information we already
        // know and costs roughly 0.5 seconds per file on the reference card.
        // The sole remaining size operation is truncating an old longer tail.
        if(data->fileExistingSize > data->fileRawSize) {
            u64 start = osGetTime();
            res = FSFILE_SetSize(data->file, data->fileRawSize);
            data->fileSetupMs += osGetTime() - start;
        }
    }
    if(R_SUCCEEDED(res)) {
        res = patch_close_current_file(data, false);
    } else {
        patch_close_current_file(data, false);
    }
    if(R_FAILED(res)) {
        patch_set_error(data, "Failed to finalize a target patch file.");
        return res;
    }
    data->filesDone++;
    if(data->fileSkipped) {
        data->filesSkipped++;
        data->bytesSkipped += data->fileRawSize;
    }
    LightLock_Lock(&data->progressLock);
    data->progress.filesDone = data->filesDone;
    data->progress.filesSkipped = data->filesSkipped;
    data->progress.bytesSkipped = data->bytesSkipped;
    LightLock_Unlock(&data->progressLock);
    data->rawDone += data->fileRawWritten;
    data->storedDone += data->fileStoredRead;
    if(data->filesDone == data->fileCount) {
        if(data->rawDone != data->totalRaw || data->storedDone != data->totalStored) {
            patch_set_error(data, "Bundle totals do not match the records.");
            return R_APP_BAD_DATA;
        }
        data->state = PATCH_PARSE_COMPLETE;
    } else {
        data->state = PATCH_PARSE_RECORD;
        data->pendingHave = 0;
        data->pendingNeed = PATCH_RECORD_SIZE;
    }
    return 0;
}

static Result patch_consume_file_data(patchinstall_data* data, const u8* input, size_t size) {
    Result res = patch_write_file(data, input, size);
    if(R_SUCCEEDED(res)) {
        data->fileStoredRead += size;
    }
    return res;
}

static Result patch_http_callback(void* userData, void* buffer, size_t size) {
    patchinstall_data* data = (patchinstall_data*) userData;
    const u8* cursor = (const u8*) buffer;
    size_t remaining = size;

    while(remaining > 0) {
        if(data->state == PATCH_PARSE_COMPLETE) {
            patch_set_error(data, "Unexpected data after the final record.");
            return R_APP_BAD_DATA;
        }

        if(data->state != PATCH_PARSE_DATA) {
            size_t needed = data->pendingNeed - data->pendingHave;
            size_t take = remaining < needed ? remaining : needed;
            memcpy(data->pending + data->pendingHave, cursor, take);
            data->pendingHave += take;
            cursor += take;
            remaining -= take;
            if(data->pendingHave != data->pendingNeed) {
                continue;
            }

            Result res = 0;
            if(data->state == PATCH_PARSE_HEADER) {
                res = patch_parse_header(data);
            } else if(data->state == PATCH_PARSE_RECORD) {
                res = patch_parse_record(data);
            } else if(data->state == PATCH_PARSE_PATH) {
                res = patch_open_record_file(data);
            }
            if(R_FAILED(res)) {
                return res;
            }
            if(data->state == PATCH_PARSE_DATA && data->fileStoredSize == 0) {
                if(R_FAILED(res = patch_finish_record(data))) {
                    return res;
                }
            }
            continue;
        }

        u64 storedRemaining = data->fileStoredSize - data->fileStoredRead;
        size_t take = storedRemaining < remaining ? (size_t) storedRemaining : remaining;
        Result res = patch_consume_file_data(data, cursor, take);
        if(R_FAILED(res)) {
            return res;
        }
        cursor += take;
        remaining -= take;
        if(data->fileStoredRead == data->fileStoredSize) {
            if(R_FAILED(res = patch_finish_record(data))) {
                return res;
            }
        }
    }
    return 0;
}

static void patch_writer_thread(void* userData) {
    patchinstall_data* data = (patchinstall_data*) userData;
    while(true) {
        LightLock_Lock(&data->queueLock);
        while(data->queuedCount == 0 && !data->producerDone && !data->abortWriter) {
            CondVar_Wait(&data->queueNotEmpty, &data->queueLock);
        }
        if(data->abortWriter || (data->producerDone && data->queuedCount == 0)) {
            LightLock_Unlock(&data->queueLock);
            break;
        }

        u32 slot = data->queueReadIndex;
        size_t size = data->queueSizes[slot];
        data->queueReadIndex = (data->queueReadIndex + 1) % data->queueSlotCount;
        data->queuedCount--;
        LightLock_Unlock(&data->queueLock);

        u8* buffer = data->queueStorage + slot * PATCH_HTTP_BUFFER_SIZE;
        Result res = patch_http_check_running(data);
        if(R_SUCCEEDED(res)) {
            res = patch_http_callback(data, buffer, size);
        }

        LightLock_Lock(&data->queueLock);
        data->inUseCount--;
        if(R_FAILED(res)) {
            data->writerResult = res;
            data->abortWriter = true;
        }
        CondVar_Broadcast(&data->queueNotFull);
        CondVar_Broadcast(&data->queueNotEmpty);
        bool done = data->abortWriter
                    || (data->producerDone && data->queuedCount == 0 && data->inUseCount == 0);
        LightLock_Unlock(&data->queueLock);
        if(done) break;
    }
}

static bool patch_init_async(patchinstall_data* data) {
    LightLock_Init(&data->queueLock);
    CondVar_Init(&data->queueNotEmpty);
    CondVar_Init(&data->queueNotFull);
    u32 slots = PATCH_QUEUE_MAX_SLOTS;
    while(slots >= PATCH_QUEUE_MIN_SLOTS) {
        // Page-align IPC buffers. This avoids partial-page handling when the
        // FS service maps large write buffers from the application process.
        data->queueStorage = (u8*) memalign(0x1000, (size_t) PATCH_HTTP_BUFFER_SIZE * slots);
        if(data->queueStorage != NULL) {
            data->queueSlotCount = slots;
            bool new3ds = false;
            s32 writerCore = 0;
            if(R_SUCCEEDED(APT_CheckNew3DS(&new3ds)) && new3ds) {
                // Keep network/TLS on core 2 and match FBI's proven local CIA
                // data worker placement for the SD writer.
                writerCore = 1;
            }
            data->writerThread = threadCreate(patch_writer_thread, data, 0x20000, 0x18, writerCore, false);
            if(data->writerThread != NULL) {
                data->asyncEnabled = true;
                return true;
            }
            free(data->queueStorage);
            data->queueStorage = NULL;
            data->queueSlotCount = 0;
        }
        slots /= 2;
    }
    return false;
}

static Result patch_queue_callback(void* userData, void* buffer, size_t size) {
    patchinstall_data* data = (patchinstall_data*) userData;
    if(size > PATCH_HTTP_BUFFER_SIZE || data->queueSlotCount < PATCH_QUEUE_MIN_SLOTS) {
        return R_APP_BAD_DATA;
    }
    while(true) {
        LightLock_Lock(&data->queueLock);
        if(R_FAILED(data->writerResult)) {
            Result res = data->writerResult;
            LightLock_Unlock(&data->queueLock);
            return res;
        }
        if(data->abortWriter) {
            LightLock_Unlock(&data->queueLock);
            return R_APP_CANCELLED;
        }
        if(data->inUseCount < data->queueSlotCount) {
            u32 slot = data->queueWriteIndex;
            memcpy(data->queueStorage + slot * PATCH_HTTP_BUFFER_SIZE, buffer, size);
            data->queueSizes[slot] = size;
            data->queueWriteIndex = (data->queueWriteIndex + 1) % data->queueSlotCount;
            data->queuedCount++;
            data->inUseCount++;
            CondVar_Signal(&data->queueNotEmpty);
            LightLock_Unlock(&data->queueLock);
            return 0;
        }
        int waitRes = CondVar_WaitTimeout(&data->queueNotFull, &data->queueLock, 25 * 1000 * 1000LL);
        LightLock_Unlock(&data->queueLock);
        if(waitRes != 0 && R_FAILED(patch_http_check_running(data))) {
            LightLock_Lock(&data->queueLock);
            data->abortWriter = true;
            CondVar_Broadcast(&data->queueNotEmpty);
            CondVar_Broadcast(&data->queueNotFull);
            LightLock_Unlock(&data->queueLock);
            return R_APP_CANCELLED;
        }
    }
}

static Result patch_finish_async(patchinstall_data* data, Result transferResult) {
    LightLock_Lock(&data->queueLock);
    data->producerDone = true;
    if(R_FAILED(transferResult)) {
        data->abortWriter = true;
    }
    CondVar_Broadcast(&data->queueNotEmpty);
    CondVar_Broadcast(&data->queueNotFull);
    LightLock_Unlock(&data->queueLock);

    Result joinResult = threadJoin(data->writerThread, U64_MAX);
    threadFree(data->writerThread);
    data->writerThread = NULL;
    if(R_SUCCEEDED(transferResult)) {
        if(R_FAILED(data->writerResult)) {
            transferResult = data->writerResult;
        } else if(R_FAILED(joinResult)) {
            transferResult = joinResult;
        }
    }
    free(data->queueStorage);
    data->queueStorage = NULL;
    data->queueSlotCount = 0;
    data->asyncEnabled = false;
    return transferResult;
}

static Result patch_http_check_running(void* userData) {
    patchinstall_data* data = (patchinstall_data*) userData;
    if(task_is_quit_all() || (data->cancelEvent != 0 && svcWaitSynchronization(data->cancelEvent, 0) == 0)) {
        return R_APP_CANCELLED;
    }
    return 0;
}

static Result patch_http_progress(void* userData, u64 total, u64 current) {
    patchinstall_data* data = (patchinstall_data*) userData;
    LightLock_Lock(&data->progressLock);
    data->progress.downloadTotal = total > 0 ? total + data->downloadBase : 0;
    data->progress.downloaded = current + data->downloadBase;
    LightLock_Unlock(&data->progressLock);
    return patch_http_check_running(userData);
}

static void patch_worker(void* arg) {
    patchinstall_data* data = (patchinstall_data*) arg;
    Result res = fs_open_archive(&data->archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    u32 previousFsPriority = 0;
    bool restoreFsPriority = false;
    if(R_SUCCEEDED(res)) {
        FS_ArchiveResource sdResource;
        FS_SdMmcSpeedInfo sdSpeed;
        if(R_SUCCEEDED(FSUSER_GetSdmcArchiveResource(&sdResource))
           && R_SUCCEEDED(FSUSER_GetSdmcSpeedInfo(&sdSpeed))) {
            data->sdClusterSize = sdResource.clusterSize;
            data->sdHighSpeed = sdSpeed.highSpeedModeEnabled;
            data->sdHighestClock = sdSpeed.usesHighestClockRate;
            data->sdInfoReady = true;
        }
        if(R_SUCCEEDED(FSUSER_GetPriority(&previousFsPriority))
           && R_SUCCEEDED(FSUSER_SetPriority((u32) -16))) {
            restoreFsPriority = true;
        }
        FSUSER_SetArchivePriority(data->archive, (u32) -16);
        data->state = PATCH_PARSE_HEADER;
        data->pendingHave = 0;
        data->pendingNeed = PATCH_HEADER_SIZE;

        // A complete marker can be decided from the fixed 72-byte header.
        // Requesting it as a real HTTP range lets both sender and receiver
        // finish normally on a cache hit instead of resetting a full GET.
        u64 downloadStart = osGetTime();
        data->downloadBase = 0;
        res = http_download_callback_range(data->url, "0-71", PATCH_HEADER_SIZE, data,
                                           patch_http_callback, patch_http_check_running,
                                           patch_http_progress);
        if(R_SUCCEEDED(res) && !data->cacheHit
           && (data->state != PATCH_PARSE_RECORD || data->pendingHave != 0
               || data->filesDone != 0)) {
            patch_set_error(data, "The header range ended at an invalid boundary.");
            res = R_APP_BAD_DATA;
        }
        if(R_SUCCEEDED(res) && !data->cacheHit) {
            data->compareBuffer = (u8*) memalign(0x1000, PATCH_COMPARE_BUFFER_SIZE);
            if(data->compareBuffer == NULL) {
                patch_set_error(data, "Failed to allocate the comparison buffer.");
                res = R_APP_OUT_OF_MEMORY;
            } else {
                // Optional: correctness does not depend on this allocation.
                // It keeps arbitrary record offsets out of FSFILE_Write's IPC
                // buffer and can be A/B measured on real cards.
                data->writeBuffer = (u8*) memalign(0x1000, PATCH_HTTP_BUFFER_SIZE);
                patch_init_async(data);
                data->downloadBase = PATCH_HEADER_SIZE;
                res = http_download_callback_range(data->url, "72-", PATCH_HTTP_BUFFER_SIZE, data,
                                                    data->asyncEnabled ? patch_queue_callback : patch_http_callback,
                                                    patch_http_check_running, patch_http_progress);
            }
        }
        u64 downloadEnd = osGetTime();
        data->downloadMs = downloadEnd - downloadStart;
        if(data->asyncEnabled) {
            if(R_SUCCEEDED(res)) {
                patch_set_phase(data, PATCH_FINISHING_FILES);
            }
            res = patch_finish_async(data, res);
        }
        if(R_SUCCEEDED(res)) {
            res = patch_http_check_running(data);
        }
        if(R_SUCCEEDED(res) && data->state != PATCH_PARSE_COMPLETE) {
            patch_set_error(data, "Patch bundle was truncated.");
            res = R_APP_BAD_DATA;
        }
        if(R_SUCCEEDED(res) && !data->cacheHit && data->cacheAllowed) {
            data->cacheSaved = R_SUCCEEDED(patch_store_cache(data));
        }
        data->finishFilesMs = osGetTime() - downloadEnd;
        if(R_FAILED(res)) {
            patch_close_current_file(data, false);
        }
        if(restoreFsPriority) {
            FSUSER_SetPriority(previousFsPriority);
        }
        fs_close_archive(data->archive);
    } else {
        patch_set_error(data, "Failed to open the SD card archive.");
    }

    free(data->compareBuffer);
    data->compareBuffer = NULL;
    free(data->writeBuffer);
    data->writeBuffer = NULL;

    LightLock_Lock(&data->progressLock);
    data->result = res;
    data->finished = true;
    LightLock_Unlock(&data->progressLock);
}

static int patch_recv_wait(int socket, void* buffer, size_t size) {
    size_t read = 0;
    while(read < size && !(hidKeysDown() & KEY_B)) {
        int result = recv(socket, (u8*) buffer + read, size - read, 0);
        if(result > 0) {
            read += result;
        } else if(result < 0 && errno == EAGAIN) {
            svcSleepThread(1000000);
        } else {
            return result < 0 ? result : (int) read;
        }
    }
    return (int) read;
}

static void patch_close_client(patchinstall_data* data, u8 status) {
    if(data->clientSocket != 0) {
        send(data->clientSocket, &status, 1, 0);
        close(data->clientSocket);
        data->clientSocket = 0;
    }
}

static void patch_close_listeners(patchinstall_data* data) {
    if(data->discoverySocket != 0) {
        close(data->discoverySocket);
        data->discoverySocket = 0;
    }
    if(data->serverSocket != 0) {
        close(data->serverSocket);
        data->serverSocket = 0;
    }
}

static void patch_free_data(patchinstall_data* data) {
    if(data == NULL) return;
    patch_close_listeners(data);
    patch_close_client(data, R_SUCCEEDED(data->result) ? 0 : 1);
    if(data->cancelEvent != 0) svcCloseHandle(data->cancelEvent);
    if(data->url != NULL) free(data->url);
    free(data);
}

static void patch_progress_update(ui_view* view, void* userData, float* progress, char* text) {
    patchinstall_data* data = (patchinstall_data*) userData;
    LightLock_Lock(&data->progressLock);
    bool finished = data->finished;
    patch_progress current = data->progress;
    LightLock_Unlock(&data->progressLock);

    u64 now = osGetTime();
    u64 elapsed = data->transferStartMs > 0 && now > data->transferStartMs
                  ? now - data->transferStartMs : 0;
    double networkBytesPerSecond = elapsed > 0
                                   ? current.downloaded * 1000.0 / elapsed : 0;
    double writeBytesPerSecond = elapsed > 0
                                 ? current.rawWritten * 1000.0 / elapsed : 0;
    double filesPerSecond = elapsed > 0
                            ? current.filesDone * 1000.0 / elapsed : 0;
    if((hidKeysDown() & KEY_B) && data->cancelEvent != 0) {
        svcSignalEvent(data->cancelEvent);
    }

    if(finished) {
        ui_pop();
        info_destroy(view);
        if(R_SUCCEEDED(data->result)) {
            static char message[320];
            u64 totalMs = data->downloadMs + data->finishFilesMs;
            if(data->cacheHit) {
                snprintf(message, sizeof(message),
                         "Cache: HIT  Files: %lu\nTime: %.1f s\nOnly the 72-byte header was read.\nTarget file reads/writes: 0\nHold X on Yes to verify next time.",
                         data->filesDone, totalMs / 1000.0);
                prompt_display_notify("Patch installed", message, COLOR_TEXT, NULL, NULL, NULL);
                patch_free_data(data);
                return;
            }
            double filesPerSecond = totalMs > 0 ? data->filesDone * 1000.0 / totalMs : 0;
            double effectiveKiB = totalMs > 0 ? data->rawDone * 1000.0 / totalMs / 1024.0 : 0;
            snprintf(message, sizeof(message),
                     "Files: %lu  Skip: %lu\nTime: %.1f s\nRate: %.1f files/s  %.0f KiB/s\nCompare: %.1f s  Setup: %.1f s\nWrite: %.1f s  Calls: %lu\nClose: %.1f s  Saved: %.2f MiB\nCard: %lu KiB HS:%c CLK:%c Cache:%c",
                     data->filesDone, data->filesSkipped,
                     totalMs / 1000.0, filesPerSecond, effectiveKiB,
                     data->fileCompareMs / 1000.0, data->fileSetupMs / 1000.0,
                     data->fileWriteMs / 1000.0, data->writeCalls,
                     data->fileCloseMs / 1000.0, data->bytesSkipped / 1048576.0,
                     data->sdInfoReady ? data->sdClusterSize / 1024 : 0,
                     data->sdInfoReady ? (data->sdHighSpeed ? 'Y' : 'N') : '?',
                     data->sdInfoReady ? (data->sdHighestClock ? 'Y' : 'N') : '?',
                     data->cacheSaved ? 'Y' : 'N');
            prompt_display_notify("Patch installed", message, COLOR_TEXT, NULL, NULL, NULL);
        } else if(data->result == R_APP_CANCELLED) {
            prompt_display_notify("Cancelled",
                                  "Completed files remain updated.\nThe current file may be incomplete.\nResend the patch to finish safely.",
                                  COLOR_TEXT, NULL, NULL, NULL);
        } else {
            error_display_res(NULL, NULL, data->result, "%s", data->error[0] != '\0' ? data->error : "LayeredFS patch failed.");
        }
        patch_free_data(data);
        return;
    }

    const char* phaseText[] = {
        [PATCH_RECEIVING] = "Receiving and installing files...",
        [PATCH_FINISHING_FILES] = "Finishing queued files...",
    };
    if(current.rawTotal > 0) {
        *progress = (float) current.rawProcessed / (float) current.rawTotal;
    } else if(current.fileCount > 0) {
        *progress = (float) current.filesDone / (float) current.fileCount;
    } else {
        *progress = 0;
    }
    snprintf(text, PROGRESS_TEXT_MAX,
             "%s\nFiles: %lu/%lu  %.1f/s  Skip: %lu\nNet: %.2f/%.2f MiB  %.0f KiB/s\nSD: %.2f MiB  %.0f KiB/s",
             phaseText[current.phase], current.filesDone, current.fileCount,
             filesPerSecond, current.filesSkipped,
             current.downloaded / 1048576.0, current.downloadTotal / 1048576.0,
             networkBytesPerSecond / 1024.0,
             current.rawWritten / 1048576.0,
             writeBytesPerSecond / 1024.0);
}

static void patch_confirm_response(ui_view* view, void* userData, u32 response) {
    patchinstall_data* data = (patchinstall_data*) userData;
    if(response != PROMPT_YES) {
        data->result = R_APP_CANCELLED;
        patch_free_data(data);
        return;
    }

    data->finished = false;
    data->result = 0;
    data->downloadMs = 0;
    data->finishFilesMs = 0;
    data->fileSetupMs = 0;
    data->fileCompareMs = 0;
    data->fileWriteMs = 0;
    data->fileCloseMs = 0;
    data->cacheMs = 0;
    data->writeCalls = 0;
    data->filesSkipped = 0;
    data->bytesSkipped = 0;
    data->downloadBase = 0;
    data->cacheHit = false;
    data->cacheSaved = false;
    data->forceVerify = (hidKeysHeld() & KEY_X) != 0;
    data->transferStartMs = osGetTime();
    LightLock_Init(&data->progressLock);
    Result res = svcCreateEvent(&data->cancelEvent, RESET_STICKY);
    if(R_SUCCEEDED(res)) {
        bool new3ds = false;
        s32 core = 1;
        if(R_SUCCEEDED(APT_CheckNew3DS(&new3ds)) && new3ds) {
            core = 2;
        }
        if(threadCreate(patch_worker, data, 0x20000, 0x18, core, true) == NULL) {
            res = R_APP_THREAD_CREATE_FAILED;
        }
    }
    if(R_FAILED(res)) {
        data->result = res;
        data->finished = true;
    }
    info_display("LayeredFS patch", "Press B to cancel.", true, data, patch_progress_update, NULL);
}

static void patch_update_discovery(patchinstall_data* data) {
    if(data->discoverySocket == 0) return;
    for(u32 i = 0; i < 8; i++) {
        char request[sizeof(PATCH_DISCOVERY_REQUEST)] = {'\0'};
        struct sockaddr_in client;
        socklen_t clientLength = sizeof(client);
        int received = recvfrom(data->discoverySocket, request, sizeof(request), 0,
                                (struct sockaddr*) &client, &clientLength);
        if(received < 0) {
            if(errno == EAGAIN) break;
            return;
        }
        if(received == (int) strlen(PATCH_DISCOVERY_REQUEST)
           && memcmp(request, PATCH_DISCOVERY_REQUEST, strlen(PATCH_DISCOVERY_REQUEST)) == 0) {
            sendto(data->discoverySocket, PATCH_DISCOVERY_RESPONSE, strlen(PATCH_DISCOVERY_RESPONSE), 0,
                   (struct sockaddr*) &client, clientLength);
        }
    }
}

bool patchinstall_listener_poll(void* listener) {
    patchinstall_data* data = (patchinstall_data*) listener;
    if(data == NULL || data->serverSocket == 0) {
        return false;
    }

    struct sockaddr_in client;
    socklen_t clientLength = sizeof(client);
    int socket = accept(data->serverSocket, (struct sockaddr*) &client, &clientLength);
    if(socket < 0) {
        return false;
    }

    data->clientSocket = socket;
    u32 networkSize = 0;
    if(patch_recv_wait(socket, &networkSize, sizeof(networkSize)) != sizeof(networkSize)) {
        data->result = R_APP_BAD_DATA;
        patch_close_client(data, 1);
        return false;
    }
    u32 size = ntohl(networkSize);
    if(size == 0 || size >= DOWNLOAD_URL_MAX) {
        data->result = R_APP_OUT_OF_RANGE;
        patch_close_client(data, 1);
        return false;
    }
    data->url = (char*) calloc(size + 1, 1);
    if(data->url == NULL) {
        data->result = R_APP_OUT_OF_MEMORY;
        patch_close_client(data, 1);
        return false;
    }
    if(patch_recv_wait(socket, data->url, size) != size
       || strlen(data->url) != size
       || strchr(data->url, '\n') != NULL
       || strchr(data->url, '\r') != NULL
       || (strncmp(data->url, "http://", 7) != 0 && strncmp(data->url, "https://", 8) != 0)) {
        data->result = R_APP_BAD_DATA;
        free(data->url);
        data->url = NULL;
        patch_close_client(data, 1);
        return false;
    }

    patch_close_listeners(data);
    return true;
}

void patchinstall_listener_prompt(void* listener) {
    patchinstall_data* data = (patchinstall_data*) listener;
    prompt_display_yes_no(
        "Install LayeredFS patch?",
        "Fast cache skips completed bundles.\nHold X while choosing Yes to verify.\nOther files will be kept.\nIf interrupted, resend the patch.",
        COLOR_TEXT, data, NULL, patch_confirm_response);
}

void patchinstall_listener_close(void* listener) {
    patch_free_data((patchinstall_data*) listener);
}

static void patch_wait_update(ui_view* view, void* userData, float* progress, char* text) {
    patchinstall_data* data = (patchinstall_data*) userData;
    if(hidKeysDown() & KEY_B) {
        ui_pop();
        info_destroy(view);
        data->result = R_APP_CANCELLED;
        patch_free_data(data);
        return;
    }

    patch_update_discovery(data);
    if(patchinstall_listener_poll(data)) {
        ui_pop();
        info_destroy(view);
        patchinstall_listener_prompt(data);
        return;
    }

    struct in_addr address = {(in_addr_t) gethostid()};
    snprintf(text, PROGRESS_TEXT_MAX,
             "Waiting for CIA Bridge...\nIP: %s\nPort: %d\nStreaming: raw\nDirect incremental overwrite: enabled",
             inet_ntoa(address), PATCH_CONTROL_PORT);
}

void* patchinstall_listener_open() {
    patchinstall_data* data = (patchinstall_data*) calloc(1, sizeof(patchinstall_data));
    if(data == NULL) {
        return NULL;
    }

    data->serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if(data->serverSocket < 0) {
        free(data);
        return NULL;
    }
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PATCH_CONTROL_PORT);
    server.sin_addr.s_addr = (in_addr_t) gethostid();
    if(bind(data->serverSocket, (struct sockaddr*) &server, sizeof(server)) < 0
       || fcntl(data->serverSocket, F_SETFL, fcntl(data->serverSocket, F_GETFL, 0) | O_NONBLOCK) < 0
       || listen(data->serverSocket, 1) < 0) {
        patch_free_data(data);
        return NULL;
    }

    return data;
}

void patchinstall_open() {
    patchinstall_data* data = (patchinstall_data*) patchinstall_listener_open();
    if(data == NULL) {
        error_display_errno(NULL, NULL, errno, "Failed to start the patch receiver.");
        return;
    }

    data->discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(data->discoverySocket >= 0) {
        int reuse = 1;
        setsockopt(data->discoverySocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in discovery;
        memset(&discovery, 0, sizeof(discovery));
        discovery.sin_family = AF_INET;
        discovery.sin_port = htons(PATCH_DISCOVERY_PORT);
        discovery.sin_addr.s_addr = INADDR_ANY;
        if(bind(data->discoverySocket, (struct sockaddr*) &discovery, sizeof(discovery)) < 0
           || fcntl(data->discoverySocket, F_SETFL, fcntl(data->discoverySocket, F_GETFL, 0) | O_NONBLOCK) < 0) {
            close(data->discoverySocket);
            data->discoverySocket = 0;
        }
    } else {
        data->discoverySocket = 0;
    }

    info_display("LayeredFS patch receiver", "B: Return", false, data, patch_wait_update, NULL);
}
