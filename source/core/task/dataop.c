#include <malloc.h>
#include <string.h>

#include <3ds.h>
#include <jansson.h>

#include "dataop.h"
#include "../core.h"

static Result task_data_op_check_running(data_op_data* data) {
    Result res = 0;

    if(task_is_quit_all() || svcWaitSynchronization(data->cancelEvent, 0) == 0) {
        res = R_APP_CANCELLED;
    } else {
        bool suspended = svcWaitSynchronization(task_get_suspend_event(), 0) != 0;
        if(suspended) {
            if(data->suspend != NULL && R_SUCCEEDED(res)) {
                res = data->suspend(data->data, data->processed);
            }
        }

        svcWaitSynchronization(task_get_pause_event(), U64_MAX);

        if(suspended) {
            if(data->restore != NULL && R_SUCCEEDED(res)) {
                res = data->restore(data->data, data->processed);
            }
        }
    }

    return res;
}

static Result task_data_op_copy(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->bytesPerSecond = 0;
    data->estimatedRemainingSeconds = 0;

    Result res = 0;

    bool isDir = false;
    if(R_SUCCEEDED(res = data->isSrcDirectory(data->data, index, &isDir)) && isDir) {
        res = data->makeDstDirectory(data->data, index);
    } else {
        u32 srcHandle = 0;
        if(R_SUCCEEDED(res = data->openSrc(data->data, index, &srcHandle))) {
            if(R_SUCCEEDED(res = data->getSrcSize(data->data, srcHandle, &data->currTotal))) {
                if(data->currTotal == 0) {
                    if(data->copyEmpty) {
                        u32 dstHandle = 0;
                        if(R_SUCCEEDED(res = data->openDst(data->data, index, NULL, data->currTotal, &dstHandle))) {
                            res = data->closeDst(data->data, index, true, dstHandle);
                        }
                    } else {
                        res = R_APP_BAD_DATA;
                    }
                } else {
                    u8* buffer = (u8*) calloc(1, data->bufferSize);
                    if(buffer != NULL) {
                        u32 dstHandle = 0;

                        u64 ioStartTime = 0;
                        u64 lastBytesPerSecondUpdate = osGetTime();
                        u32 bytesSinceUpdate = 0;

                        bool firstRun = true;
                        while(data->currProcessed < data->currTotal) {
                            if(R_FAILED(res = task_data_op_check_running(data))) {
                                break;
                            }

                            u32 bytesRead = 0;
                            if(R_FAILED(res = data->readSrc(data->data, srcHandle, &bytesRead, buffer, data->currProcessed, data->bufferSize))) {
                                break;
                            }

                            if(firstRun) {
                                firstRun = false;

                                if(R_FAILED(res = data->openDst(data->data, index, buffer, data->currTotal, &dstHandle))) {
                                    break;
                                }
                            }

                            u32 bytesWritten = 0;
                            if(R_FAILED(res = data->writeDst(data->data, dstHandle, &bytesWritten, buffer, data->currProcessed, bytesRead))) {
                                break;
                            }

                            data->currProcessed += bytesWritten;
                            bytesSinceUpdate += bytesWritten;

                            u64 time = osGetTime();
                            u64 elapsed = time - lastBytesPerSecondUpdate;
                            if(elapsed >= 1000) {
                                data->bytesPerSecond = (u32) (bytesSinceUpdate / (elapsed / 1000.0f));

                                if(ioStartTime != 0) {
                                    data->estimatedRemainingSeconds = (u32) ((data->currTotal - data->currProcessed) / (data->currProcessed / ((time - ioStartTime) / 1000.0f)));
                                } else {
                                    data->estimatedRemainingSeconds = 0;
                                }

                                if(ioStartTime == 0 && data->currProcessed > 0) {
                                    ioStartTime = time;
                                }

                                bytesSinceUpdate = 0;
                                lastBytesPerSecondUpdate = time;
                            }
                        }

                        if(dstHandle != 0) {
                            Result closeDstRes = data->closeDst(data->data, index, res == 0, dstHandle);
                            if(R_SUCCEEDED(res)) {
                                res = closeDstRes;
                            }
                        }

                        free(buffer);
                    } else {
                        res = R_APP_OUT_OF_MEMORY;
                    }
                }
            }

            Result closeSrcRes = data->closeSrc(data->data, index, res == 0, srcHandle);
            if(R_SUCCEEDED(res)) {
                res = closeSrcRes;
            }
        }
    }

    return res;
}

#define DATA_OP_DOWNLOAD_QUEUE_MAX_SLOTS 16
#define DATA_OP_DOWNLOAD_QUEUE_MIN_SLOTS 2

typedef struct {
    data_op_data* data;

    u32 index;

    u32 dstHandle;
    bool firstRun;
    u64 ioStartTime;
    u64 lastBytesPerSecondUpdate;
    u32 bytesSinceUpdate;

    u64 writeOffset;

    u8* queueStorage;
    size_t queueSizes[DATA_OP_DOWNLOAD_QUEUE_MAX_SLOTS];
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
} data_op_download_data;

static Result task_data_op_download_write_all(data_op_download_data* downloadData, void* buffer, size_t size) {
    data_op_data* data = downloadData->data;

    size_t position = 0;
    while(position < size) {
        u32 remaining = (u32) (size - position);
        u32 bytesWritten = 0;
        Result res = data->writeDst(data->data, downloadData->dstHandle, &bytesWritten,
                                    (u8*) buffer + position, downloadData->writeOffset, remaining);
        if(R_FAILED(res)) {
            return res;
        }

        if(bytesWritten == 0 || bytesWritten > remaining) {
            return R_APP_BAD_DATA;
        }

        position += bytesWritten;
        downloadData->writeOffset += bytesWritten;
    }

    return 0;
}

static void task_data_op_download_writer_thread(void* arg) {
    data_op_download_data* downloadData = (data_op_download_data*) arg;

    while(true) {
        LightLock_Lock(&downloadData->queueLock);
        while(downloadData->queuedCount == 0 && !downloadData->producerDone && !downloadData->abortWriter) {
            CondVar_Wait(&downloadData->queueNotEmpty, &downloadData->queueLock);
        }

        if(downloadData->abortWriter || (downloadData->producerDone && downloadData->queuedCount == 0)) {
            LightLock_Unlock(&downloadData->queueLock);
            break;
        }

        u32 slot = downloadData->queueReadIndex;
        size_t size = downloadData->queueSizes[slot];
        downloadData->queueReadIndex = (downloadData->queueReadIndex + 1) % downloadData->queueSlotCount;
        downloadData->queuedCount--;
        LightLock_Unlock(&downloadData->queueLock);

        u8* slotBuffer = downloadData->queueStorage + (slot * downloadData->data->bufferSize);
        Result res = task_data_op_download_write_all(downloadData, slotBuffer, size);

        LightLock_Lock(&downloadData->queueLock);
        downloadData->inUseCount--;
        if(R_FAILED(res)) {
            downloadData->writerResult = res;
            downloadData->abortWriter = true;
        }

        CondVar_Broadcast(&downloadData->queueNotFull);
        CondVar_Broadcast(&downloadData->queueNotEmpty);

        bool finished = downloadData->abortWriter
                        || (downloadData->producerDone && downloadData->queuedCount == 0 && downloadData->inUseCount == 0);
        LightLock_Unlock(&downloadData->queueLock);

        if(finished) {
            break;
        }
    }
}

static void task_data_op_download_free_async(data_op_download_data* downloadData) {
    free(downloadData->queueStorage);
    downloadData->queueStorage = NULL;
    downloadData->queueSlotCount = 0;
    downloadData->asyncEnabled = false;
}

static bool task_data_op_download_init_async(data_op_download_data* downloadData) {
    LightLock_Init(&downloadData->queueLock);
    CondVar_Init(&downloadData->queueNotEmpty);
    CondVar_Init(&downloadData->queueNotFull);

    u32 slotCount = DATA_OP_DOWNLOAD_QUEUE_MAX_SLOTS;
    while(slotCount >= DATA_OP_DOWNLOAD_QUEUE_MIN_SLOTS) {
        size_t allocationSize = (size_t) downloadData->data->bufferSize * slotCount;
        downloadData->queueStorage = (u8*) malloc(allocationSize);
        if(downloadData->queueStorage != NULL) {
            downloadData->queueSlotCount = slotCount;

            // Keep the writer on the application core. The download worker runs on core 2 on
            // New 3DS and core 1 on Old 3DS, so network receive can overlap the AM/FS IPC wait.
            downloadData->writerThread = threadCreate(task_data_op_download_writer_thread, downloadData,
                                                       0x10000, 0x20, 0, false);
            if(downloadData->writerThread != NULL) {
                downloadData->asyncEnabled = true;
                return true;
            }

            free(downloadData->queueStorage);
            downloadData->queueStorage = NULL;
            downloadData->queueSlotCount = 0;
        }

        slotCount /= 2;
    }

    return false;
}

static Result task_data_op_download_enqueue(data_op_download_data* downloadData, void* buffer, size_t size) {
    if(size > downloadData->data->bufferSize || downloadData->queueSlotCount < DATA_OP_DOWNLOAD_QUEUE_MIN_SLOTS) {
        return R_APP_BAD_DATA;
    }

    while(true) {
        LightLock_Lock(&downloadData->queueLock);

        if(R_FAILED(downloadData->writerResult)) {
            Result res = downloadData->writerResult;
            LightLock_Unlock(&downloadData->queueLock);
            return res;
        }

        if(downloadData->abortWriter) {
            LightLock_Unlock(&downloadData->queueLock);
            return R_APP_CANCELLED;
        }

        if(downloadData->inUseCount < downloadData->queueSlotCount) {
            u32 slot = downloadData->queueWriteIndex;
            u8* slotBuffer = downloadData->queueStorage + (slot * downloadData->data->bufferSize);
            memcpy(slotBuffer, buffer, size);
            downloadData->queueSizes[slot] = size;
            downloadData->queueWriteIndex = (downloadData->queueWriteIndex + 1) % downloadData->queueSlotCount;
            downloadData->queuedCount++;
            downloadData->inUseCount++;

            CondVar_Signal(&downloadData->queueNotEmpty);
            LightLock_Unlock(&downloadData->queueLock);
            return 0;
        }

        int waitRes = CondVar_WaitTimeout(&downloadData->queueNotFull, &downloadData->queueLock, 25 * 1000 * 1000LL);
        LightLock_Unlock(&downloadData->queueLock);

        if(waitRes != 0) {
            Result res = task_data_op_check_running(downloadData->data);
            if(R_FAILED(res)) {
                LightLock_Lock(&downloadData->queueLock);
                downloadData->abortWriter = true;
                CondVar_Broadcast(&downloadData->queueNotEmpty);
                CondVar_Broadcast(&downloadData->queueNotFull);
                LightLock_Unlock(&downloadData->queueLock);
                return res;
            }
        }
    }
}

static Result task_data_op_download_finish_async(data_op_download_data* downloadData, Result transferResult) {
    LightLock_Lock(&downloadData->queueLock);
    downloadData->producerDone = true;
    if(R_FAILED(transferResult)) {
        downloadData->abortWriter = true;
    }

    CondVar_Broadcast(&downloadData->queueNotEmpty);
    CondVar_Broadcast(&downloadData->queueNotFull);
    LightLock_Unlock(&downloadData->queueLock);

    Result joinResult = threadJoin(downloadData->writerThread, U64_MAX);
    threadFree(downloadData->writerThread);
    downloadData->writerThread = NULL;

    if(R_SUCCEEDED(transferResult)) {
        if(R_FAILED(downloadData->writerResult)) {
            transferResult = downloadData->writerResult;
        } else if(R_FAILED(joinResult)) {
            transferResult = joinResult;
        }
    }

    task_data_op_download_free_async(downloadData);
    return transferResult;
}

static Result task_data_op_download_callback(void* userData, void* buffer, size_t size) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;
    data_op_data* data = downloadData->data;

    if(downloadData->firstRun) {
        downloadData->firstRun = false;

        Result res = data->openDst(data->data, downloadData->index, buffer, data->currTotal, &downloadData->dstHandle);
        if(R_FAILED(res)) {
            return res;
        }
    }

    if(downloadData->asyncEnabled) {
        return task_data_op_download_enqueue(downloadData, buffer, size);
    }

    return task_data_op_download_write_all(downloadData, buffer, size);
}

static Result task_data_op_download_check_running(void* userData) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;

    if(downloadData->asyncEnabled) {
        LightLock_Lock(&downloadData->queueLock);
        Result writerResult = downloadData->writerResult;
        LightLock_Unlock(&downloadData->queueLock);

        if(R_FAILED(writerResult)) {
            return writerResult;
        }
    }

    return task_data_op_check_running(downloadData->data);
}

static Result task_data_op_download_progress(void* userData, u64 total, u64 curr) {
    data_op_download_data* downloadData = (data_op_download_data*) userData;
    data_op_data* data = downloadData->data;

    downloadData->bytesSinceUpdate += curr - data->currProcessed;

    data->currTotal = total;
    data->currProcessed = curr;

    u64 time = osGetTime();
    u64 elapsed = time - downloadData->lastBytesPerSecondUpdate;
    if(elapsed >= 1000) {
        data->bytesPerSecond = (u32) (downloadData->bytesSinceUpdate / (elapsed / 1000.0f));

        if(downloadData->ioStartTime != 0) {
            data->estimatedRemainingSeconds = (u32) ((data->currTotal - data->currProcessed) / (data->currProcessed / ((time - downloadData->ioStartTime) / 1000.0f)));
        } else {
            data->estimatedRemainingSeconds = 0;
        }

        if(downloadData->ioStartTime == 0 && data->currProcessed > 0) {
            downloadData->ioStartTime = time;
        }

        downloadData->bytesSinceUpdate = 0;
        downloadData->lastBytesPerSecondUpdate = time;
    }

    return 0;
}

static Result task_data_op_download(data_op_data* data, u32 index) {
    data->currProcessed = 0;
    data->currTotal = 0;

    data->bytesPerSecond = 0;
    data->estimatedRemainingSeconds = 0;

    Result res = 0;

    char url[DOWNLOAD_URL_MAX];
    if(R_SUCCEEDED(res = data->getSrcUrl(data->data, index, url, DOWNLOAD_URL_MAX))) {
        data_op_download_data downloadData;
        memset(&downloadData, 0, sizeof(downloadData));
        downloadData.data = data;
        downloadData.index = index;
        downloadData.firstRun = true;
        downloadData.lastBytesPerSecondUpdate = osGetTime();

        task_data_op_download_init_async(&downloadData);

        res = http_download_callback(url, data->bufferSize, &downloadData, task_data_op_download_callback, task_data_op_download_check_running, task_data_op_download_progress);

        if(downloadData.asyncEnabled) {
            res = task_data_op_download_finish_async(&downloadData, res);
        }

        if(downloadData.dstHandle != 0) {
            Result closeDstRes = data->closeDst(data->data, index, res == 0, downloadData.dstHandle);
            if(R_SUCCEEDED(res)) {
                res = closeDstRes;
            }
        }
    }

    return res;
}

static Result task_data_op_delete(data_op_data* data, u32 index) {
    return data->delete(data->data, index);
}

static void task_data_op_retry_onresponse(ui_view* view, void* data, u32 response) {
    ((data_op_data*) data)->retryResponse = response == PROMPT_YES;
}

static void task_data_op_thread(void* arg) {
    data_op_data* data = (data_op_data*) arg;

    for(data->processed = 0; data->processed < data->total; data->processed++) {
        Result res = 0;

        if(R_SUCCEEDED(res = task_data_op_check_running(data))) {
            switch(data->op) {
                case DATAOP_COPY:
                    res = task_data_op_copy(data, data->processed);
                    break;
                case DATAOP_DOWNLOAD:
                    res = task_data_op_download(data, data->processed);
                    break;
                case DATAOP_DELETE:
                    res = task_data_op_delete(data, data->processed);
                    break;
                default:
                    break;
            }
        }

        data->result = res;

        if(R_FAILED(res)) {
            if(res == R_APP_CANCELLED) {
                prompt_display_notify("Failure", "Operation cancelled.", COLOR_TEXT, NULL, NULL, NULL);
                break;
            } else if(res != R_APP_SKIPPED) {
                ui_view* errorView = NULL;
                bool proceed = data->error(data->data, data->processed, res, &errorView);

                if(errorView != NULL) {
                    svcWaitSynchronization(errorView->active, U64_MAX);
                }

                ui_view* retryView = prompt_display_yes_no("Confirmation", "Retry?", COLOR_TEXT, data, NULL, task_data_op_retry_onresponse);
                if(retryView != NULL) {
                    svcWaitSynchronization(retryView->active, U64_MAX);

                    if(data->retryResponse) {
                        if(proceed) {
                            data->processed--;
                        } else {
                            data->processed = 0;
                        }
                    } else if(!proceed) {
                        break;
                    }
                }
            }
        }
    }

    svcCloseHandle(data->cancelEvent);

    data->finished = true;

    aptSetSleepAllowed(true);
}

Result task_data_op(data_op_data* data) {
    if(data == NULL) {
        return R_APP_INVALID_ARGUMENT;
    }

    data->processed = 0;

    data->currProcessed = 0;
    data->currTotal = 0;

    data->finished = false;
    data->result = 0;
    data->cancelEvent = 0;

    Result res = 0;
    if(R_SUCCEEDED(res = svcCreateEvent(&data->cancelEvent, RESET_STICKY))) {
        int core = 1;
        if(data->op == DATAOP_DOWNLOAD) {
            bool n3ds = false;
            if(R_SUCCEEDED(APT_CheckNew3DS(&n3ds)) && n3ds) {
                core = 2;
            }
        }

        Thread worker = threadCreate(task_data_op_thread, data, 0x10000, 0x18, core, true);
        if(worker == NULL && core == 2) {
            worker = threadCreate(task_data_op_thread, data, 0x10000, 0x18, 1, true);
        }

        if(worker == NULL) {
            res = R_APP_THREAD_CREATE_FAILED;
        }
    }

    if(R_FAILED(res)) {
        data->finished = true;

        if(data->cancelEvent != 0) {
            svcCloseHandle(data->cancelEvent);
            data->cancelEvent = 0;
        }
    }

    aptSetSleepAllowed(false);

    return res;
}
