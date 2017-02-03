LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

src_files := \
	plr_main.c \
    plr_common.c \
    plr_err_handling.c \
    plr_deviceio.c \
    plr_rtc64.c \
    plr_rand_sector.c \
    plr_write_rand.c \
    plr_precondition.c \
    plr_hooking_5410_system.c \
    plr_tick_conversion.c \
    plr_case_info.c \
    plr_protocol.c \
    plr_calibration.c \
    plr_tftp.c \
    plr_pattern.c \
    plr_verify.c \
    plr_verify_report.c \
    plr_verify_log.c \
    plr_io_statistics.c \
    plr_system.c \
	test_case/plr_daxx0000.c \
	test_case/plr_daxx0001.c \
	test_case/plr_daxx0002.c \
	test_case/plr_daxx0003_4.c \
	test_case/plr_daxx0008.c \
	test_case/plr_daxx0009.c \
	test_case/plr_daxx0010.c \
	test_case/plr_daxx0011.c \
	test_case/plr_daxx0012.c \
	test_case/plr_dpin0000.c \
	test_case/plr_dpin0001.c \
	test_case/plr_dpin0002.c \
	test_case/plr_dpin0003.c \
	test_case/plr_dpin0004.c \
	test_case/plr_dpin0005.c \
	test_case/plr_dpin0006.c \
	test_case/plr_dpin0007.c \
	test_case/plr_dpin0008.c \
	test_case/plr_dpin0013.c \
	test_case/plr_dpin0014.c \
	test_case/plr_dpin0015.c \
	test_case/plr_dpin0009_12.c

include $(CLEAR_VARS)

LOCAL_MODULE := plrtest
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_SRC_FILES := $(src_files) \
	plrtest.c

LOCAL_CFLAGS := -Wall
#LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libc libcutils libm

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := plrprepare
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_SRC_FILES := $(src_files) \
	plrprepare.c

LOCAL_CFLAGS := -Wall
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libc libcutils libm

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := tr
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_SRC_FILES := $(src_files) \
	tr.c

LOCAL_CFLAGS := -Wall
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libc libcutils libm

#LOCAL_CFLAGS := -Wall -Wno-unused-parameter -Werror
#LOCAL_CFLAGS := -Wall -Wno-unused-parameter
#LOCAL_FORCE_STATIC_EXECUTABLE := true
#LOCAL_STATIC_LIBRARIES := libc libcutils libm
#LOCAL_STATIC_LIBRARIES := libc libcutils libm libpthread libdl 
#LOCAL_SHARED_LIBRARIES := libc libcutils

include $(BUILD_EXECUTABLE)
LOCAL_MODULE:= tr
