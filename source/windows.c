/**
 * CleanRip - main.c
 * Copyright (C) 2010-2026 emu_kidid
 *
 * Main driving code behind the disc ripper
 *
 * CleanRip homepage: https://github.com/emukidid/cleanrip
 * email address: emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <stdarg.h>
#include "ios.h"
#include "crc32.h"
#include "sha1.h"
#include "md5.h"
#ifdef __CYGWIN__
#include <windows.h>
#include <winioctl.h>
#include <stdint.h>
#include <time.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef volatile u32 vu32;
typedef u32 sec_t;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#define ATTRIBUTE_ALIGN(x)

typedef struct {
    int x, y, w, h;
    int viTVMode;
    int fbWidth;
    int efbHeight;
    int xfbHeight;
} GXRModeObj;

typedef struct {
    bool (*startup)(void);
    bool (*isInserted)(void);
    bool (*readSectors)(sec_t, sec_t, void*);
    bool (*writeSectors)(sec_t, sec_t, void*);
    bool (*clearStatus)(void);
    bool (*shutdown)(void);
} DISC_INTERFACE;

typedef struct {
    char* name;
} ntfs_md;

typedef void* mqbox_t;
typedef pthread_t lwp_t;
typedef void* mqmsg_t;

#define MQ_MSG_BLOCK 0
#define SYS_POWEROFF 0
#define VI_NON_INTERLACE 0
#define GX_CULL_NONE 0
#define GX_TRUE 1
#define NTFS_DEFAULT 0
#define NTFS_RECOVER 0
#define COLOR_BLACK 0

#define PAD_BUTTON_LEFT 0x0001
#define PAD_BUTTON_RIGHT 0x0002
#define PAD_BUTTON_DOWN 0x0004
#define PAD_BUTTON_UP 0x0008
#define PAD_TRIGGER_Z 0x0010
#define PAD_TRIGGER_R 0x0020
#define PAD_TRIGGER_L 0x0040
#define PAD_BUTTON_A 0x0100
#define PAD_BUTTON_B 0x0200
#define PAD_BUTTON_X 0x0400
#define PAD_BUTTON_Y 0x0800
#define PAD_BUTTON_START 0x1000

typedef struct {
    u8 r, g, b, a;
} GXColor;

#define MEM_K0_TO_K1(x) (x)

enum {
	TYPE_USB = 0,
	TYPE_SD,
	TYPE_READONLY
};
static const DISC_INTERFACE* sdcard = NULL;
static const DISC_INTERFACE* usb = NULL;
static HANDLE hSourceDrive = INVALID_HANDLE_VALUE;

#else
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/pad.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/message.h>
#include <fat.h>
#include <ntfs.h>
#endif
#include <time.h>
#include <stdbool.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#include <ogc/es.h>
#include <ogc/conf.h>
#include <ogc/usbstorage.h>
#include <sdcard/wiisd_io.h>
#endif

u64 gettime();
u32 diff_msec(u64 start, u64 end);
u32 diff_sec(u64 start, u64 end);

#define V_MAJOR 2
#define V_MID 5
#define V_MINOR 0

#define READ_SIZE (1024*1024)
#define ONE_GIGABYTE (1024*1024*1024)

#define WII_D1_SIZE 0x118240
#define WII_D5_SIZE 0x1182400
#define WII_D9_SIZE 0x1F80A00
#define NGC_DISC_SIZE 0x1182400

enum {
	IS_NGC_DISC = 0,
	IS_WII_DISC,
	IS_DATEL_DISC,
	IS_OTHER_DISC,
	IS_UNK_DISC
};

enum {
	NO_DISC = -1,
	CANCELLED = -2,
};

enum {
	TYPE_FAT = 0,
	TYPE_NTFS
};

enum {
	NOT_ASKED = -1,
	ANSWER_NO = 0,
	ANSWER_YES = 1
};

enum {
	VERIFY_INTERNAL_CRC = 0,
	VERIFY_INTERNAL_MD5,
	VERIFY_REDUMP_CRC,
	VERIFY_REDUMP_MD5
};

enum {
	NGC_UNCOMPRESSED = 0,
	NGC_COMPRESSED,
	NGC_RESERVED1,
	NGC_RESERVED2,
	WII_DUAL_LAYER,
	WII_CHUNK_SIZE,
	WII_NEWFILE,
	AUDIO_OUTPUT,
	MAX_OPTIONS
};

#define MAX_NGC_OPTIONS 4
#define MAX_WII_OPTIONS 4

enum {
	AUTO_DETECT = 0,
	SINGLE_MINI,
	SINGLE_LAYER,
	DUAL_LAYER,
	DUAL_DELIM
};

enum {
	CHUNK_1GB = 0,
	CHUNK_2GB,
	CHUNK_3GB,
	CHUNK_MAX,
	CHUNK_DELIM
};

enum {
	ASK_USER = 0,
	AUTO_CHUNK,
	NEWFILE_DELIM
};

enum {
	AUDIO_OUT_BIN = 0,
	AUDIO_OUT_WAV,
	AUDIO_OUT_WAV_FAST,
	AUDIO_OUT_WAV_BEST,
	AUDIO_OUT_DELIM
};

#define DEFAULT_FIFO_SIZE    (256*1024)//(64*1024) minimum
static ntfs_md *mounts = NULL;

#ifdef HW_RVL
#ifdef __CYGWIN__
static const DISC_INTERFACE* sdcard = NULL;
static const DISC_INTERFACE* usb = NULL;
#else
static const DISC_INTERFACE* sdcard = &__io_wiisd;
static const DISC_INTERFACE* usb = &__io_usbstorage;
#endif
enum {
	TYPE_USB = 0,
	TYPE_SD,
	TYPE_READONLY
};
enum {
	SRC_INTERNAL_DISC = 0,
	SRC_USB_DRIVE
};
#endif
#ifdef HW_DOL
#include <sdcard/gcsd.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
static int sdcard_slot = 0;
static DISC_INTERFACE* sdcard = NULL;
static DISC_INTERFACE* m2loader = &__io_m2ldr;
enum {
	TYPE_SD = 0,
	TYPE_M2LOADER,
	TYPE_READONLY
};
#endif

static int selected_device = 0;
#ifdef HW_RVL
static int selected_source = SRC_INTERNAL_DISC;
#endif
static int calcChecksums = 0;
static int dumpCounter = 0;
static char gameName[32];
static char internalName[512];
static char mountPath[512];
static char wpadNeedScan = 0;
static char padNeedScan = 0;
char txtbuffer[2048];
int print_usb = 0;
int shutdown = 0;
int whichfb = 0;
int isDumping = 0;
u32 iosversion = -1;
int verify_type_in_use = 0;
GXRModeObj *vmode = NULL;
u32 *xfb[2] = { NULL, NULL };
int options_map[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int newProgressDisplay = 1;
static int forced_disc_profile = 0;
static u32 forced_audio_sector_size = 0;

#define NGC_MAGIC 0xC2339F3D
#define WII_MAGIC 0x5D1C9EA3
#define B_SELECTED 1
#define B_NOSELECT 0
static u32 defaultColor = 0xFFFFFFFF;

static char selected_source_drive_letter = 0;

// Definitions for Raw Audio Read
#define IOCTL_CDROM_READ_RAW_AUDIO_LOCAL 0x2403E

typedef enum {
    YellowMode2_Local,
    XAForm2_Local,
    CDDA_Local
} TRACK_MODE_TYPE_LOCAL;

typedef struct {
    LARGE_INTEGER DiskOffset;
    ULONG SectorCount;
    TRACK_MODE_TYPE_LOCAL TrackMode;
} RAW_READ_INFO_LOCAL;

// Definitions for TOC
#define IOCTL_CDROM_READ_TOC_LOCAL 0x00024000
#define MAXIMUM_NUMBER_TRACKS_LOCAL 100

typedef struct _TRACK_DATA_LOCAL {
    UCHAR Reserved;
    UCHAR Control : 4;
    UCHAR Adr : 4;
    UCHAR TrackNumber;
    UCHAR Reserved1;
    UCHAR Address[4];
} TRACK_DATA_LOCAL, *PTRACK_DATA_LOCAL;

typedef struct _CDROM_TOC_LOCAL {
    UCHAR Length[2];
    UCHAR FirstTrack;
    UCHAR LastTrack;
    TRACK_DATA_LOCAL TrackData[MAXIMUM_NUMBER_TRACKS_LOCAL];
} CDROM_TOC_LOCAL, *PCDROM_TOC_LOCAL;

void DrawFrameStart();
void DrawFrameFinish();
void DrawEmptyBox(int x, int y, int width, int height, u32 color);
void DrawSelectableButton(int x, int y, int width, int height, char *message, int selected, int id);
void WriteCentre(int y, char *string);
u32 get_buttons_pressed();

static int select_source_drive() {
    char drive_path[32];
    char root_path[4] = "A:\\";
    char available_drives[26];
    int num_drives = 0;
    int i;

    // Scan for CD/DVD drives
    for (i = 0; i < 26; i++) {
        root_path[0] = 'A' + i;
        if (GetDriveType(root_path) == DRIVE_CDROM) {
            available_drives[num_drives++] = 'A' + i;
        }
    }

    if (num_drives == 0) {
        DrawFrameStart();
        DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
        WriteCentre(255, "No optical drives found!");
        DrawFrameFinish();
        sleep(2);
        return 0;
    }

    int selected_index = 0;
    
    while ((get_buttons_pressed() & PAD_BUTTON_A));
    while (1) {
        DrawFrameStart();
        DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
        WriteCentre(255, "Please select the source drive");
        
        sprintf(drive_path, "Drive %c:", available_drives[selected_index]);
        DrawSelectableButton(200, 310, -1, 340, drive_path, B_SELECTED, -1);
        
        DrawFrameFinish();
        
        while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_B)));
        u32 btns = get_buttons_pressed();
        
        if (btns & PAD_BUTTON_RIGHT) {
            selected_index++;
            if (selected_index >= num_drives) selected_index = 0;
        }
        if (btns & PAD_BUTTON_LEFT) {
            selected_index--;
            if (selected_index < 0) selected_index = num_drives - 1;
        }
        if (btns & PAD_BUTTON_A) {
            selected_source_drive_letter = available_drives[selected_index];
            break;
        }
        if (btns & PAD_BUTTON_B) return 0;

        while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_B)));
    }
    while ((get_buttons_pressed() & PAD_BUTTON_A));
    return 1;
}

int init_dvd(bool prompt) {
    if (hSourceDrive != INVALID_HANDLE_VALUE) {
        CloseHandle(hSourceDrive);
        hSourceDrive = INVALID_HANDLE_VALUE;
    }
    if (prompt || selected_source_drive_letter == 0) {
        if (!select_source_drive()) return CANCELLED;
    }
    {
        char path[32];
        sprintf(path, "\\\\.\\%c:", selected_source_drive_letter);
        hSourceDrive = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hSourceDrive == INVALID_HANDLE_VALUE) return NO_DISC;
        DWORD bytesReturned;
        if (!DeviceIoControl(hSourceDrive, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
            CloseHandle(hSourceDrive);
            hSourceDrive = INVALID_HANDLE_VALUE;
            return NO_DISC;
        }
        return 0;
    }
    return NO_DISC;
}
const char* dvd_error_str() { return "No Error"; }
u32 dvd_get_error() { return 0; }
int DVD_LowRead64(void *buf, u32 len, u64 offset) {
    if (hSourceDrive == INVALID_HANDLE_VALUE) return -1;

    // Try IOCTL for Audio CD (2352 byte sectors)
    if (len % 2352 == 0) {
        u32 done = 0;
        while (done < len) {
            // Limit chunk size to ~64KB (27 sectors) to avoid driver transfer limits
            u32 chunk = len - done;
            if (chunk > (2352 * 27)) chunk = 2352 * 27;

            RAW_READ_INFO_LOCAL rawReadInfo;
            // Windows expects 2048-byte sector addressing for DiskOffset
            rawReadInfo.DiskOffset.QuadPart = ((offset + done) / 2352) * 2048;
            rawReadInfo.SectorCount = chunk / 2352;
            rawReadInfo.TrackMode = CDDA_Local;

            DWORD bytesReturned;
            if (!DeviceIoControl(hSourceDrive, IOCTL_CDROM_READ_RAW_AUDIO_LOCAL, 
                                &rawReadInfo, sizeof(rawReadInfo), 
                                (u8*)buf + done, chunk, 
                                &bytesReturned, NULL)) {
                break;
            }
            done += chunk;
        }
        if (done == len) return 0;
    }

    LARGE_INTEGER li;
    li.QuadPart = offset;
    if (SetFilePointer(hSourceDrive, li.LowPart, &li.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) return -1;
    DWORD bytesRead;
    if (!ReadFile(hSourceDrive, buf, len, &bytesRead, NULL) || bytesRead != len) return -1;
    return 0;
}
int DVD_LowRead64Datel(void *buf, u32 len, u64 offset, int isKnown) { return 0; }
void dvd_motor_off(int eject) {}
void dvd_read_bca(void *buf) {}

void DrawFrameStart() { printf("\033[2J\033[1;1H"); }
void DrawFrameFinish() { fflush(stdout); }
void DrawEmptyBox(int x, int y, int width, int height, u32 color) {}
void DrawSelectableButton(int x, int y, int width, int height, char *message, int selected, int id) { printf("%s %s\n", selected ? "->" : "  ", message); }
void DrawAButton(int x, int y) { printf("[A] "); }
void DrawBButton(int x, int y) { printf("[B] "); }
void WriteFont(int x, int y, char *string) { printf("%s\n", string); }
void WriteFontStyled(int x, int y, char *string, float size, bool centered, u32 color) { printf("%s\n", string); }
void WriteCentre(int y, char *string) { printf("%s\n", string); }
void init_font() {}
void init_textures() {}
int DrawYesNoDialog(char *message, char *message2) { return 1; }
void DrawProgressBar(int percent, char *message, int disc_type) {
    printf("Progress: [");
    for (int i = 0; i < 50; i++) {
        if (i < percent / 2) printf("=");
        else printf(" ");
    }
    printf("] %d%%\n", percent);
    printf("%s\n", message);
}
void DrawProgressDetailed(int percent, char *message, int mb_done, int mb_total, char* discTypeStr, int showChecksums, int disc_type) {
    printf("Ripping %s\n", discTypeStr);
    printf("Progress: [");
    for (int i = 0; i < 50; i++) {
        if (i < percent / 2) printf("=");
        else printf(" ");
    }
    printf("] %d%%\n", percent);
    printf("Size: %d / %d MB\n", mb_done, mb_total);
    printf("%s\n", message);
    if (showChecksums) printf("Checksums: Enabled\n");
}

#ifdef __CYGWIN__
static struct termios orig_termios;
static int term_setup = 0;
void disable_raw_mode() { if (term_setup) { tcsetattr(0, TCSANOW, &orig_termios); term_setup = 0; } }
void enable_raw_mode() { if (!term_setup) { tcgetattr(0, &orig_termios); atexit(disable_raw_mode); struct termios raw = orig_termios; raw.c_lflag &= ~(ECHO | ICANON); raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0; tcsetattr(0, TCSANOW, &raw); term_setup = 1; } }

static u32 current_pad_buttons = 0;
static u64 last_key_time = 0;

void PAD_Init() { enable_raw_mode(); }
u32 PAD_ButtonsDown(int pad) { return current_pad_buttons; }
void PAD_ScanPads() {
    unsigned char c;
    if (read(0, &c, 1) == 1) {
        current_pad_buttons = 0;
        if (c == 27) {
            unsigned char seq[2];
            if (read(0, seq, 2) == 2) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A') current_pad_buttons |= PAD_BUTTON_UP;
                    if (seq[1] == 'B') current_pad_buttons |= PAD_BUTTON_DOWN;
                    if (seq[1] == 'C') current_pad_buttons |= PAD_BUTTON_RIGHT;
                    if (seq[1] == 'D') current_pad_buttons |= PAD_BUTTON_LEFT;
                } else {
                    current_pad_buttons |= PAD_BUTTON_B;
                }
            }
        } else {
            if (c == 'a' || c == 'A' || c == '\n') current_pad_buttons |= PAD_BUTTON_A;
            if (c == 'b' || c == 'B') current_pad_buttons |= PAD_BUTTON_B;
            if (c == 'x' || c == 'X') current_pad_buttons |= PAD_BUTTON_X;
            if (c == 'y' || c == 'Y') current_pad_buttons |= PAD_BUTTON_Y;
            if (c == 'q' || c == 'Q') shutdown = 1;
        }
        last_key_time = gettime();
    } else if (diff_msec(last_key_time, gettime()) > 100) {
        current_pad_buttons = 0;
    }
    usleep(10000);
}

void VIDEO_Init() {}
GXRModeObj* VIDEO_GetPreferredMode(void* mode) { 
    static GXRModeObj m = {0,0,640,480,0,640,480,480}; 
    return &m; 
}
void VIDEO_Configure(GXRModeObj* mode) {}
void* SYS_AllocateFramebuffer(GXRModeObj* mode) { return malloc(640*480*4); }
void VIDEO_ClearFrameBuffer(GXRModeObj* mode, void* fb, u32 color) {}
void VIDEO_SetNextFramebuffer(void* fb) {}
void VIDEO_SetPostRetraceCallback(void (*callback)(u32)) {}
void VIDEO_SetBlack(bool black) {}
void VIDEO_Flush() {}
void VIDEO_WaitVSync() {}
void GX_Init(void* fifo, u32 size) {}
void GX_SetCopyClear(GXColor color, u32 z) {}
void GX_SetViewport(f32 x, f32 y, f32 w, f32 h, f32 n, f32 f) {}
void GX_SetDispCopyYScale(f32 y) {}
void GX_SetDispCopyDst(u16 w, u16 h) {}
void GX_SetCullMode(u8 mode) {}
void GX_CopyDisp(void* dest, u8 clear) {}

typedef struct {
    mqmsg_t* msg_queue;
    int q_size;
    int head;
    int tail;
    sem_t sem_full;
    sem_t sem_empty;
    pthread_mutex_t mutex;
} mq_obj_t;

void LWP_SetThreadPriority(lwp_t thread, u32 prio) {}
void LWP_CreateThread(lwp_t* thread, void* (*func)(void*), void* arg, void* stack, u32 stack_size, u32 prio) {
    pthread_create(thread, NULL, func, arg);
}
void MQ_Init(mqbox_t* mq, u32 count) {
    mq_obj_t* mq_obj = (mq_obj_t*)malloc(sizeof(mq_obj_t));
    mq_obj->msg_queue = (mqmsg_t*)malloc(sizeof(mqmsg_t) * count);
    mq_obj->q_size = count;
    mq_obj->head = 0;
    mq_obj->tail = 0;
    sem_init(&mq_obj->sem_full, 0, 0);
    sem_init(&mq_obj->sem_empty, 0, count);
    pthread_mutex_init(&mq_obj->mutex, NULL);
    *mq = mq_obj;
}
bool MQ_Receive(mqbox_t mq, mqmsg_t* msg, u32 flags) {
    mq_obj_t* mq_obj = (mq_obj_t*)mq;
    if (sem_wait(&mq_obj->sem_full) != 0) return false;
    pthread_mutex_lock(&mq_obj->mutex);
    *msg = mq_obj->msg_queue[mq_obj->head];
    mq_obj->head = (mq_obj->head + 1) % mq_obj->q_size;
    pthread_mutex_unlock(&mq_obj->mutex);
    sem_post(&mq_obj->sem_empty);
    return true;
}
bool MQ_Send(mqbox_t mq, mqmsg_t msg, u32 flags) {
    mq_obj_t* mq_obj = (mq_obj_t*)mq;
    if (sem_wait(&mq_obj->sem_empty) != 0) return false;
    pthread_mutex_lock(&mq_obj->mutex);
    mq_obj->msg_queue[mq_obj->tail] = msg;
    mq_obj->tail = (mq_obj->tail + 1) % mq_obj->q_size;
    pthread_mutex_unlock(&mq_obj->mutex);
    sem_post(&mq_obj->sem_full);
    return true;
}
void MQ_Jam(mqbox_t mq, mqmsg_t msg, u32 flags) {
    // This is not a real jam, but it will work for the error case.
    MQ_Send(mq, msg, flags);
}
void MQ_Close(mqbox_t mq) {
    mq_obj_t* mq_obj = (mq_obj_t*)mq;
    free(mq_obj->msg_queue);
    free(mq_obj);
}
void LWP_JoinThread(lwp_t thread, void** value_ptr) { pthread_join(thread, value_ptr); }
void LWP_YieldThread() { sched_yield(); }
int fatMountSimple(const char* name, const DISC_INTERFACE* disc_if) { return 1; }
void fatUnmount(const char* name) {}
int ntfsMountDevice(const DISC_INTERFACE* disc_if, ntfs_md** mounts, u32 flags) { return 0; }
void ntfsUnmount(const char* name, bool force) {}
char* ntfsGetVolumeName(const char* name) { return "NTFS"; }
void DCZeroRange(void* addr, u32 len) {}
void DCFlushRange(void* addr, u32 len) {}
#endif

u64 gettime() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (u64)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
u32 diff_msec(u64 start, u64 end) {
	return (u32)((end - start) / 1000000ULL);
}
u32 diff_sec(u64 start, u64 end) {
	return (u32)((end - start) / 1000000000ULL);
}

void verify_init(char *path) {}
void verify_download(char *path) {}
int verify_is_available(int disc_type) { return 0; }
int verify_findMD5Sum(char *md5, int disc_type) { return 0; }
int verify_findCrc32(u32 crc32, int disc_type) { return 0; }
char* verify_get_name(int type) { return "Unknown"; }

void datel_init(char *path) {}
void datel_download(char *path) {}
int datel_findMD5Sum(char *md5) { return 0; }
int datel_findCrcSum(u32 crc) { return 0; }
char* datel_get_name(int type) { return "Unknown"; }
void dump_skips(char *path, u32 crc) {}

enum forcedDiscProfile {
	FORCED_DISC_NONE = 0,
	FORCED_DVD_VIDEO_SL,
	FORCED_DVD_VIDEO_DL,
	FORCED_MINI_DVD,
	FORCED_AUDIO_CD
};

static u32 get_forced_disc_sector_size() {
	if (forced_disc_profile == FORCED_AUDIO_CD) {
		return forced_audio_sector_size ? forced_audio_sector_size : 2048;
	}
	return 2048;
}

static u32 get_forced_disc_end_sectors() {
	switch (forced_disc_profile) {
	case FORCED_DVD_VIDEO_SL:
		return WII_D5_SIZE;
	case FORCED_DVD_VIDEO_DL:
		return WII_D9_SIZE;
	case FORCED_MINI_DVD:
		return WII_D1_SIZE;
	case FORCED_AUDIO_CD:
		// 80 min audio CD @ 75 sectors/sec
		return 360000;
	default:
		return WII_D5_SIZE;
	}
}

static const char *get_output_extension(int disc_type) {
	if (disc_type == IS_OTHER_DISC && forced_disc_profile == FORCED_AUDIO_CD) {
		return (options_map[AUDIO_OUTPUT] == AUDIO_OUT_WAV || options_map[AUDIO_OUTPUT] == AUDIO_OUT_WAV_FAST || options_map[AUDIO_OUTPUT] == AUDIO_OUT_WAV_BEST) ? ".wav" : ".bin";
	}
	return ".iso";
}

static void sanitize_game_name() {
	int has_valid = 0;
	for (int i = 0; i < 31 && gameName[i]; i++) {
		char c = gameName[i];
		int ok = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.');
		if (!ok) {
			gameName[i] = '_';
		}
		else {
			has_valid = 1;
		}
	}
	if (!gameName[0] || !has_valid) {
		sprintf(&gameName[0], "disc%i", dumpCounter);
	}
}

static u32 detect_audio_cd_size_sectors(u32 sector_size) {
	(void)sector_size;

    if (hSourceDrive != INVALID_HANDLE_VALUE) {
        CDROM_TOC_LOCAL toc;
        DWORD bytesReturned;
        if (DeviceIoControl(hSourceDrive, IOCTL_CDROM_READ_TOC_LOCAL, NULL, 0, &toc, sizeof(toc), &bytesReturned, NULL)) {
            int leadOutIndex = toc.LastTrack - toc.FirstTrack + 1;
            if (leadOutIndex < MAXIMUM_NUMBER_TRACKS_LOCAL) {
                TRACK_DATA_LOCAL *tr = &toc.TrackData[leadOutIndex];
                u32 frames = (tr->Address[1] * 60 + tr->Address[2]) * 75 + tr->Address[3];
                if (frames >= 150) return frames - 150;
                return frames;
            }
        }
    }
	return 360000;
}

enum {
	MSG_SETFILE,
	MSG_WRITE,
	MSG_FLUSH,
};

typedef union _writer_msg {
	struct {
		int command;
		void* data;
		u32 length;
		mqbox_t ret_box;
	};
	uint8_t pad[32]; // pad to 32 bytes for alignment
} writer_msg;

static void* writer_thread(void* _msgq) {
	FILE* fp = NULL;
	mqbox_t msgq = (mqbox_t)_msgq;
	writer_msg* msg;

	// stupid libogc returns TRUE even if the message queue gets destroyed while waiting
	while (MQ_Receive(msgq, (mqmsg_t*)&msg, MQ_MSG_BLOCK)==TRUE && msg) {
		switch (msg->command) {
			case MSG_SETFILE:
				fp = (FILE*)msg->data;
				break;
			case MSG_WRITE:
				if(selected_device != TYPE_READONLY) {
					if (fp && fwrite(msg->data, msg->length, 1, fp)!=1) {
						// write error, signal it by pushing a NULL message to the front
						MQ_Jam(msg->ret_box, (mqmsg_t)NULL, MQ_MSG_BLOCK);
						return NULL;
					}
				}
				// release the block so it can be reused
				MQ_Send(msg->ret_box, (mqmsg_t)msg, MQ_MSG_BLOCK);
				break;
			case MSG_FLUSH:
				*(vu32*)msg->data = 1;
				break;
		}
	}

	return msg;
}


void print_gecko(const char* fmt, ...)
{
#if defined(HW_RVL) || defined(HW_DOL)
	if(print_usb) {
		char tempstr[2048];
		va_list arglist;
		va_start(arglist, fmt);
		vsprintf(tempstr, fmt, arglist);
		va_end(arglist);
		usb_sendbuffer_safe(1,tempstr,strlen(tempstr));
	}
#else
	va_list arglist;
	va_start(arglist, fmt);
	vprintf(fmt, arglist);
	va_end(arglist);
#endif
}


void check_exit_status() {
#ifdef HW_DOL
	if(shutdown == 1 || shutdown == 2)
		exit(0);
#endif
#ifdef HW_RVL
	if (shutdown == 1) {//Power off System
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	if (shutdown == 2) { //Return to HBC/whatever
		void (*rld)() = (void(*)()) 0x80001800;
		rld();
	}
#else
	if (shutdown)
		exit(0);
#endif
}

#ifdef HW_RVL
u32 get_wii_buttons_pressed(u32 buttons) {
	WPADData *wiiPad;
	if (wpadNeedScan) {
		WPAD_ScanPads();
		wpadNeedScan = 0;
	}
	wiiPad = WPAD_Data(0);

	if (wiiPad->btns_h & WPAD_BUTTON_B) {
		buttons |= PAD_BUTTON_B;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_A) {
		buttons |= PAD_BUTTON_A;
	}
	
	if (wiiPad->btns_h & WPAD_BUTTON_1) {
		buttons |= PAD_BUTTON_Y;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_LEFT) {
		buttons |= PAD_BUTTON_LEFT;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_RIGHT) {
		buttons |= PAD_BUTTON_RIGHT;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_UP) {
		buttons |= PAD_BUTTON_UP;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_DOWN) {
		buttons |= PAD_BUTTON_DOWN;
	}

	if (wiiPad->btns_h & WPAD_BUTTON_HOME) {
		shutdown = 2;
	}
	return buttons;
}
#endif

u32 get_buttons_pressed() {
	u32 buttons = 0;

#ifdef __CYGWIN__
    PAD_ScanPads();
#else
	if (padNeedScan) {
		PAD_ScanPads();
		padNeedScan = 0;
	}
#endif

#ifdef HW_RVL
	buttons = get_wii_buttons_pressed(buttons);
#endif

	u16 gcPad = PAD_ButtonsDown(0);

	if (gcPad & PAD_BUTTON_B) {
		buttons |= PAD_BUTTON_B;
	}
	
	if (gcPad & PAD_BUTTON_Y) {
		buttons |= PAD_BUTTON_Y;
	}

	if (gcPad & PAD_BUTTON_A) {
		buttons |= PAD_BUTTON_A;
	}

	if (gcPad & PAD_BUTTON_LEFT) {
		buttons |= PAD_BUTTON_LEFT;
	}

	if (gcPad & PAD_BUTTON_RIGHT) {
		buttons |= PAD_BUTTON_RIGHT;
	}

	if (gcPad & PAD_BUTTON_UP) {
		buttons |= PAD_BUTTON_UP;
	}

	if (gcPad & PAD_BUTTON_DOWN) {
		buttons |= PAD_BUTTON_DOWN;
	}

	if (gcPad & PAD_TRIGGER_Z) {
		shutdown = 2;
	}
	check_exit_status();
	return buttons;
}

void wait_press_A(char* text) {
	// Draw the A button
	WriteFont(210, 315, "Press");
	DrawAButton(285, 310);
	WriteFont(330, 315, text);
	DrawFrameFinish();
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (!(get_buttons_pressed() & PAD_BUTTON_A));
}

void wait_press_A_exit_B(bool tryAgain) {
	// Draw the A and B buttons
	DrawAButton(195, 310);
	DrawBButton(390, 310);
	WriteFont(120, 315, "Press");
	WriteFont(235, 315, tryAgain ? "to retry" : "to continue");
	WriteFont(435, 315, "to exit");
	DrawFrameFinish();
	while ((get_buttons_pressed() & (PAD_BUTTON_A | PAD_BUTTON_B)));
	while (1) {
		while (!(get_buttons_pressed() & (PAD_BUTTON_A | PAD_BUTTON_B)));
		if (get_buttons_pressed() & PAD_BUTTON_A) {
			break;
		}
		else if (get_buttons_pressed() & PAD_BUTTON_B) {
			print_gecko("Exit\r\n");
			exit(0);
		}
	}
}

static void InvalidatePADS(u32 retrace) {
	padNeedScan = wpadNeedScan = 1;
}

void ShutdownWii() {
	shutdown = 1;
}

/* start up the GameCube/Wii */
static void Initialise() {
#ifdef HW_RVL
	disable_ahbprot();
#endif
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();
#ifdef HW_RVL
	CONF_Init();
	WPAD_Init();
	WPAD_SetIdleTimeout(120);
	WPAD_SetPowerButtonCallback((WPADShutdownCallback) ShutdownWii);
	SYS_SetPowerCallback(ShutdownWii);
#endif

	vmode = VIDEO_GetPreferredMode(NULL);
	VIDEO_Configure(vmode);
	xfb[0] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetPostRetraceCallback(InvalidatePADS);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	// setup the fifo and then init GX
	void *gp_fifo = NULL;
	gp_fifo = MEM_K0_TO_K1 (memalign (32, DEFAULT_FIFO_SIZE));
	memset (gp_fifo, 0, DEFAULT_FIFO_SIZE);
	GX_Init (gp_fifo, DEFAULT_FIFO_SIZE);
	// clears the bg to color and clears the z buffer
	GX_SetCopyClear ((GXColor){0,0,0,255}, 0x00000000);
	// init viewport
	GX_SetViewport (0, 0, vmode->fbWidth, vmode->efbHeight, 0, 1);
	// Set the correct y scaling for efb->xfb copy operation
	GX_SetDispCopyYScale ((f32) vmode->xfbHeight / (f32) vmode->efbHeight);
	GX_SetDispCopyDst (vmode->fbWidth, vmode->xfbHeight);
	GX_SetCullMode (GX_CULL_NONE); // default in rsp init
	GX_CopyDisp (xfb[0], GX_TRUE); // This clears the efb
	GX_CopyDisp (xfb[0], GX_TRUE); // This clears the xfb

	init_font();
	init_textures();
	whichfb = 0;
}

#ifdef HW_RVL
/* FindIOS - borrwed from Tantric */
static int FindIOS(u32 ios) {
	s32 ret;
	u32 n;

	u64 *titles = NULL;
	u32 num_titles = 0;

	ret = ES_GetNumTitles(&num_titles);
	if (ret < 0)
		return 0;

	if (num_titles < 1)
		return 0;

	titles = (u64 *) memalign(32, num_titles * sizeof(u64) + 32);
	if (!titles)
		return 0;

	ret = ES_GetTitles(titles, num_titles);
	if (ret < 0) {
		free(titles);
		return 0;
	}

	for (n = 0; n < num_titles; n++) {
		if ((titles[n] & 0xFFFFFFFF) == ios) {
			free(titles);
			return 1;
		}
	}
	free(titles);
    printf("CleanRip for Windows\n");
    printf("Note: This is currently a stub for compilation verification.\n");
	return 0;
}

/* check for AHBPROT & IOS58 */
static void hardware_checks() {
	if (!(AHBPROT_DISABLED || is_dolphin())) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(190, "AHBPROT check failed");
		WriteCentre(255, "Please install the latest HBC");
		WriteCentre(280, "Check the FAQ for more info");
		wait_press_A("to exit");
		exit(0);
	}

	int ios58exists = FindIOS(58);
	print_gecko("IOS 58 Exists: %s\r\n", ios58exists ? "YES":"NO");
	if (ios58exists && iosversion != 58) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(190, "IOS Version check failed");
		WriteCentre(255, "IOS58 exists but is not in use");
		WriteCentre(280, "Loading IOS58 for USB 2.0 speeds");
		sleep(1);
		WPAD_Shutdown(); // Shut down Wii Remote input (WPAD)
		IOS_ReloadIOS(58);
		iosversion = 58;
		disable_ahbprot(); // We must also re-run the IOS exploit, as reloading IOS re-enables AHBPROT.
		WPAD_Init(); // Reinit WPAD
		wait_press_A("to continue");
		
	}
	if (!ios58exists) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(190, "IOS Version check failed");
		WriteCentre(255, "Please install IOS58");
		WriteCentre(280, "Dumping to USB will be SLOW!");
		wait_press_A_exit_B(false);
	}
}
#endif

/* show the disclaimer */
static void show_disclaimer() {
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	WriteCentre(190, "Disclaimer");
	WriteCentre(230, "The author is not responsible for any");
	WriteCentre(255, "damage or wear that could occur to any");
	WriteCentre(280, "devices used with this program");
	DrawFrameFinish();

	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	WriteCentre(190, "Disclaimer");
	WriteCentre(230, "The author is not responsible for any");
	WriteCentre(255, "damage or wear that could occur to any");
	WriteCentre(280, "devices used with this program");
	sleep(5);
	wait_press_A_exit_B(false);
}

/* Initialise the dvd drive + disc */
static int initialise_dvd() {
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
#ifdef HW_DOL
	WriteCentre(255, "Insert a GameCube DVD Disc");
#else
	WriteCentre(255, "Insert a disc (GC/Wii/DVD/CD)");
#endif
	wait_press_A_exit_B(false);

	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	WriteCentre(255, "Initialising Disc ...");
	DrawFrameFinish();
	int ret = init_dvd(true);
    if (ret == CANCELLED) exit(0);

	while (ret == NO_DISC) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "No disc detected");
        WriteCentre(280, "Insert disc to continue");
        WriteCentre(305, "Press B to exit");
		print_gecko("No disc detected\r\n");
		DrawFrameFinish();
		
        for (int i = 0; i < 20; i++) {
            u32 btns = get_buttons_pressed();
            if (btns & PAD_BUTTON_B) exit(0);
            usleep(100000);
        }
        ret = init_dvd(false);
	}
	return ret;
}

#ifdef HW_RVL
static int initialise_source() {
	if (selected_source == SRC_USB_DRIVE) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "Initialising USB source drive ...");
		DrawFrameFinish();
		if (!usb->startup()) {
			return NO_DISC;
		}
		if (!usb->isInserted()) {
			return NO_DISC;
		}
		return 0;
	}
	return initialise_dvd();
}

static int source_read(void* dst, u32 len, u64 offset, int disc_type, int isKnownDatel) {
	if (selected_source == SRC_USB_DRIVE) {
		if ((offset & 0x1FF) || (len & 0x1FF)) {
			return 1;
		}
		return usb->readSectors((sec_t)(offset >> 9), (sec_t)(len >> 9), dst) ? 0 : 1;
	}
	if (disc_type == IS_DATEL_DISC) {
		return DVD_LowRead64Datel(dst, len, offset, isKnownDatel);
	}
	return DVD_LowRead64(dst, len, offset);
}
#else
static int initialise_source() {
	return initialise_dvd();
}

static int source_read(void* dst, u32 len, u64 offset, int disc_type, int isKnownDatel) {
	if (disc_type == IS_DATEL_DISC) {
		return DVD_LowRead64Datel(dst, len, offset, isKnownDatel);
	}
	return DVD_LowRead64(dst, len, offset);
}
#endif

#ifdef HW_DOL
int select_sd_gecko_slot() {
	int slot = 0;
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "Please select SD location");
		DrawSelectableButton(100, 310, -1, 340, "Slot A", slot == 0 ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(240, 310, -1, 340, "Slot B", slot == 1 ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(380, 310, -1, 340, "SD2SP2", slot == 2 ? B_SELECTED : B_NOSELECT, -1);
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
		u32 btns = get_buttons_pressed();
		if (btns & PAD_BUTTON_RIGHT) {
			slot++;
			if (slot > 2) slot = 0;
		}
		if (btns & PAD_BUTTON_LEFT) {
			slot--;
			if (slot < 0) slot = 2;
		}
		if (btns & PAD_BUTTON_A)
			break;
		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A));

	return slot;
}

DISC_INTERFACE* get_sd_card_handler(int slot) {
	switch (slot) {
		case 1:
			return get_io_gcsdb();
		case 2:
			return get_io_gcsd2();
		default: /* Also handles case 0 */
			return get_io_gcsda();
	}
}
#endif

#ifdef __CYGWIN__
static char selected_drive_letter = 0;

static int select_drive() {
    char drive_path[32];
    struct stat st;
    char available_drives[26];
    int num_drives = 0;
    int i;

    // Scan for drives
    for (i = 0; i < 26; i++) {
        sprintf(drive_path, "%c:/", 'A' + i);
        if (stat(drive_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            available_drives[num_drives++] = 'a' + i;
        }
    }

    if (num_drives == 0) {
        DrawFrameStart();
        DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
        WriteCentre(255, "No drives found!");
        DrawFrameFinish();
        sleep(2);
        return 0;
    }

    int selected_index = 0;
    // Default to 'c' if available
    for (i = 0; i < num_drives; i++) {
        if (available_drives[i] == 'c') {
            selected_index = i;
            break;
        }
    }

    while ((get_buttons_pressed() & PAD_BUTTON_A));
    while (1) {
        DrawFrameStart();
        DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
        WriteCentre(255, "Please select the output drive");
        
        sprintf(drive_path, "Drive %c:", available_drives[selected_index] - 32);
        DrawSelectableButton(200, 310, -1, 340, drive_path, B_SELECTED, -1);
        
        DrawFrameFinish();
        
        while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_B)));
        u32 btns = get_buttons_pressed();
        
        if (btns & PAD_BUTTON_RIGHT) {
            selected_index++;
            if (selected_index >= num_drives) selected_index = 0;
        }
        if (btns & PAD_BUTTON_LEFT) {
            selected_index--;
            if (selected_index < 0) selected_index = num_drives - 1;
        }
        if (btns & PAD_BUTTON_A) {
            selected_drive_letter = available_drives[selected_index];
            break;
        }
        if (btns & PAD_BUTTON_B) {
            return 0;
        }
        
        while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_B)));
    }
    while ((get_buttons_pressed() & PAD_BUTTON_A));
    return 1;
}
#endif

/* Initialise the device */
static int initialise_device(int fs) {
	int ret = 0;

#ifdef __CYGWIN__
    if (selected_device != TYPE_READONLY) {
        if (select_drive()) {
            sprintf(mountPath, "%c:/", selected_drive_letter - 32);
            return 1;
        }
        return 0;
    }
#endif

	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	if (selected_device == TYPE_SD) {
#ifdef HW_DOL
		sdcard_slot = select_sd_gecko_slot();
		sdcard = get_sd_card_handler(sdcard_slot);

		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
#endif
		WriteCentre(255, "Insert a SD FAT/NTFS formatted device");
	}
#ifdef HW_DOL
	else if (selected_device == TYPE_M2LOADER) {
		WriteCentre(255, "Insert a M.2 FAT/NTFS formatted device");
	}
#else
	else if (selected_device == TYPE_USB) {
		WriteCentre(255, "Insert a USB FAT/NTFS formatted device");
	}
#endif
	wait_press_A_exit_B(false);

	if (fs == TYPE_FAT) {
		switch (selected_device) {
			case TYPE_SD:
				ret = fatMountSimple("fat", sdcard);
				break;
#ifdef HW_DOL
			case TYPE_M2LOADER:
				ret = fatMountSimple("fat", m2loader);
				break;
#else
			case TYPE_USB:
				ret = fatMountSimple("fat", usb);
				break;
#endif
		}
		if (ret != 1) {
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Error Mounting Device [%08X]", ret);
			WriteCentre(255, txtbuffer);
			wait_press_A_exit_B(true);
		}
		sprintf(&mountPath[0], "fat:/");
	}
	else if (fs == TYPE_NTFS) {
		int mountCount = 0;
		switch (selected_device) {
			case TYPE_SD:
				mountCount = ntfsMountDevice(sdcard, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				break;
#ifdef HW_DOL
			case TYPE_M2LOADER:
				mountCount = ntfsMountDevice(m2loader, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				break;
#else
			case TYPE_USB:
				mountCount = ntfsMountDevice(usb, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				break;
#endif
		}

		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		if (!mountCount || mountCount == -1) {
			if (mountCount == -1) {
				sprintf(txtbuffer, "Error whilst mounting devices (%i)", errno);
			} else {
				sprintf(txtbuffer, "No NTFS volume(s) were found or mounted");
			}
			WriteCentre(255, txtbuffer);
			wait_press_A_exit_B(true);
		} else {
			sprintf(txtbuffer, "%s Mounted", ntfsGetVolumeName(mounts[0].name));
			WriteCentre(230, txtbuffer);
			sprintf(txtbuffer, "%i NTFS volume(s) mounted!", mountCount);
			WriteCentre(255, txtbuffer);
			wait_press_A("to continue");
			sprintf(&mountPath[0], "%s:/", mounts[0].name);
			ret = 1;
		}
	}
	return ret;
}

/* identify whether this disc is a Gamecube or Wii disc */
static int identify_disc() {
	char readbuf[2048] __attribute__((aligned(32)));

	memset(&internalName[0],0,512);
	memset(readbuf, 0, sizeof(readbuf));
	forced_audio_sector_size = 0;
	// Read the header
	source_read(readbuf, 2048, 0ULL, IS_OTHER_DISC, 0);
	if (readbuf[0]) {
		strncpy(&gameName[0], readbuf, 6);
		gameName[6] = 0;
		// Multi Disc identifier support
		if (readbuf[6]) {
			size_t lastPos = strlen(gameName);
			sprintf(&gameName[lastPos], "-disc%i", (readbuf[6]) + 1);
		}
		strncpy(&internalName[0],&readbuf[32],512);
		internalName[511] = '\0';
	} else {
		sprintf(&gameName[0], "disc%i", dumpCounter);
	}
	if ((*(volatile u32*)(readbuf + 0x1C)) == NGC_MAGIC) {
		print_gecko("NGC disc\r\n");
		return IS_NGC_DISC;
	}
	if ((*(volatile u32*)(readbuf + 0x18)) == WII_MAGIC) {
		print_gecko("Wii disc\r\n");
		return IS_WII_DISC;
	}
	sanitize_game_name();
	print_gecko("Unkown disc\r\n");
	return IS_UNK_DISC;
}

const char* const get_game_name() {
	return gameName;
}

/* the user must specify the disc type */
static int force_disc() {
	static const char *forcedTypeNames[] = {
		"GameCube",
		"Wii",
		"DVD-Video (single layer)",
		"DVD-Video (dual layer)",
		"MiniDVD",
		"Audio CD (experimental)"
	};
	int type = 0;
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(190, "Failed to detect the disc type");
		WriteCentre(225, "Please select a type to continue");
		WriteCentre(255, "This can be used for DVD-Video/miniDVD/audio CD");
		DrawSelectableButton(70, 310, vmode->fbWidth - 78, 340, (char*)forcedTypeNames[type],
			B_SELECTED, -1);
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)))
			;
		u32 btns = get_buttons_pressed();
		if (btns & PAD_BUTTON_RIGHT)
			type = (type + 1) % 6;
		if (btns & PAD_BUTTON_LEFT)
			type = (type == 0) ? 5 : (type - 1);
		if (btns & PAD_BUTTON_A)
			break;
		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)))
			;
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A))
		;
	forced_disc_profile = FORCED_DISC_NONE;
	forced_audio_sector_size = 0;
	if (type == 0) {
		return IS_NGC_DISC;
	}
	if (type == 1) {
		return IS_WII_DISC;
	}
	if (type == 2) {
		forced_disc_profile = FORCED_DVD_VIDEO_SL;
	}
	else if (type == 3) {
		forced_disc_profile = FORCED_DVD_VIDEO_DL;
	}
	else if (type == 4) {
		forced_disc_profile = FORCED_MINI_DVD;
	}
	else {
		forced_disc_profile = FORCED_AUDIO_CD;
		forced_audio_sector_size = 0;
	}
	return IS_OTHER_DISC;
}

/*
 Detect if a dual-layer disc was inserted by checking if reading from sectors
 on the second layer is succesful or not. Returns the correct disc size.
*/
int detect_duallayer_disc() {
	char *readBuf = (char*)memalign(32,64);
	int ret = WII_D1_SIZE;
	uint64_t offset = (uint64_t)WII_D1_SIZE << 11;
	if (source_read(readBuf, 64, offset, IS_WII_DISC, 0) == 0) {
		ret = WII_D5_SIZE;
	}
	offset = (uint64_t)WII_D5_SIZE << 11;//offsetToSecondLayer
	if (source_read(readBuf, 64, offset, IS_WII_DISC, 0) == 0) {
		ret = WII_D9_SIZE;
	}
	free(readBuf);

	print_gecko("Detect: %s\r\n", (ret == WII_D1_SIZE) ? "Wii mini DVD size"
		: (ret == WII_D5_SIZE) ? "Wii Single Layer"
		: "Wii Dual Layer");

	return ret;
}

/* the user must specify the device type */
#ifdef HW_RVL
void select_source_type() {
	selected_source = SRC_INTERNAL_DISC;
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "Please select the source drive");
		DrawSelectableButton(80, 310, -1, 340, "Internal Disc Drive",
			(selected_source == SRC_INTERNAL_DISC) ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(350, 310, -1, 340, "USB Disc Drive (exp)",
			(selected_source == SRC_USB_DRIVE) ? B_SELECTED : B_NOSELECT, -1);
		if (selected_source == SRC_USB_DRIVE) {
			WriteFontStyled(320, 370, "USB source -> Front SD/Read Only only in this build.", 0.65f, true, defaultColor);
		}
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_A)));
		u32 btns = get_buttons_pressed();
		if (btns & PAD_BUTTON_RIGHT)
			selected_source ^= 1;
		if (btns & PAD_BUTTON_LEFT)
			selected_source ^= 1;
		if (btns & PAD_BUTTON_A)
			break;
		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_A)));
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A));
}
#endif

/* the user must specify the destination device type */
void select_device_type() {
	selected_device = 0;
#ifdef __CYGWIN__
	return;
#endif
	
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "Please select the device type");
#ifdef HW_DOL
		DrawSelectableButton(90, 310, -1, 340, "SD Card",
				(selected_device == TYPE_SD) ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(225, 310, -1, 340, "M.2 Loader",
				(selected_device == TYPE_M2LOADER) ? B_SELECTED : B_NOSELECT, -1);
		if(selected_device == TYPE_SD) {
			WriteFontStyled(320, 370, "SD Cards are supported via devices such as:", 0.65f, true, defaultColor);
			WriteFontStyled(320, 390, "SD Gecko, SD2SP2, GC2SD, FlipperMCE / GCMCE", 0.65f, true, defaultColor);
		}
#endif
#if defined(HW_RVL) || defined(__CYGWIN__)
		DrawSelectableButton(100, 310, -1, 340, "USB",
				(selected_device == TYPE_USB) ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(210, 310, -1, 340, "Front SD",
				(selected_device == TYPE_SD) ? B_SELECTED : B_NOSELECT, -1);
#endif
		DrawSelectableButton(390, 310, -1, 340, "Read Only",
				(selected_device == TYPE_READONLY) ? B_SELECTED : B_NOSELECT, -1);
		if(selected_device == TYPE_READONLY) {
			WriteFontStyled(320, 370, "Reads a disc from start to end,", 0.65f, true, defaultColor);
			WriteFontStyled(320, 390, "and verifies it against an internal checksum list.", 0.65f, true, defaultColor);
		}
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
		u32 btns = get_buttons_pressed();

		if (btns & PAD_BUTTON_RIGHT)
			selected_device = selected_device == 2 ? 0 : (selected_device+1);
		if (btns & PAD_BUTTON_LEFT)
			selected_device = selected_device == 0 ? 2 : (selected_device-1);

		if (btns & PAD_BUTTON_A)
			break;

		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A));
}

/* the user must specify the file system type */
int filesystem_type() {
	int type = TYPE_FAT;
#ifdef __CYGWIN__
	return TYPE_NTFS;
#endif
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(255, "Please select the filesystem type");
		DrawSelectableButton(100, 310, -1, 340, "FAT",
				(type == TYPE_FAT) ? B_SELECTED : B_NOSELECT, -1);
		DrawSelectableButton(380, 310, -1, 340, "NTFS",
				(type == TYPE_NTFS) ? B_SELECTED : B_NOSELECT, -1);
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
		u32 btns = get_buttons_pressed();
		if (btns & PAD_BUTTON_RIGHT)
			type ^= 1;
		if (btns & PAD_BUTTON_LEFT)
			type ^= 1;
		if (btns & PAD_BUTTON_A)
			break;
		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	return type;
}

char *getDualLayerOption() {
	int opt = options_map[WII_DUAL_LAYER];
	if (opt == AUTO_DETECT)
		return "Auto";
	else if (opt == SINGLE_MINI)
		return "1.4GB";
	else if (opt == SINGLE_LAYER)
		return "4.4GB";
	else if (opt == DUAL_LAYER)
		return "8GB";
	return 0;
}

char *getNewFileOption() {
	int opt = options_map[WII_NEWFILE];
	if (opt == ASK_USER)
		return "Yes";
	else if (opt == AUTO_CHUNK)
		return "No";
	return 0;
}

char *getChunkSizeOption() {
	int opt = options_map[WII_CHUNK_SIZE];
	if (opt == CHUNK_1GB)
		return "1GB";
	else if (opt == CHUNK_2GB)
		return "2GB";
	else if (opt == CHUNK_3GB)
		return "3GB";
	else if (opt == CHUNK_MAX)
		return "Max";
	return 0;
}

char *getAudioOutputOption() {
	int opt = options_map[AUDIO_OUTPUT];
	if (opt == AUDIO_OUT_BIN)
		return "BIN";
	else if (opt == AUDIO_OUT_WAV)
		return "WAV";
	else if (opt == AUDIO_OUT_WAV_FAST)
		return "WAV (fast)";
	else if (opt == AUDIO_OUT_WAV_BEST)
		return "WAV (best)";
	return 0;
}

int getMaxPos(int option_pos) {
	switch (option_pos) {
	case WII_DUAL_LAYER:
		return DUAL_DELIM;
	case WII_CHUNK_SIZE:
		return CHUNK_DELIM;
	case WII_NEWFILE:
		return NEWFILE_DELIM;
	case AUDIO_OUTPUT:
		return AUDIO_OUT_DELIM;
	}
	return 0;
}

void toggleOption(int option_pos, int dir) {
	int max = getMaxPos(option_pos);
	if (options_map[option_pos] + dir >= max) {
		options_map[option_pos] = 0;
	} else if (options_map[option_pos] + dir < 0) {
		options_map[option_pos] = max - 1;
	} else {
		options_map[option_pos] += dir;
	}
}

static void get_settings(int disc_type) {
	int currentSettingPos = 0;
	int optionBase = (disc_type == IS_WII_DISC || disc_type == IS_OTHER_DISC) ? MAX_NGC_OPTIONS : 0;
	int maxSettingPos = 0;
	if (disc_type == IS_WII_DISC) {
		maxSettingPos = MAX_WII_OPTIONS - 1;
	}
	else if (disc_type == IS_OTHER_DISC) {
		// For forced non-Nintendo profiles expose chunking + audio output mode.
		maxSettingPos = (forced_disc_profile == FORCED_AUDIO_CD) ? 2 : 1;
	}
	else {
		maxSettingPos = MAX_NGC_OPTIONS - 1;
	}

	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(75, 120, vmode->fbWidth - 78, 400, COLOR_BLACK);
		sprintf(txtbuffer, "%s Disc Ripper Setup:",
				disc_type == IS_WII_DISC ? "Wii" : (disc_type == IS_OTHER_DISC ? "Other" : "Gamecube"));
		WriteCentre(130, txtbuffer);

		// Wii Settings
		if (disc_type == IS_WII_DISC) {
			WriteFont(80, 160 + (32 * 1), "Dump Size");
			DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 1), -1, 160 + (32 * 1) + 30, getDualLayerOption(), (!currentSettingPos) ? B_SELECTED : B_NOSELECT, -1);
			WriteFont(80, 160 + (32 * 2), "Chunk Size");
			DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 2), -1, 160 + (32 * 2) + 30, getChunkSizeOption(), (currentSettingPos == 1) ? B_SELECTED : B_NOSELECT, -1);
			WriteFont(80, 160 + (32 * 3), "New device per chunk");
			DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 3), -1, 160 + (32 * 3) + 30, getNewFileOption(), (currentSettingPos == 2) ? B_SELECTED : B_NOSELECT, -1);
		}
		else if (disc_type == IS_OTHER_DISC) {
			WriteFont(80, 160 + (32 * 1), "Chunk Size");
			DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 1), -1, 160 + (32 * 1) + 30, getChunkSizeOption(), (!currentSettingPos) ? B_SELECTED : B_NOSELECT, -1);
			WriteFont(80, 160 + (32 * 2), "New device per chunk");
			DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 2), -1, 160 + (32 * 2) + 30, getNewFileOption(), (currentSettingPos == 1) ? B_SELECTED : B_NOSELECT, -1);
			if (forced_disc_profile == FORCED_AUDIO_CD) {
				WriteFont(80, 160 + (32 * 3), "Audio Output");
				DrawSelectableButton(vmode->fbWidth - 220, 160 + (32 * 3), -1, 160 + (32 * 3) + 30, getAudioOutputOption(), (currentSettingPos == 2) ? B_SELECTED : B_NOSELECT, -1);
			}
		}
		WriteCentre(370,"Press  A  to continue");
		DrawAButton(265,360);
		DrawFrameFinish();

		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_UP | PAD_BUTTON_DOWN)));
		u32 btns = get_buttons_pressed();
		if(btns & PAD_BUTTON_RIGHT) {
			int optionPos = optionBase + currentSettingPos;
			if (disc_type == IS_OTHER_DISC) {
				optionPos = (currentSettingPos == 0) ? WII_CHUNK_SIZE : (currentSettingPos == 1 ? WII_NEWFILE : AUDIO_OUTPUT);
			}
			toggleOption(optionPos, 1);
		}
		if(btns & PAD_BUTTON_LEFT) {
			int optionPos = optionBase + currentSettingPos;
			if (disc_type == IS_OTHER_DISC) {
				optionPos = (currentSettingPos == 0) ? WII_CHUNK_SIZE : (currentSettingPos == 1 ? WII_NEWFILE : AUDIO_OUTPUT);
			}
			toggleOption(optionPos, -1);
		}
		if(btns & PAD_BUTTON_UP) {
			currentSettingPos = (currentSettingPos>0) ? (currentSettingPos-1):maxSettingPos;
		}
		if(btns & PAD_BUTTON_DOWN) {
			currentSettingPos = (currentSettingPos<maxSettingPos) ? (currentSettingPos+1):0;
		}
		if(btns & PAD_BUTTON_A) {
			break;
		}
		while (get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A | PAD_BUTTON_UP | PAD_BUTTON_DOWN));
	}
	while(get_buttons_pressed() & PAD_BUTTON_B);
}

void prompt_new_file(FILE **fp, int chunk, int fs, int silent, int disc_type) {
	// Close the file and unmount the fs
	fclose(*fp);
#ifndef __CYGWIN__
	if(silent == ASK_USER) {
		if (fs == TYPE_FAT) {
			fatUnmount("fat:/");
			if (selected_device == TYPE_SD) {
				sdcard->shutdown();
			}
#ifdef HW_DOL
			else if (selected_device == TYPE_M2LOADER) {
				m2loader->shutdown(m2loader);
			}
#else
			else if (selected_device == TYPE_USB) {
				usb->shutdown();
			}
#endif
		}
		else if (fs == TYPE_NTFS) {
			ntfsUnmount(mounts[0].name, true);
			free(mounts);
			if (selected_device == TYPE_SD) {
				sdcard->shutdown();
			}
#ifdef HW_DOL
			else if (selected_device == TYPE_M2LOADER) {
				m2loader->shutdown(m2loader);
			}
#else
			else if (selected_device == TYPE_USB) {
				usb->shutdown();
			}
#endif
		}
		// Stop the disc if we're going to wait on the user
		dvd_motor_off(0);
	}

	if(silent == ASK_USER) {
		int ret = -1;
		do {
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				WriteCentre(255, "Insert a device for the next chunk");
				wait_press_A_exit_B(false);

			if (fs == TYPE_FAT) {
				int i = 0;
				for (i = 0; i < 10; i++) {
					switch (selected_device) {
						case TYPE_SD:
							ret = fatMountSimple("fat", sdcard);
							break;
#ifdef HW_DOL
						case TYPE_M2LOADER:
							ret = fatMountSimple("fat", m2loader);
							break;
#else
						case TYPE_USB:
							ret = fatMountSimple("fat", usb);
							break;
#endif
					}
					if (ret == 1) {
						break;
					}
				}
			}
			else if (fs == TYPE_NTFS) {
				int mountCount = 0;
				if(selected_device == TYPE_SD) {
					mountCount = ntfsMountDevice(sdcard, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				}
#ifdef HW_DOL
				if(selected_device == TYPE_M2LOADER) {
					mountCount = ntfsMountDevice(m2loader, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				}
#else
				if(selected_device ==  TYPE_USB) {
					mountCount = ntfsMountDevice(usb, &mounts, NTFS_DEFAULT | NTFS_RECOVER);
				}
#endif
				if (mountCount && mountCount != -1) {
					sprintf(&mountPath[0], "%s:/", mounts[0].name);
					ret = 1;
				} else {
					ret = -1;
				}
			}
			if (ret != 1) {
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				sprintf(txtbuffer, "Error Mounting Device [%08X]", ret);
				WriteCentre(255, txtbuffer);
				wait_press_A_exit_B(true);
			}
		} while (ret != 1);
	}
#endif

	*fp = NULL;
	sprintf(txtbuffer, "%s%s.part%i%s", &mountPath[0], &gameName[0], chunk, get_output_extension(disc_type));
	remove(&txtbuffer[0]);
	*fp = fopen(&txtbuffer[0], "wb");
	if (*fp == NULL) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(230, "Failed to create file:");
		WriteCentre(255, txtbuffer);
		WriteCentre(315, "Exiting in 5 seconds");
		DrawFrameFinish();
		sleep(5);
		exit(0);
	}
#ifndef __CYGWIN__
	if(silent == ASK_USER) {
		initialise_source();
	}
#endif
}

void dump_bca() {
	sprintf(txtbuffer, "%s%s.bca", &mountPath[0], &gameName[0]);
	remove(&txtbuffer[0]);
	FILE *fp = fopen(txtbuffer, "wb");
	if (fp) {
		char bca_data[64] __attribute__((aligned(32)));
		DCZeroRange(bca_data, 64);
		DCFlushRange(bca_data, 64);
		dvd_read_bca(bca_data);
		fwrite(bca_data, 1, 0x40, fp);
		fclose(fp);
	}
}

static void write_le32(FILE *fp, u32 value) {
	fputc((int)(value & 0xFF), fp);
	fputc((int)((value >> 8) & 0xFF), fp);
	fputc((int)((value >> 16) & 0xFF), fp);
	fputc((int)((value >> 24) & 0xFF), fp);
}

static void write_le16(FILE *fp, u16 value) {
	fputc((int)(value & 0xFF), fp);
	fputc((int)((value >> 8) & 0xFF), fp);
}

void write_wav_header(FILE *fp, u32 data_size, int channels) {
	if (!fp) {
		return;
	}
    // Calculate sample rate to ensure same playback time (duration)
    // Original: 44100Hz * 2ch * 2bytes = 176400 bytes/sec
    // New: Rate * channels * 2bytes = 176400 => Rate = 88200 / channels
    int sample_rate = 88200 / channels;

	// 44-byte PCM RIFF header, 16-bit stereo 44.1kHz
	fwrite("RIFF", 1, 4, fp);
	write_le32(fp, data_size + 36);
	fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp);
	write_le32(fp, 16);          // PCM fmt chunk size
	write_le16(fp, 1);           // AudioFormat = PCM
	write_le16(fp, channels);    // NumChannels
	write_le32(fp, sample_rate);       // SampleRate
	write_le32(fp, sample_rate * channels * 2);      // ByteRate
	write_le16(fp, channels * 2);  // BlockAlign
	write_le16(fp, 16);          // BitsPerSample = 16
	fwrite("data", 1, 4, fp);
	write_le32(fp, data_size);
}

void dump_audio_cue(const char *audioFileName, int isWave) {
	if (selected_device == TYPE_READONLY || !audioFileName) {
		return;
	}

	sprintf(txtbuffer, "%s%s.cue", mountPath, gameName);
	printf("\n*** Attempting to write CUE to %s ***\n", txtbuffer);
    fflush(stdout);
	remove(txtbuffer);
	FILE *fp = fopen(txtbuffer, "wb");
	if (!fp) {
		printf("Error opening CUE file: %s\n", strerror(errno));
        printf("MountPath: %s, GameName: %s\n", mountPath, gameName);
		return;
	}

	fprintf(fp, "FILE \"%s\" %s\r\n", audioFileName, isWave ? "WAVE" : "BINARY");

    int toc_read = 0;
    if (hSourceDrive != INVALID_HANDLE_VALUE) {
        CDROM_TOC_LOCAL toc;
        DWORD bytesReturned;
        if (DeviceIoControl(hSourceDrive, IOCTL_CDROM_READ_TOC_LOCAL, NULL, 0, &toc, sizeof(toc), &bytesReturned, NULL)) {
            printf("TOC read successfully. Tracks: %d-%d\n", toc.FirstTrack, toc.LastTrack);
            toc_read = 1;
            for (int i = toc.FirstTrack; i <= toc.LastTrack; i++) {
                int index = i - toc.FirstTrack;
                if (index >= MAXIMUM_NUMBER_TRACKS_LOCAL) break;
                
                TRACK_DATA_LOCAL *tr = &toc.TrackData[index];
                
                // Address is MSF. Byte 1=M, 2=S, 3=F.
                u32 m = tr->Address[1];
                u32 s = tr->Address[2];
                u32 f = tr->Address[3];
                
                // Convert to frames
                u32 frames = (m * 60 + s) * 75 + f;
                
                // Subtract 2 seconds (150 frames) for relative time from start of file (assuming file starts at 00:02:00 abs)
                if (frames >= 150) frames -= 150;
                else frames = 0;
                
                m = frames / (75 * 60);
                s = (frames / 75) % 60;
                f = frames % 75;
                
                fprintf(fp, "  TRACK %02d AUDIO\r\n", tr->TrackNumber);
                fprintf(fp, "    INDEX 01 %02d:%02d:%02d\r\n", m, s, f);
                printf("Track %02d Start: %02d:%02d:%02d\n", tr->TrackNumber, m, s, f);
            }
        } else {
            printf("DeviceIoControl TOC failed: %u\n", (unsigned int)GetLastError());
        }
    }

    if (!toc_read) {
        printf("Using default 1-track CUE.\n");
        fprintf(fp, "  TRACK 01 AUDIO\r\n");
        fprintf(fp, "    INDEX 01 00:00:00\r\n");
    }

    fflush(fp);
	fclose(fp);
    printf("CUE file closed.\n");
    printf("*** CUE file created successfully ***\n");
}


void dump_info(char *md5, char *sha1, u32 crc32, int verified, u32 seconds, char* name) {
	if(selected_device == TYPE_READONLY) {
		return;
	}
	
	char infoLine[1024];
	char timeLine[256];
	memset(infoLine, 0, 1024);
	memset(timeLine, 0, 256);
	time_t curtime;
	time(&curtime);
	strftime(timeLine, sizeof(timeLine), "%Y-%m-%d %H:%M:%S", localtime(&curtime));

	if(md5 && sha1 && crc32) {
		sprintf(infoLine, "--File Generated by CleanRip v%i.%i.%i--"
						  "\r\n\r\nFilename: %s\r\nInternal Name: %s\r\nMD5: %s\r\n"
						  "SHA-1: %s\r\nCRC32: %08X\r\nVersion: 1.0%i\r\nVerified: %s\r\nDuration: %u min. %u sec\r\nDumped at: %s.\r\n",
				V_MAJOR,V_MID,V_MINOR,&gameName[0],&internalName[0], md5, sha1, crc32, *(u8*)0x80000007,
				verified ? "Yes" : "No", seconds/60, seconds%60, timeLine);
	}
	else {
		sprintf(infoLine, "--File Generated by CleanRip v%i.%i.%i--"
						  "\r\n\r\nFilename: %s\r\nInternal Name: %s\r\n"
						  "CRC32: %08X\r\nVersion: 1.0%i\r\nVerified: %s\r\nDuration: %u min. %u sec\r\nDumped at: %s.\r\n"
						  "\r\n-- DO NOT USE THIS FOR REDUMP SUBMISSIONS, ENABLE CHECKSUM CALCULATIONS FOR THAT!",
				V_MAJOR,V_MID,V_MINOR,&gameName[0],&internalName[0], crc32, *(u8*)0x80000007,
				verified ? "Yes" : "No", seconds/60, seconds%60, timeLine);
	}

	if (name != NULL) {
		sprintf(txtbuffer, "%s%s-dumpinfo.txt", &mountPath[0], &name[0]);
	}
	else {
		sprintf(txtbuffer, "%s%s-dumpinfo.txt", &mountPath[0], &gameName[0]);
	}
	
	remove(&txtbuffer[0]);
	FILE *fp = fopen(txtbuffer, "wb");
	if (fp) {
		fwrite(infoLine, 1, strlen(&infoLine[0]), fp);
		fclose(fp);
	}
}

void renameFile(const char* mountPath, const char* befor, const char* after, const char* base) {
	char tempstr[2048];

	if (mountPath == NULL || befor == NULL || after == NULL || base == NULL) return;

	sprintf(txtbuffer, "%s%s%s", &mountPath[0], &befor[0], &base[0]);
	sprintf(tempstr, "%s%s%s", &mountPath[0], &after[0], &base[0]);
	remove(&tempstr[0]);
	if (rename(txtbuffer, tempstr) == 0) {
		print_gecko("Renamed: %s\r\n\t->%s\r\n", txtbuffer, tempstr);
	}
	else {
		print_gecko("Rename failed: %s\r\n", txtbuffer);
	}
}

char *getDiscTypeStr(int disc_type, bool isDualLayer) {
	if(disc_type == IS_NGC_DISC) {
		return "GameCube";
	}
	if(disc_type == IS_DATEL_DISC) {
		return "Datel";
	}
	if(disc_type == IS_WII_DISC) {
		return isDualLayer ? "Wii (dual layer)" : "Wii";
	}
	if (disc_type == IS_OTHER_DISC) {
		if (forced_disc_profile == FORCED_DVD_VIDEO_DL) {
			return "DVD-Video (dual layer)";
		}
		if (forced_disc_profile == FORCED_MINI_DVD) {
			return "MiniDVD";
		}
		if (forced_disc_profile == FORCED_AUDIO_CD) {
			return "Audio CD";
		}
		return "DVD-Video";
	}
	return "Unknown";
}

#define MSG_COUNT 8
#define THREAD_PRIO 128

static int select_wav_channels() {
    int channels = 2;
    while ((get_buttons_pressed() & PAD_BUTTON_A));
    while (1) {
        DrawFrameStart();
        DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
        WriteCentre(255, "Select Audio Channels");
        
        char buf[32];
        sprintf(buf, "< %d >", channels);
        DrawSelectableButton(280, 310, -1, 340, buf, B_SELECTED, -1);
        WriteCentre(360, "Left/Right to change, A to confirm");
        
        DrawFrameFinish();
        
        while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A)));
        u32 btns = get_buttons_pressed();
        
        if (btns & PAD_BUTTON_RIGHT) channels++;
        if (btns & PAD_BUTTON_LEFT) {
            channels--;
            if (channels < 1) channels = 1;
        }
        if (btns & PAD_BUTTON_A) break;
        while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT | PAD_BUTTON_A)));
    }
    while ((get_buttons_pressed() & PAD_BUTTON_A));
    return channels;
}

int dump_game(int disc_type, int fs) {

	isDumping = 1;
	md5_state_t state;
	md5_byte_t digest[16];
	SHA1Context sha;
	u32 crc32 = 0;
	u32 crc100000 = 0;
	char *buffer;
	mqbox_t msgq, blockq;
	lwp_t writer;
	writer_msg *wmsg;
	writer_msg msg;
	int i;

	MQ_Init(&blockq, MSG_COUNT);
	MQ_Init(&msgq, MSG_COUNT);

	// since libogc is too shitty to be able to get the current thread priority, just force it to a known value
	LWP_SetThreadPriority(pthread_self(), THREAD_PRIO);
	// writer thread should have same priority so it can be yielded to
	LWP_CreateThread(&writer, writer_thread, (void*)msgq, NULL, 0, THREAD_PRIO);

	// Check if we will ask the user to insert a new device per chunk
	int silent = options_map[WII_NEWFILE];
	int audio_mode = options_map[AUDIO_OUTPUT];

	int is_audio_profile = (disc_type == IS_OTHER_DISC && forced_disc_profile == FORCED_AUDIO_CD);
    printf("Debug: dump_game start. is_audio_profile=%d, disc_type=%d, forced_disc_profile=%d\n", is_audio_profile, disc_type, forced_disc_profile);
    fflush(stdout);
	if (is_audio_profile && forced_audio_sector_size == 0) {
		forced_audio_sector_size = 2352;
	}
	u32 sector_size = (disc_type == IS_OTHER_DISC) ? get_forced_disc_sector_size() : 2048;
	u32 target_read_size = READ_SIZE;
	if (is_audio_profile && sector_size == 2352) {
		// Keep blocks aligned to CDDA frames.
		target_read_size = (READ_SIZE / 2352) * 2352;
	}
	u32 read_sectors = target_read_size / sector_size;
	if (read_sectors == 0) {
		read_sectors = 1;
	}
	u32 max_read_size = read_sectors * sector_size;
	u64 one_gigabyte_bytes = (u64)ONE_GIGABYTE * 2048;

	u32 startLBA = 0;
	u32 endLBA = (disc_type == IS_NGC_DISC || disc_type == IS_DATEL_DISC) ? NGC_DISC_SIZE
		: (disc_type == IS_WII_DISC) ? (options_map[WII_DUAL_LAYER] == AUTO_DETECT ? detect_duallayer_disc()
			: (options_map[WII_DUAL_LAYER] == SINGLE_MINI ? WII_D1_SIZE
				: (options_map[WII_DUAL_LAYER] == DUAL_LAYER ? WII_D9_SIZE
					: WII_D5_SIZE)))
		: get_forced_disc_end_sectors();
	if (disc_type == IS_OTHER_DISC && forced_disc_profile == FORCED_AUDIO_CD) {
		endLBA = detect_audio_cd_size_sectors(sector_size);
	}
	u64 total_bytes = (u64)endLBA * sector_size;

	// Work out the chunk size
	u32 chunk_size_wii = options_map[WII_CHUNK_SIZE];
	u64 opt_chunk_size;
	if (chunk_size_wii == CHUNK_MAX) {
		// use 4GB chunks max for FAT drives
		if (selected_device != TYPE_READONLY && fs == TYPE_FAT) {
			long file_size_bits = pathconf("fat:/", _PC_FILESIZEBITS);
			if (file_size_bits <= 33) {
			opt_chunk_size = (4ULL * one_gigabyte_bytes) - max_read_size - 1;
		} else {
			opt_chunk_size = total_bytes + max_read_size;
		}
	} else {
			opt_chunk_size = total_bytes + max_read_size;
		}
	} else {
		opt_chunk_size = (u64)(chunk_size_wii + 1) * one_gigabyte_bytes;
	}

	if (disc_type == IS_NGC_DISC || disc_type == IS_DATEL_DISC
		|| (disc_type == IS_WII_DISC && options_map[WII_DUAL_LAYER] == SINGLE_MINI)) {
		opt_chunk_size = (u64)NGC_DISC_SIZE * 2048;
	}
	if (is_audio_profile) {
		// Keep audio dumps as a single BIN so a single CUE can reference it.
		opt_chunk_size = total_bytes + max_read_size;
	}

	// Dump the BCA for Nintendo discs
	if(selected_device != TYPE_READONLY && disc_type != IS_OTHER_DISC
#ifdef HW_RVL
		&& selected_source == SRC_INTERNAL_DISC
#endif
	) {
		dump_bca();
	}

	// Create the read buffers
	buffer = memalign(32, MSG_COUNT*(max_read_size+sizeof(writer_msg)));
	for (i=0; i < MSG_COUNT; i++) {
		MQ_Send(blockq, (mqmsg_t)(buffer+i*(max_read_size+sizeof(writer_msg))), MQ_MSG_BLOCK);
	}

	// Reset MD5/SHA-1/CRC
	md5_init(&state);
	SHA1Reset(&sha);
	crc32 = 0;

	// There will be chunks, name accordingly
	FILE *fp = NULL;
	const char *output_ext = get_output_extension(disc_type);
	int should_eject = (disc_type == IS_NGC_DISC || disc_type == IS_WII_DISC || disc_type == IS_DATEL_DISC);
	FILE *badfp = NULL;
	const int audio_max_attempts = (audio_mode == AUDIO_OUT_WAV_FAST) ? 3 : (audio_mode == AUDIO_OUT_WAV_BEST ? 10 : 6);
	const int audio_sector_recovery = (audio_mode == AUDIO_OUT_WAV || audio_mode == AUDIO_OUT_WAV_BEST);
    
    int wav_channels = 2;
    if (is_audio_profile && strcmp(output_ext, ".wav") == 0) {
        wav_channels = select_wav_channels();
    }

	if(selected_device != TYPE_READONLY) {
		if (opt_chunk_size < total_bytes) {
			sprintf(txtbuffer, "%s%s.part0%s", &mountPath[0], &gameName[0], output_ext);
		} else {
			sprintf(txtbuffer, "%s%s%s", &mountPath[0], &gameName[0], output_ext);
		}
		remove(&txtbuffer[0]);
		fp = fopen(&txtbuffer[0], "wb");
		if (fp == NULL) {
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			WriteCentre(230, "Failed to create file:");
			WriteCentre(255, txtbuffer);
			WriteCentre(315, "Exiting in 5 seconds");
			DrawFrameFinish();
			sleep(5);
			exit(0);
		}
		if (is_audio_profile && strcmp(output_ext, ".wav") == 0) {
			write_wav_header(fp, 0, wav_channels);
		}
		if (is_audio_profile) {
			printf("Debug: Calling dump_audio_cue at start\n"); fflush(stdout);
			char cueFileName[64];
			int cueIsWave = (strcmp(output_ext, ".wav") == 0);
			sprintf(cueFileName, "%s%s", &gameName[0], output_ext);
			dump_audio_cue(cueFileName, cueIsWave);
		}
		msg.command = MSG_SETFILE;
		msg.data = fp;
		MQ_Send(msgq, (mqmsg_t)&msg, MQ_MSG_BLOCK);

		if (is_audio_profile) {
			sprintf(txtbuffer, "%s%s.bad", &mountPath[0], &gameName[0]);
			remove(&txtbuffer[0]);
			badfp = fopen(&txtbuffer[0], "wb");
			if (badfp) {
				fprintf(badfp, "# zero-filled ranges (start_lba,sectors)\n");
			}
		}
	}

	int ret = 0;
	u32 audio_read_errors = 0;
	u32 audio_blocks_total = 0;
	u32 audio_sectors_total = 0;
	u32 audio_sectors_failed = 0;
	u32 lastLBA = 0;
	u64 lastCheckedTime = gettime();
	u64 startTime = gettime();
	int chunk = 1;
	int isKnownDatel = 0;
	char *discTypeStr = getDiscTypeStr(disc_type, endLBA == WII_D9_SIZE);

	while (!ret && (startLBA < endLBA)) {
		MQ_Receive(blockq, (mqmsg_t*)&wmsg, MQ_MSG_BLOCK);
		if(selected_device != TYPE_READONLY) {
			if (wmsg==NULL) { // asynchronous write error
				LWP_JoinThread(writer, NULL);
				fclose(fp);
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				WriteCentre(255, "Write Error!");
				WriteCentre(315, "Exiting in 10 seconds");
				DrawFrameFinish();
				sleep(10);
				exit(1);
			}

			if (((u64)startLBA * sector_size) > (opt_chunk_size * chunk)) {
				// wait for writing to finish
				vu32 sema = 0;
				msg.command = MSG_FLUSH;
				msg.data = (void*)&sema;
				MQ_Send(msgq, (mqmsg_t)&msg, MQ_MSG_BLOCK);
				while (!sema)
					LWP_YieldThread();

				// open new file
				u64 wait_begin = gettime();
				if (badfp && silent == ASK_USER) {
					fclose(badfp);
					badfp = NULL;
				}
				prompt_new_file(&fp, chunk, fs, silent, disc_type);
				if (is_audio_profile && selected_device != TYPE_READONLY && silent == ASK_USER) {
					sprintf(txtbuffer, "%s%s.bad", &mountPath[0], &gameName[0]);
					badfp = fopen(&txtbuffer[0], "ab");
				}
				// pretend the wait didn't happen
				startTime -= (gettime() - wait_begin);

				// set writing file
				msg.command = MSG_SETFILE;
				msg.data = fp;
				MQ_Send(msgq, (mqmsg_t)&msg, MQ_MSG_BLOCK);
				chunk++;
			}	
		}

		u32 cur_read_sectors = ((startLBA + read_sectors) <= endLBA) ? read_sectors : (endLBA - startLBA);
		u32 opt_read_size = cur_read_sectors * sector_size;
		if (is_audio_profile) {
			audio_blocks_total++;
			audio_sectors_total += cur_read_sectors;
		}

		wmsg->command =  MSG_WRITE;
		wmsg->data = wmsg+1;
		wmsg->length = opt_read_size;
		wmsg->ret_box = blockq;

		// Read from Disc
		if (is_audio_profile) {
			// Audio CD mode: retry several times before zero-filling.
			ret = 1;
			for (int attempt = 0; attempt < audio_max_attempts; attempt++) {
				ret = source_read(wmsg->data, (u32)opt_read_size, (u64)startLBA * sector_size, disc_type, isKnownDatel);
				if (ret == 0) {
					break;
				}
				usleep(1000 + (attempt * 500));
			}
		}
		else
			ret = source_read(wmsg->data, (u32)opt_read_size, (u64)startLBA * sector_size, disc_type, isKnownDatel);
		if (ret != 0) {
			if (is_audio_profile) {
				if (audio_sector_recovery && cur_read_sectors > 1) {
					u32 bad_run_start = 0;
					u32 bad_run_len = 0;
					for (u32 s = 0; s < cur_read_sectors; s++) {
						int sec_ret = 1;
						for (int a = 0; a < audio_max_attempts; a++) {
							sec_ret = source_read(((u8*)wmsg->data) + (s * sector_size), sector_size, ((u64)startLBA + s) * sector_size, disc_type, isKnownDatel);
							if (sec_ret == 0) {
								break;
							}
							usleep(1000 + (a * 500));
						}
						if (sec_ret != 0) {
							audio_read_errors++;
							audio_sectors_failed++;
							memset(((u8*)wmsg->data) + (s * sector_size), 0, sector_size);
							if (bad_run_len == 0) {
								bad_run_start = startLBA + s;
							}
							bad_run_len++;
						}
						else if (bad_run_len > 0) {
							if (badfp) {
								fprintf(badfp, "%u,%u\n", bad_run_start, bad_run_len);
							}
							bad_run_len = 0;
						}
					}
					if (bad_run_len > 0 && badfp) {
						fprintf(badfp, "%u,%u\n", bad_run_start, bad_run_len);
					}
				}
				else {
					audio_read_errors += cur_read_sectors;
					audio_sectors_failed += cur_read_sectors;
					memset(wmsg->data, 0, opt_read_size);
					if (badfp) {
						fprintf(badfp, "%u,%u\n", startLBA, cur_read_sectors);
					}
				}
				// Keep dumping despite sporadic read errors; report every 64 failed sectors.
				if ((audio_read_errors & 63) == 1) {
					print_gecko("Audio CD read errors=%u sectors (last LBA %u, err=%08X)\r\n",
						audio_read_errors, startLBA, dvd_get_error());
				}
				ret = 0;
			}
			else {
				MQ_Send(blockq, (mqmsg_t)wmsg, MQ_MSG_BLOCK);
				break;
			}
		}
		usleep(50);
		MQ_Send(msgq, (mqmsg_t)wmsg, MQ_MSG_BLOCK);
		if(calcChecksums) {
			// Calculate MD5
			md5_append(&state, (const md5_byte_t *) (wmsg+1), (u32) opt_read_size);
			// Calculate SHA-1
			SHA1Input(&sha, (const unsigned char *) (wmsg+1), (u32) opt_read_size);
		}
		// Always calculate CRC32
		crc32 = Crc32_ComputeBuf( crc32, wmsg+1, (u32) opt_read_size);

		if(disc_type == IS_DATEL_DISC && (((u64)startLBA * sector_size) + opt_read_size == 0x100000)){
			crc100000 = crc32;
			isKnownDatel = datel_findCrcSum(crc100000);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			if(!isKnownDatel) {
				WriteCentre(215, "(Warning: This disc will take a while to dump!)");
			}
			sprintf(txtbuffer, "%s CRC100000=%08X", (isKnownDatel ? "Known":"Unknown"), crc100000);
			WriteCentre(255, txtbuffer);
			u64 waitTimeStart = gettime();
			wait_press_A_exit_B(false);
			startTime += (gettime() - waitTimeStart);	// Don't throw time off because we'd paused here
		}

		check_exit_status();
	
		u32 pressedButtons = get_buttons_pressed();

		if (pressedButtons & PAD_BUTTON_B) {
			ret = -61;
		}
		if (pressedButtons & PAD_BUTTON_Y) {
			newProgressDisplay ^= 1;
		}
		// Update status every second
		u64 curTime = gettime();
		s32 timePassed = diff_msec(lastCheckedTime, curTime);
		if (timePassed >= 1000) {
			u64 current_bytes = (u64)startLBA * sector_size;
			u64 last_bytes = (u64)lastLBA * sector_size;
			u32 bytes_since_last_read = (u32)((current_bytes - last_bytes) * (1000.0f/timePassed));
			u64 remainder = (((u64)endLBA - startLBA) * sector_size) - opt_read_size;
			u32 etaTime = bytes_since_last_read ? (remainder / bytes_since_last_read) : 0;
			DrawFrameStart();
			if(newProgressDisplay) {
				sprintf(txtbuffer, "Rate: %4.2fKB/s\nETA: %02d:%02d:%02d",
					(float)bytes_since_last_read/1024.0f,
					(int)((etaTime/3600)%60),(int)((etaTime/60)%60),(int)(etaTime%60));
					
					DrawProgressDetailed((int)((float)((float)startLBA/(float)endLBA)*100), txtbuffer, 
						(int) ((((u64)startLBA * sector_size) / (1024*1024))),
						(int) ((((u64)endLBA * sector_size) / (1024*1024))), discTypeStr, calcChecksums, disc_type);
			}
			else {
				sprintf(txtbuffer, "%dMB %4.2fKB/s - ETA %02d:%02d:%02d",
					(int) ((((u64)startLBA * sector_size) / (1024*1024))),
					(float)bytes_since_last_read/1024.0f,
					(int)((etaTime/3600)%60),(int)((etaTime/60)%60),(int)(etaTime%60));
				DrawProgressBar((int)((float)((float)startLBA/(float)endLBA)*100), txtbuffer, disc_type);
			}
      		DrawFrameFinish();
			lastCheckedTime = curTime;
			lastLBA = startLBA;
		}
		startLBA += cur_read_sectors;
	}
	if(calcChecksums) {
		md5_finish(&state, digest);
	}
	if (is_audio_profile && audio_sectors_total && audio_sectors_failed == audio_sectors_total) {
		ret = -62; // all audio blocks failed
	}

	// signal writer to finish
	MQ_Send(msgq, (mqmsg_t)NULL, MQ_MSG_BLOCK);
	LWP_JoinThread(writer, NULL);
	if(selected_device != TYPE_READONLY) {
		if (fp && is_audio_profile && strcmp(output_ext, ".wav") == 0) {
			u32 wav_data_size = (u32)((u64)startLBA * sector_size);
			fseek(fp, 0, SEEK_SET);
			write_wav_header(fp, wav_data_size, wav_channels);
		}
		fclose(fp);
		if (badfp) {
			fclose(badfp);
		}
	}

	free(buffer);
	MQ_Close(blockq);
	MQ_Close(msgq);

	if(ret != -61 && ret) {
		DrawFrameStart();
		DrawEmptyBox (30,180, vmode->fbWidth-38, 350, COLOR_BLACK);
		if (ret == -62) {
			sprintf(txtbuffer, "Audio read failed (all blocks)");
		}
		else {
			sprintf(txtbuffer, "%s",dvd_error_str());
		}
		print_gecko("Error: %s\r\n",txtbuffer);
		WriteCentre(255,txtbuffer);
		dvd_motor_off(should_eject ? 1 : 0);
		wait_press_A("to continue");
		return 0;
	}
	else if (ret == -61) {
		DrawFrameStart();
		DrawEmptyBox (30,180, vmode->fbWidth-38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Copy Cancelled");
		print_gecko("%s\r\n",txtbuffer);
		WriteCentre(255,txtbuffer);
		dvd_motor_off(0);
		wait_press_A("to continue");
		return 0;
	}
	else {
		DrawFrameStart();
		DrawProgressDetailed((int)((float)((float)startLBA/(float)endLBA)*100), "Finished", 
						(int) ((((u64)startLBA * sector_size) / (1024*1024))),
						(int) ((((u64)endLBA * sector_size) / (1024*1024))), discTypeStr, calcChecksums, disc_type);
		DrawEmptyBox (30,180, vmode->fbWidth-38, 350, COLOR_BLACK);
		sprintf(txtbuffer,"Copy completed in %u mins. Press A",diff_sec(startTime, gettime())/60);
		WriteCentre(190,txtbuffer);

		int verified = 0;
		char tempstr[64];

		if ((disc_type == IS_DATEL_DISC)) {
				dump_skips(&mountPath[0], crc100000);
		}
		char md5sum[64];
		char sha1sum[64];
		memset(&md5sum[0], 0, 64);
		memset(&sha1sum[0], 0, 64);
		if (calcChecksums) {
			int i; for (i=0; i<16; i++) sprintf(&md5sum[i*2],"%02x",digest[i]);
			if(SHA1Result(&sha)) {
				for (i=0; i<5; i++) sprintf(&sha1sum[i*8],"%08x",sha.Message_Digest[i]);
			}
			else {
				sprintf(sha1sum, "Error computing SHA-1");
			}
		}
		char* name = NULL;
		int canVerifyWithDat = (disc_type == IS_NGC_DISC || disc_type == IS_WII_DISC || disc_type == IS_DATEL_DISC);
		int availableVerificationType = canVerifyWithDat ? verify_is_available(disc_type) : -1;
		if (canVerifyWithDat) {
			if(availableVerificationType != VERIFY_INTERNAL_CRC && calcChecksums) {
				verified = verify_findMD5Sum(&md5sum[0], disc_type);
			}
			else {
				verified = verify_findCrc32(crc32, disc_type);
			}
		}
		if (verified && availableVerificationType != VERIFY_INTERNAL_CRC) {
			if (opt_chunk_size < total_bytes) {
				for (int i = 0; i < chunk; i++) {
					sprintf(tempstr, ".part%i%s", i, output_ext);
					renameFile(&mountPath[0], &gameName[0], verify_get_name(0), &tempstr[0]);
				}
			}
			else {
				renameFile(&mountPath[0], &gameName[0], verify_get_name(0), output_ext);
			}
#ifdef HW_RVL
			renameFile(&mountPath[0], &gameName[0], verify_get_name(0), ".bca");
#endif

			name = verify_get_name(0);
		}
		if ((disc_type == IS_DATEL_DISC)) {
			verified = datel_findMD5Sum(&md5sum[0]);
			if (verified) {
				renameFile(&mountPath[0], &gameName[0], datel_get_name(0), ".iso");
				renameFile(&mountPath[0], &gameName[0], datel_get_name(0), ".skp");
#ifdef HW_RVL
				renameFile(&mountPath[0], &gameName[0], datel_get_name(0), ".bca");
#endif
				name = datel_get_name(0);
			}
		}
		if(calcChecksums) {
			dump_info(&md5sum[0], &sha1sum[0], crc32, verified, diff_sec(startTime, gettime()), name);
			if (canVerifyWithDat) {
				print_gecko("MD5: %s\r\n", verified ? "Verified OK" : "Not Verified ");
			}
			else {
				print_gecko("Verification: Not available for this disc profile\r\n");
			}

		}
		if (canVerifyWithDat) {
			sprintf(txtbuffer, "%s: %s", (availableVerificationType != VERIFY_INTERNAL_CRC) ? "MD5" : "CRC32", verified ? "Verified OK" : "");
		}
		else {
			sprintf(txtbuffer, "CRC32: %08X", crc32);
		}
		WriteCentre(230, txtbuffer);
		if (!canVerifyWithDat) {
			WriteCentre(255, "Redump verification not available for this disc type");
		}
		else if ((disc_type == IS_DATEL_DISC)) {
			WriteCentre(255, verified ? datel_get_name(1) : "Not Verified with datel.dat");
		}
		else {
			if(verified) {
				WriteCentre(255, (availableVerificationType != VERIFY_INTERNAL_CRC) ? verify_get_name(1) : "Verified disc dump");
			}
			else {
				WriteCentre(255, "Not verified with redump DAT");
			}
		}
		if (is_audio_profile && audio_read_errors) {
			sprintf(txtbuffer, "Audio CD had %u read errors (zero-filled)", audio_read_errors);
			WriteCentre(305, txtbuffer);
			sprintf(txtbuffer, "%s%s.bad", &mountPath[0], &gameName[0]);
			WriteCentre(330, txtbuffer);
		}
		WriteCentre(280, &md5sum[0]);
		if(!calcChecksums) {
			dump_info(NULL, NULL, crc32, verified, diff_sec(startTime, gettime()), NULL);
		}
        printf("Debug: Checking audio profile. is_audio_profile=%d, disc_type=%d, forced_disc_profile=%d\n", is_audio_profile, disc_type, forced_disc_profile);
        fflush(stdout);
		if (is_audio_profile && selected_device != TYPE_READONLY) {
			// CUE file is now generated at the start of the rip
		}
		if ((disc_type == IS_DATEL_DISC) && !(verified)) {
			dump_skips(&mountPath[0], crc100000);
			
			char tempstr[64];
			sprintf(tempstr, "datel_%08x", crc100000);
			renameFile(&mountPath[0], &gameName[0], &tempstr[0], output_ext);
			renameFile(&mountPath[0], &gameName[0], &tempstr[0], "-dumpinfo.txt");
			renameFile(&mountPath[0], &gameName[0], &tempstr[0], ".skp");
#ifdef HW_RVL
			renameFile(&mountPath[0], &gameName[0], &tempstr[0], ".bca");
#endif
		}
		dvd_motor_off(should_eject ? 1 : 0);
		wait_press_A_exit_B(false);
	}
	return 1;
}

int main(int argc, char **argv) {
#ifdef HW_RVL
	// disable ahbprot and reload IOS to clear up memory
	IOS_ReloadIOS(IOS_GetVersion());
	disable_ahbprot();
#endif
	Initialise();
#ifdef HW_RVL
	iosversion = IOS_GetVersion();
#endif
#if defined(HW_RVL) || defined(HW_DOL)
	if(usb_isgeckoalive(1)) {
		usb_flush(1);
		print_usb = 1;
	}
#endif
	print_gecko("CleanRip Version %i.%i.%i\r\n",V_MAJOR, V_MID, V_MINOR);
#if defined(HW_RVL) || defined(HW_DOL)
	print_gecko("Arena Size: %iKb\r\n",(SYS_GetArena1Hi()-SYS_GetArena1Lo())/1024);
#endif

#ifdef HW_RVL
	print_gecko("Running on IOS ver: %i\r\n", iosversion);
#endif
	show_disclaimer();
#ifdef HW_RVL
	hardware_checks();
#endif

	// Ask the user if they want checksum calculations enabled this time?
	calcChecksums = DrawYesNoDialog("Enable checksum calculations?",
									"(Enabling will add about 3 minutes)");

	int reuseSettings = NOT_ASKED;
	while (1) {
		int fs = 0, ret = 0;
		if(reuseSettings == NOT_ASKED || reuseSettings == ANSWER_NO) {
			int validSelection = 0;
			while (!validSelection) {
#ifdef HW_RVL
				select_source_type();
#endif
				select_device_type();
#ifdef HW_RVL
				if (selected_source == SRC_USB_DRIVE && selected_device == TYPE_USB) {
					DrawFrameStart();
					DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
					WriteCentre(230, "USB source -> USB destination");
					WriteCentre(255, "is not supported in this build.");
					WriteCentre(280, "Choose Front SD or Read Only.");
					wait_press_A_exit_B(false);
				}
				else
#endif
				{
					validSelection = 1;
				}
			}
			if (selected_device != TYPE_READONLY) {
				fs = filesystem_type();
				ret = -1;
				do {
					ret = initialise_device(fs);
				} while (ret != 1);
			}			
		}

		if(selected_device != TYPE_READONLY && calcChecksums) {
			// Try to load up redump.org dat files
			verify_init(&mountPath[0]);
#ifdef HW_RVL
			// Ask the user if they want to download new ones
			verify_download(&mountPath[0]);

			// User might've got some new files.
			verify_init(&mountPath[0]);
#endif
		}

		// Init the source and try to detect disc type
		ret = NO_DISC;
		while (ret == NO_DISC) {
			ret = initialise_source();
			if (ret == NO_DISC) {
				if (DrawYesNoDialog("Disc init reports no disc",
									"Continue anyway and force type?")) {
					ret = 0;
					break;
				}
			}
		}

		forced_disc_profile = FORCED_DISC_NONE;
		forced_audio_sector_size = 0;
		int disc_type = identify_disc();

		if (disc_type == IS_UNK_DISC) {
			disc_type = force_disc();
		}

		if(reuseSettings == NOT_ASKED || reuseSettings == ANSWER_NO) {
			if ((disc_type == IS_WII_DISC || disc_type == IS_OTHER_DISC) && selected_device != TYPE_READONLY) {
				get_settings(disc_type);
			}
		
			// Ask the user if they want to force Datel check this time?
			if(disc_type != IS_OTHER_DISC && selected_device != TYPE_READONLY
#ifdef HW_RVL
				&& selected_source == SRC_INTERNAL_DISC
#endif
				&& DrawYesNoDialog("Is this a unlicensed datel disc?",
								 "(Will attempt auto-detect if no)")) {
				disc_type = IS_DATEL_DISC;
				datel_init(&mountPath[0]);
#ifdef HW_RVL
				datel_download(&mountPath[0]);
				datel_init(&mountPath[0]);
#endif
				calcChecksums = 1;
			}
		}
		
		if(reuseSettings == NOT_ASKED) {
			if(DrawYesNoDialog("Remember settings?",
								 "Will only ask again next session")) {
				reuseSettings = ANSWER_YES;
			}
		}

		verify_type_in_use = (disc_type == IS_OTHER_DISC) ? -1 : verify_is_available(disc_type);
		ret = dump_game(disc_type, fs);
		isDumping = 0;
		verify_type_in_use = 0;
		dumpCounter += (ret ? 1 : 0);
		
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "%i disc(s) dumped", dumpCounter);
		WriteCentre(190, txtbuffer);
		WriteCentre(255, "Dump another disc?");
		wait_press_A_exit_B(false);
	}
    return 0;
}
