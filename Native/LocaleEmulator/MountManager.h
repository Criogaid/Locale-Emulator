#pragma once

// SDK 未公开的 Mount Manager 用户态协议；布局与系统设备控制接口一致。
#define MOUNTMGR_DEVICE_NAME L"\\Device\\MountPointManager"
#define IOCTL_MOUNTMGR_QUERY_DOS_VOLUME_PATH CTL_CODE('m', 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
typedef struct _MOUNTMGR_TARGET_NAME {
    USHORT DeviceNameLength;
    WCHAR DeviceName[1];
} MOUNTMGR_TARGET_NAME, *PMOUNTMGR_TARGET_NAME;
typedef struct _MOUNTMGR_VOLUME_PATHS {
    ULONG MultiSzLength;
    WCHAR MultiSz[1];
} MOUNTMGR_VOLUME_PATHS, *PMOUNTMGR_VOLUME_PATHS;
