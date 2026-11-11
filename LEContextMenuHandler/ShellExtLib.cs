/********************************** Module Header **********************************\
Module Name:  ShellExtLib.cs
Project:      CSShellExtContextMenuHandler
Copyright (c) Microsoft Corporation.

The file declares the imported Shell interfaces: IShellExtInit and IContextMenu, 
implements the helper functions for registering and unregistering a shell context 
menu handler, and declares the Win32 enums, structs, consts, and functions used by 
the code sample.

This source is subject to the Microsoft Public License.
See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
All other rights reserved.

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER 
EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***********************************************************************************/

#region Using directives

using System;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32;

#endregion

namespace LEContextMenuHandler
{

    #region Shell Interfaces

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("000214e8-0000-0000-c000-000000000046")]
    internal interface IShellExtInit
    {
        void Initialize(
            IntPtr /*LPCITEMIDLIST*/ pidlFolder,
            IntPtr /*LPDATAOBJECT*/ pDataObj,
            IntPtr /*HKEY*/ hKeyProgId);
    }

    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("000214e4-0000-0000-c000-000000000046")]
    internal interface IContextMenu
    {
        [PreserveSig]
        int QueryContextMenu(
            IntPtr /*HMENU*/ hMenu,
            uint iMenu,
            uint idCmdFirst,
            uint idCmdLast,
            uint uFlags);

        void InvokeCommand(IntPtr pici);

        void GetCommandString(
            UIntPtr idCmd,
            uint uFlags,
            IntPtr pReserved,
            StringBuilder pszName,
            uint cchMax);
    }

    #endregion

    #region Enums & Structs

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CMINVOKECOMMANDINFO
    {
        public uint cbSize;
        public uint fMask;
        public IntPtr hwnd;
        public IntPtr verb;
        [MarshalAs(UnmanagedType.LPStr)] public string parameters;
        [MarshalAs(UnmanagedType.LPStr)] public string directory;
        public int nShow;
        public uint dwHotKey;
        public IntPtr hIcon;
    }

    internal enum CLIPFORMAT : uint
    {
        CF_HDROP = 15
    }

    [Flags]
    internal enum CMF : uint
    {
        CMF_NORMAL = 0x00000000,
        CMF_DEFAULTONLY = 0x00000001,
        CMF_VERBSONLY = 0x00000002,
        CMF_EXPLORE = 0x00000004,
        CMF_NOVERBS = 0x00000008,
        CMF_CANRENAME = 0x00000010,
        CMF_NODEFAULT = 0x00000020,
        CMF_INCLUDESTATIC = 0x00000040,
        CMF_ITEMMENU = 0x00000080,
        CMF_EXTENDEDVERBS = 0x00000100,
        CMF_DISABLEDVERBS = 0x00000200,
        CMF_ASYNCVERBSTATE = 0x00000400,
        CMF_OPTIMIZEFORINVOKE = 0x00000800,
        CMF_SYNCCASCADEMENU = 0x00001000,
        CMF_DONOTPICKDEFAULT = 0x00002000,
        CMF_RESERVED = 0xFFFF0000
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct MENUITEMINFO
    {
        public uint cbSize;
        public MIIM fMask;
        public MFT fType;
        public MFS fState;
        public uint wID;
        public IntPtr hSubMenu;
        public IntPtr hbmpChecked;
        public IntPtr hbmpUnchecked;
        public UIntPtr dwItemData;
        [MarshalAs(UnmanagedType.LPTStr)] public string dwTypeData;
        public uint cch;
        public IntPtr hbmpItem;
    }

    [Flags]
    internal enum MIIM : uint
    {
        MIIM_STATE = 0x00000001,
        MIIM_ID = 0x00000002,
        MIIM_SUBMENU = 0x00000004,
        MIIM_CHECKMARKS = 0x00000008,
        MIIM_TYPE = 0x00000010,
        MIIM_DATA = 0x00000020,
        MIIM_STRING = 0x00000040,
        MIIM_BITMAP = 0x00000080,
        MIIM_FTYPE = 0x00000100
    }

    internal enum MFT : uint
    {
        MFT_STRING = 0x00000000,
        MFT_BITMAP = 0x00000004,
        MFT_MENUBARBREAK = 0x00000020,
        MFT_MENUBREAK = 0x00000040,
        MFT_OWNERDRAW = 0x00000100,
        MFT_RADIOCHECK = 0x00000200,
        MFT_SEPARATOR = 0x00000800,
        MFT_RIGHTORDER = 0x00002000,
        MFT_RIGHTJUSTIFY = 0x00004000
    }

    internal enum MFS : uint
    {
        MFS_ENABLED = 0x00000000,
        MFS_UNCHECKED = 0x00000000,
        MFS_UNHILITE = 0x00000000,
        MFS_GRAYED = 0x00000003,
        MFS_DISABLED = 0x00000003,
        MFS_CHECKED = 0x00000008,
        MFS_HILITE = 0x00000080,
        MFS_DEFAULT = 0x00001000
    }

    #endregion
}