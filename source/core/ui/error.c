#include "../i18n.h"
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>
#include <curl/curl.h>
#include <jansson.h>

#include "error.h"
#include "../http.h"
#include "prompt.h"
#include "../error.h"
#include "../screen.h"
#include "../../fbi/resources.h"

static const char* level_to_string(Result res) {
    switch(R_LEVEL(res)) {
        case RL_SUCCESS:
            return i18n_get(I18N_SUCCESS);
        case RL_INFO:
            return i18n_get(I18N_INFO);
        case RL_FATAL:
            return i18n_get(I18N_FATAL);
        case RL_RESET:
            return i18n_get(I18N_RESET);
        case RL_REINITIALIZE:
            return i18n_get(I18N_REINITIALIZE);
        case RL_USAGE:
            return i18n_get(I18N_USAGE);
        case RL_PERMANENT:
            return i18n_get(I18N_PERMANENT);
        case RL_TEMPORARY:
            return i18n_get(I18N_TEMPORARY);
        case RL_STATUS:
            return i18n_get(I18N_STATUS);
        default:
            return i18n_get(I18N_UNKNOWN);
    }
}

static const char* summary_to_string(Result res) {
    switch(R_SUMMARY(res)) {
        case RS_SUCCESS:
            return i18n_get(I18N_SUCCESS);
        case RS_NOP:
            return i18n_get(I18N_NOP);
        case RS_WOULDBLOCK:
            return i18n_get(I18N_WOULD_BLOCK);
        case RS_OUTOFRESOURCE:
            return i18n_get(I18N_OUT_OF_RESOURCE);
        case RS_NOTFOUND:
            return i18n_get(I18N_NOT_FOUND);
        case RS_INVALIDSTATE:
            return i18n_get(I18N_INVALID_STATE);
        case RS_NOTSUPPORTED:
            return i18n_get(I18N_NOT_SUPPORTED);
        case RS_INVALIDARG:
            return i18n_get(I18N_INVALID_ARGUMENT);
        case RS_WRONGARG:
            return i18n_get(I18N_WRONG_ARGUMENT);
        case RS_CANCELED:
            return i18n_get(I18N_CANCELED);
        case RS_STATUSCHANGED:
            return i18n_get(I18N_STATUS_CHANGED);
        case RS_INTERNAL:
            return i18n_get(I18N_INTERNAL);
        default:
            return i18n_get(I18N_UNKNOWN);
    }
}

static const char* module_to_string(Result res) {
    switch(R_MODULE(res)) {
        case RM_COMMON:
            return i18n_get(I18N_COMMON);
        case RM_KERNEL:
            return i18n_get(I18N_KERNEL);
        case RM_UTIL:
            return i18n_get(I18N_UTIL);
        case RM_FILE_SERVER:
            return i18n_get(I18N_FILE_SERVER);
        case RM_LOADER_SERVER:
            return i18n_get(I18N_LOADER_SERVER);
        case RM_TCB:
            return "TCB";
        case RM_OS:
            return "OS";
        case RM_DBG:
            return "DBG";
        case RM_DMNT:
            return "DMNT";
        case RM_PDN:
            return "PDN";
        case RM_GSP:
            return "GSP";
        case RM_I2C:
            return "I2C";
        case RM_GPIO:
            return "GPIO";
        case RM_DD:
            return "DD";
        case RM_CODEC:
            return "CODEC";
        case RM_SPI:
            return "SPI";
        case RM_PXI:
            return "PXI";
        case RM_FS:
            return "FS";
        case RM_DI:
            return "DI";
        case RM_HID:
            return "HID";
        case RM_CAM:
            return "CAM";
        case RM_PI:
            return "PI";
        case RM_PM:
            return "PM";
        case RM_PM_LOW:
            return "PMLOW";
        case RM_FSI:
            return "FSI";
        case RM_SRV:
            return "SRV";
        case RM_NDM:
            return "NDM";
        case RM_NWM:
            return "NWM";
        case RM_SOC:
            return "SOC";
        case RM_LDR:
            return "LDR";
        case RM_ACC:
            return "ACC";
        case RM_ROMFS:
            return "RomFS";
        case RM_AM:
            return "AM";
        case RM_HIO:
            return "HIO";
        case RM_UPDATER:
            return i18n_get(I18N_UPDATER);
        case RM_MIC:
            return "MIC";
        case RM_FND:
            return "FND";
        case RM_MP:
            return "MP";
        case RM_MPWL:
            return "MPWL";
        case RM_AC:
            return "AC";
        case RM_HTTP:
            return "HTTP";
        case RM_DSP:
            return "DSP";
        case RM_SND:
            return "SND";
        case RM_DLP:
            return "DLP";
        case RM_HIO_LOW:
            return "HIOLOW";
        case RM_CSND:
            return "CSND";
        case RM_SSL:
            return "SSL";
        case RM_AM_LOW:
            return "AMLOW";
        case RM_NEX:
            return "NEX";
        case RM_FRIENDS:
            return i18n_get(I18N_FRIENDS);
        case RM_RDT:
            return "RDT";
        case RM_APPLET:
            return i18n_get(I18N_APPLET);
        case RM_NIM:
            return "NIM";
        case RM_PTM:
            return "PTM";
        case RM_MIDI:
            return "MIDI";
        case RM_MC:
            return "MC";
        case RM_SWC:
            return "SWC";
        case RM_FATFS:
            return "FatFS";
        case RM_NGC:
            return "NGC";
        case RM_CARD:
            return "CARD";
        case RM_CARDNOR:
            return "CARDNOR";
        case RM_SDMC:
            return "SDMC";
        case RM_BOSS:
            return "BOSS";
        case RM_DBM:
            return "DBM";
        case RM_CONFIG:
            return i18n_get(I18N_CONFIG);
        case RM_PS:
            return "PS";
        case RM_CEC:
            return "CEC";
        case RM_IR:
            return "IR";
        case RM_UDS:
            return "UDS";
        case RM_PL:
            return "PL";
        case RM_CUP:
            return "CUP";
        case RM_GYROSCOPE:
            return i18n_get(I18N_GYROSCOPE);
        case RM_MCU:
            return "MCU";
        case RM_NS:
            return "NS";
        case RM_NEWS:
            return "NEWS";
        case RM_RO:
            return "RO";
        case RM_GD:
            return "GD";
        case RM_CARD_SPI:
            return "CARDSPI";
        case RM_EC:
            return "EC";
        case RM_WEB_BROWSER:
            return i18n_get(I18N_WEB_BROWSER);
        case RM_TEST:
            return "TEST";
        case RM_ENC:
            return "ENC";
        case RM_PIA:
            return "PIA";
        case RM_ACT:
            return "ACT";
        case RM_VCTL:
            return "VCTL";
        case RM_OLV:
            return "OLV";
        case RM_NEIA:
            return "NEIA";
        case RM_NPNS:
            return "NPNS";
        case RM_AVD:
            return "AVD";
        case RM_L2B:
            return "L2B";
        case RM_MVD:
            return "MVD";
        case RM_NFC:
            return "NFC";
        case RM_UART:
            return "UART";
        case RM_SPM:
            return "SPM";
        case RM_QTM:
            return "QTM";
        case RM_NFP:
            return "NFP";
        case RM_APPLICATION:
            return i18n_get(I18N_APPLICATION);
        default:
            return i18n_get(I18N_UNKNOWN);
    }
}

static const char* description_to_string(Result res) {
    static char descriptionString[256];

    int module = R_MODULE(res);
    int description = R_DESCRIPTION(res);

    switch(module) {
        case RM_KERNEL:
            switch(description) {
                case 2:
                    return i18n_get(I18N_INVALID_DMA_BUFFER_MEMORY_PERMISSIONS);
                default:
                    break;
            }

            break;
        case RM_OS:
            switch(description) {
                case 1:
                    return i18n_get(I18N_OUT_OF_SYNCHRONIZATION_OBJECT);
                case 2:
                    return i18n_get(I18N_OUT_OF_SHARED_MEMORY_OBJECTS);
                case 9:
                    return i18n_get(I18N_OUT_OF_SESSION_OBJECTS);
                case 10:
                    return i18n_get(I18N_NOT_ENOUGH_MEMORY_FOR_ALLOCATION);
                case 20:
                    return i18n_get(I18N_WRONG_PERMISSIONS_FOR_UNPRIVILEGED_ACCESS);
                case 26:
                    return i18n_get(I18N_SESSION_CLOSED_BY_REMOTE_PROCESS);
                case 47:
                    return i18n_get(I18N_INVALID_COMMAND_HEADER);
                case 52:
                    return i18n_get(I18N_MAX_PORT_CONNECTIONS_EXCEEDED);
                default:
                    break;
            }

            break;
        case RM_FS:
            switch(description) {
                case 101:
                    return i18n_get(I18N_ARCHIVE_NOT_MOUNTED);
                case 120:
                    return i18n_get(I18N_DOESN_T_EXIST_FAILED_TO_OPEN);
                case 141:
                    return i18n_get(I18N_GAME_CARD_NOT_INSERTED);
                case 171:
                    return i18n_get(I18N_BUS_BUSY_UNDERRUN);
                case 172:
                    return i18n_get(I18N_BUS_ILLEGAL_FUNCTION);
                case 190:
                    return i18n_get(I18N_ALREADY_EXISTS_FAILED_TO_CREATE);
                case 210:
                    return i18n_get(I18N_PARTITION_FULL);
                case 230:
                    return i18n_get(I18N_ILLEGAL_OPERATION_FILE_IN_USE);
                case 231:
                    return i18n_get(I18N_RESOURCE_LOCKED);
                case 250:
                    return i18n_get(I18N_FAT_OPERATION_DENIED);
                case 265:
                    return i18n_get(I18N_BUS_TIMEOUT);
                case 331:
                    return i18n_get(I18N_BUS_ERROR_TWL_PARTITION_INVALID);
                case 332:
                    return i18n_get(I18N_BUS_STOP_BIT_ERROR);
                case 391:
                    return i18n_get(I18N_HASH_VERIFICATION_FAILURE);
                case 392:
                    return i18n_get(I18N_RSA_HASH_VERIFICATION_FAILURE);
                case 395:
                    return i18n_get(I18N_INVALID_ROMFS_OR_SAVE_DATA_BLOCK_HASH);
                case 630:
                    return i18n_get(I18N_ARCHIVE_PERMISSION_DENIED);
                case 702:
                    return i18n_get(I18N_INVALID_PATH_INACCESSIBLE_ARCHIVE);
                case 705:
                    return i18n_get(I18N_OFFSET_OUT_OF_BOUNDS);
                case 721:
                    return i18n_get(I18N_REACHED_FILE_SIZE_LIMIT);
                case 760:
                    return i18n_get(I18N_UNSUPPORTED_OPERATION);
                case 761:
                    return i18n_get(I18N_EXEFS_READ_SIZE_MISMATCH);
                default:
                    break;
            }

            break;
        case RM_SRV:
            switch(description) {
                case 5:
                    return i18n_get(I18N_INVALID_SERVICE_NAME_LENGTH);
                case 6:
                    return i18n_get(I18N_SERVICE_ACCESS_DENIED);
                case 7:
                    return i18n_get(I18N_STRING_SIZE_MISMATCH);
                default:
                    break;
            }

            break;
        case RM_AM:
            switch(description) {
                case 4:
                    return i18n_get(I18N_WRONG_INSTALLATION_STATE);
                case 37:
                    return i18n_get(I18N_INVALID_NCCH);
                case 39:
                    return i18n_get(I18N_INVALID_OR_OUTDATED_TITLE_VERSION);
                case 41:
                    return i18n_get(I18N_ERROR_TYPE_1_2);
                case 43:
                    return i18n_get(I18N_DATABASE_DOES_NOT_EXIST);
                case 44:
                    return i18n_get(I18N_ATTEMPTED_TO_DELETE_SYSTEM_TITLE);
                case 101:
                    return i18n_get(I18N_ERROR_TYPE_1);
                case 102:
                    return i18n_get(I18N_ERROR_TYPE_2);
                case 103:
                    return i18n_get(I18N_ERROR_TYPE_3);
                case 104:
                    return i18n_get(I18N_ERROR_TYPE_4);
                case 105:
                    return i18n_get(I18N_ERROR_TYPE_5);
                case 106:
                    return i18n_get(I18N_CERT_SIGNATURE_OR_HASH_CHECK_FAILED);
                case 107:
                    return i18n_get(I18N_ERROR_TYPE_7);
                case 108:
                    return i18n_get(I18N_ERROR_TYPE_8);
                case 109:
                    return i18n_get(I18N_ERROR_TYPE_9);
                case 110:
                    return i18n_get(I18N_ERROR_TYPE_10);
                case 111:
                    return i18n_get(I18N_ERROR_TYPE_11);
                case 112:
                    return i18n_get(I18N_ERROR_TYPE_12);
                case 113:
                    return i18n_get(I18N_ERROR_TYPE_13);
                case 114:
                    return i18n_get(I18N_ERROR_TYPE_14);
                case 393:
                    return i18n_get(I18N_INVALID_DATABASE);
                default:
                    break;
            }

            break;
        case RM_HTTP:
            switch(description) {
                case 60:
                    return i18n_get(I18N_FAILED_TO_VERIFY_TLS_CERTIFICATE);
                case 70:
                    return i18n_get(I18N_NETWORK_UNAVAILABLE);
                case 102:
                    return i18n_get(I18N_WRONG_CONTEXT_HANDLE);
                case 105:
                    return i18n_get(I18N_REQUEST_TIMED_OUT);
                default:
                    break;
            }

            break;
        case RM_SSL:
            switch(description) {
                case 20:
                    return i18n_get(I18N_UNTRUSTED_ROOTCA);
                case 54:
                    return i18n_get(I18N_ROOTCERTCHAIN_HANDLE_NOT_FOUND);
                default:
                    break;
            }

            break;
        case RM_SDMC:
            switch(description) {
                case 1:
                    return i18n_get(I18N_BUS_BIT23_ERROR);
                case 2:
                    return i18n_get(I18N_BUS_RX_READY_ERROR);
                case 3:
                    return i18n_get(I18N_BUS_BIT28_ERROR);
                case 4:
                    return i18n_get(I18N_BUS_BIT27_ERROR);
                default:
                    break;
            }

            break;
        case RM_MVD:
            switch(description) {
                case 271:
                    return i18n_get(I18N_INVALID_CONFIGURATION);
                default:
                    break;
            }

            break;
        case RM_NFC:
            switch(description) {
                case 512:
                    return i18n_get(I18N_INVALID_NFC_STATE);
                default:
                    break;
            }

            break;
        case RM_QTM:
            switch(description) {
                case 8:
                    return i18n_get(I18N_CAMERA_BUSY);
                default:
                    break;
            }

            break;
        case RM_APPLICATION:
            switch(res) {
                case R_APP_INVALID_ARGUMENT:
                    return i18n_get(I18N_INVALID_ARGUMENT);
                case R_APP_CANCELLED:
                    return i18n_get(I18N_OPERATION_CANCELLED);
                case R_APP_SKIPPED:
                    return i18n_get(I18N_OPERATION_SKIPPED);
                case R_APP_THREAD_CREATE_FAILED:
                    return i18n_get(I18N_THREAD_CREATION_FAILED);
                case R_APP_PARSE_FAILED:
                    return i18n_get(I18N_PARSE_FAILED);
                case R_APP_BAD_DATA:
                    return i18n_get(I18N_BAD_DATA);
                case R_APP_HTTP_TOO_MANY_REDIRECTS:
                    return i18n_get(I18N_TOO_MANY_REDIRECTS);
                default:
                    if(res >= R_APP_HTTP_ERROR_BASE && res < R_APP_HTTP_ERROR_END) {
                        switch(res - R_APP_HTTP_ERROR_BASE) {
                            case 100:
                                return i18n_get(I18N_HTTP_100_CONTINUE);
                            case 101:
                                return i18n_get(I18N_HTTP_101_SWITCHING_PROTOCOLS);
                            case 102:
                                return i18n_get(I18N_HTTP_102_PROCESSING);
                            case 103:
                                return i18n_get(I18N_HTTP_103_EARLY_HINTS);
                            case 200:
                                return i18n_get(I18N_HTTP_200_OK);
                            case 201:
                                return i18n_get(I18N_HTTP_201_CREATED);
                            case 202:
                                return i18n_get(I18N_HTTP_202_ACCEPTED);
                            case 203:
                                return i18n_get(I18N_HTTP_203_NON_AUTHORITATIVE_INFORMATION);
                            case 204:
                                return i18n_get(I18N_HTTP_204_NO_CONTENT);
                            case 205:
                                return i18n_get(I18N_HTTP_205_RESET_CONTENT);
                            case 206:
                                return i18n_get(I18N_HTTP_206_PARTIAL_CONTENT);
                            case 207:
                                return i18n_get(I18N_HTTP_207_MULTI_STATUS);
                            case 208:
                                return i18n_get(I18N_HTTP_208_ALREADY_REPORTED);
                            case 226:
                                return i18n_get(I18N_HTTP_226_IM_USED);
                            case 300:
                                return i18n_get(I18N_HTTP_300_MULTIPLE_CHOICES);
                            case 301:
                                return i18n_get(I18N_HTTP_301_MOVED_PERMANENTLY);
                            case 302:
                                return i18n_get(I18N_HTTP_302_FOUND);
                            case 303:
                                return i18n_get(I18N_HTTP_303_SEE_OTHER);
                            case 304:
                                return i18n_get(I18N_HTTP_304_NOT_MODIFIED);
                            case 305:
                                return i18n_get(I18N_HTTP_305_USE_PROXY);
                            case 306:
                                return i18n_get(I18N_HTTP_306_SWITCH_PROXY);
                            case 307:
                                return i18n_get(I18N_HTTP_307_TEMPORARY_REDIRECT);
                            case 308:
                                return i18n_get(I18N_HTTP_308_PERMANENT_REDIRECT);
                            case 400:
                                return i18n_get(I18N_HTTP_400_BAD_REQUEST);
                            case 401:
                                return i18n_get(I18N_HTTP_401_UNAUTHORIZED);
                            case 402:
                                return i18n_get(I18N_HTTP_402_PAYMENT_REQUIRED);
                            case 403:
                                return i18n_get(I18N_HTTP_403_FORBIDDEN);
                            case 404:
                                return i18n_get(I18N_HTTP_404_NOT_FOUND);
                            case 405:
                                return i18n_get(I18N_HTTP_405_METHOD_NOT_ALLOWED);
                            case 406:
                                return i18n_get(I18N_HTTP_406_NOT_ACCEPTABLE);
                            case 407:
                                return i18n_get(I18N_HTTP_407_PROXY_AUTHENTICATION_REQUIRED);
                            case 408:
                                return i18n_get(I18N_HTTP_408_REQUEST_TIMEOUT);
                            case 409:
                                return i18n_get(I18N_HTTP_409_CONFLICT);
                            case 410:
                                return i18n_get(I18N_HTTP_410_GONE);
                            case 411:
                                return i18n_get(I18N_HTTP_411_LENGTH_REQUIRED);
                            case 412:
                                return i18n_get(I18N_HTTP_412_PRECONDITION_FAILED);
                            case 413:
                                return i18n_get(I18N_HTTP_413_PAYLOAD_TOO_LARGE);
                            case 414:
                                return i18n_get(I18N_HTTP_414_URI_TOO_LONG);
                            case 415:
                                return i18n_get(I18N_HTTP_415_UNSUPPORTED_MEDIA_TYPE);
                            case 416:
                                return i18n_get(I18N_HTTP_416_RANGE_NOT_SATISFIABLE);
                            case 417:
                                return i18n_get(I18N_HTTP_417_EXPECTATION_FAILED);
                            case 418:
                                return i18n_get(I18N_HTTP_418_I_M_A_TEAPOT);
                            case 421:
                                return i18n_get(I18N_HTTP_421_MISDIRECTED_REQUEST);
                            case 422:
                                return i18n_get(I18N_HTTP_422_UNPROCESSABLE_ENTITY);
                            case 423:
                                return i18n_get(I18N_HTTP_423_LOCKED);
                            case 424:
                                return i18n_get(I18N_HTTP_424_FAILED_DEPENDENCY);
                            case 426:
                                return i18n_get(I18N_HTTP_426_UPGRADE_REQUIRED);
                            case 428:
                                return i18n_get(I18N_HTTP_428_PRECONDITION_REQUIRED);
                            case 429:
                                return i18n_get(I18N_HTTP_429_TOO_MANY_REQUESTS);
                            case 431:
                                return i18n_get(I18N_HTTP_431_REQUEST_HEADER_FIELDS_TOO_LARGE);
                            case 451:
                                return i18n_get(I18N_HTTP_451_UNAVAILABLE_FOR_LEGAL_REASONS);
                            case 500:
                                return i18n_get(I18N_HTTP_500_INTERNAL_SERVER_ERROR);
                            case 501:
                                return i18n_get(I18N_HTTP_501_NOT_IMPLEMENTED);
                            case 502:
                                return i18n_get(I18N_HTTP_502_BAD_GATEWAY);
                            case 503:
                                return i18n_get(I18N_HTTP_503_SERVICE_UNAVAILABLE);
                            case 504:
                                return i18n_get(I18N_HTTP_504_GATEWAY_TIMEOUT);
                            case 505:
                                return i18n_get(I18N_HTTP_505_HTTP_VERSION_NOT_SPECIFIED);
                            case 506:
                                return i18n_get(I18N_HTTP_506_VARIANT_ALSO_NEGOTIATES);
                            case 507:
                                return i18n_get(I18N_HTTP_507_INSUFFICIENT_STORAGE);
                            case 508:
                                return i18n_get(I18N_HTTP_508_LOOP_DETECTED);
                            case 510:
                                return i18n_get(I18N_HTTP_510_NOT_EXTENDED);
                            case 511:
                                return i18n_get(I18N_HTTP_511_NETWORK_AUTHENTICATION_REQUIRED);
                            default:
                                return i18n_get(I18N_HTTP_UNKNOWN_RESPONSE_CODE);
                        }
                    } else if(res >= R_APP_CURL_ERROR_BASE && res < R_APP_CURL_ERROR_END) {
                        const char* detail = http_curl_detail_message();
                        if(detail[0] != '\0') {
                            snprintf(descriptionString, sizeof(descriptionString), "%s (%s)", curl_easy_strerror(res - R_APP_CURL_ERROR_BASE), detail);
                            return descriptionString;
                        }

                        return curl_easy_strerror(res - R_APP_CURL_ERROR_BASE);
                    }

                    break;
            }
        default:
            break;
    }

    switch(description) {
        case RD_SUCCESS:
            return i18n_get(I18N_SUCCESS);
        case RD_TIMEOUT:
            return i18n_get(I18N_TIMEOUT);
        case RD_OUT_OF_RANGE:
            return i18n_get(I18N_OUT_OF_RANGE);
        case RD_ALREADY_EXISTS:
            return i18n_get(I18N_ALREADY_EXISTS);
        case RD_CANCEL_REQUESTED:
            return i18n_get(I18N_CANCEL_REQUESTED);
        case RD_NOT_FOUND:
            return i18n_get(I18N_NOT_FOUND);
        case RD_ALREADY_INITIALIZED:
            return i18n_get(I18N_ALREADY_INITIALIZED);
        case RD_NOT_INITIALIZED:
            return i18n_get(I18N_NOT_INITIALIZED);
        case RD_INVALID_HANDLE:
            return i18n_get(I18N_INVALID_HANDLE);
        case RD_INVALID_POINTER:
            return i18n_get(I18N_INVALID_POINTER);
        case RD_INVALID_ADDRESS:
            return i18n_get(I18N_INVALID_ADDRESS);
        case RD_NOT_IMPLEMENTED:
            return i18n_get(I18N_NOT_IMPLEMENTED);
        case RD_OUT_OF_MEMORY:
            return i18n_get(I18N_OUT_OF_MEMORY);
        case RD_MISALIGNED_SIZE:
            return i18n_get(I18N_MISALIGNED_SIZE);
        case RD_MISALIGNED_ADDRESS:
            return i18n_get(I18N_MISALIGNED_ADDRESS);
        case RD_BUSY:
            return i18n_get(I18N_BUSY);
        case RD_NO_DATA:
            return i18n_get(I18N_NO_DATA);
        case RD_INVALID_COMBINATION:
            return i18n_get(I18N_INVALID_COMBINATION);
        case RD_INVALID_ENUM_VALUE:
            return i18n_get(I18N_INVALID_ENUM_VALUE);
        case RD_INVALID_SIZE:
            return i18n_get(I18N_INVALID_SIZE);
        case RD_ALREADY_DONE:
            return i18n_get(I18N_ALREADY_DONE);
        case RD_NOT_AUTHORIZED:
            return i18n_get(I18N_NOT_AUTHORIZED);
        case RD_TOO_LARGE:
            return i18n_get(I18N_TOO_LARGE);
        case RD_INVALID_SELECTION:
            return i18n_get(I18N_INVALID_SELECTION);
        default:
            return i18n_get(I18N_UNKNOWN);
    }
}

typedef struct {
    char fullText[4096];
    void* data;
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
} error_data;

static void error_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    error_data* errorData = (error_data*) data;

    if(errorData->drawTop != NULL) {
        errorData->drawTop(view, errorData->data, x1, y1, x2, y2);
    }
}

static void error_onresponse(ui_view* view, void* data, u32 response) {
    free(data);
}

ui_view* error_display(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    va_list list;
    va_start(list, text);
    vsnprintf(errorData->fullText, 4096, text, list);
    va_end(list);

    return prompt_display_notify(i18n_get(I18N_ERROR), errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}

ui_view* error_display_res(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    int level = R_LEVEL(result);
    int summary = R_SUMMARY(result);
    int module = R_MODULE(result);
    int description = R_DESCRIPTION(result);

    snprintf(errorData->fullText, 4096, i18n_get(I18N_S_RESULT_CODE_0X_08LX_LEVEL_S_D_SUMMARY_S), textBuf, result, level_to_string(result), level, summary_to_string(result), summary, module_to_string(result), module, description_to_string(result), description);

    return prompt_display_notify(i18n_get(I18N_ERROR), errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}

ui_view* error_display_errno(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    if(errorData == NULL) {
        // No use trying to spawn another if we're out of memory.
        return NULL;
    }

    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    if(err < 0) {
        err = -err;
    }

    snprintf(errorData->fullText, 4096, i18n_get(I18N_IO_ERROR_S_D), textBuf, strerror(err), err);

    return prompt_display_notify(i18n_get(I18N_ERROR), errorData->fullText, COLOR_TEXT, errorData, error_draw_top, error_onresponse);
}
