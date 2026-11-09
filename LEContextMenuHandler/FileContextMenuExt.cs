/********************************** Module Header **********************************\
Module Name:  FileContextMenuExt.cs
Project:      CSShellExtContextMenuHandler
Copyright (c) Microsoft Corporation.

The FileContextMenuExt.cs file defines a context menu handler by implementing the 
IShellExtInit and IContextMenu interfaces.

This source is subject to the Microsoft Public License.
See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
All other rights reserved.

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER 
EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***********************************************************************************/

#region Using directives

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;
using System.Windows.Forms;
using LECommonLibrary;
using IDataObject = System.Runtime.InteropServices.ComTypes.IDataObject;

#endregion

namespace LEContextMenuHandler
{
    [ClassInterface(ClassInterfaceType.None)]
    [Guid("C52B9871-E5E9-41FD-B84D-C5ACADBEC7AE"), ComVisible(true)]
    public class FileContextMenuExt : IShellExtInit, IContextMenu
    {
        // The name of the selected file.
        private IntPtr _menuBmpBlue = IntPtr.Zero;
        private IntPtr _menuBmpYellow = IntPtr.Zero;
        private readonly List<LEMenuItem> menuItems = new List<LEMenuItem>();
        private IntPtr _menuBmpGray = IntPtr.Zero;
        private IntPtr _menuBmpPink = IntPtr.Zero;
        private string _selectedFile;
        private string _selectedLauncher;

        public FileContextMenuExt()
        {
            var is4K = SystemHelper.Is4KDisplay();

            //Load the bitmap for the menu item.
            _menuBmpPink = is4K ? Resource.purple_200.GetHbitmap() : Resource.purple.GetHbitmap();
            _menuBmpGray = is4K ? Resource.gray_200.GetHbitmap() : Resource.gray.GetHbitmap();
            _menuBmpBlue = is4K ? Resource.blue_200.GetHbitmap() : Resource.blue.GetHbitmap();
            _menuBmpYellow = is4K ? Resource.yellow_200.GetHbitmap() : Resource.yellow.GetHbitmap();

            //Load default items.
            menuItems.Add(new LEMenuItem(I18n.GetString("Submenu"), null, _menuBmpYellow, ""));
            menuItems.Add(new LEMenuItem(I18n.GetString("RunDefault"), null, _menuBmpYellow, "-run \"%APP%\""));
            menuItems.Add(new LEMenuItem(I18n.GetString("ManageApp"), null, _menuBmpGray, "-manage \"%APP%\""));
            menuItems.Add(new LEMenuItem(I18n.GetString("ManageAll"), null, _menuBmpBlue, "-global"));

            //If global config does not exist, create a new one.
            LEConfig.CheckGlobalConfigFile(true);

            //Load global profiles.
            Array.ForEach(LEConfig.GetProfiles(),
                          p =>
                          menuItems.Add(new LEMenuItem(p.Name,
                                                       p.ShowInMainMenu,
                                                       _menuBmpPink,
                                                       $"-runas \"{p.Guid}\" \"%APP%\"")));
        }

        #region IShellExtInit Members

        /// <summary>
        ///     Initialize the context menu handler.
        /// </summary>
        /// <param name="pidlFolder">
        ///     A pointer to an ITEMIDLIST structure that uniquely identifies a folder.
        /// </param>
        /// <param name="pDataObj">
        ///     A pointer to an IDataObject interface object that can be used to retrieve
        ///     the objects being acted upon.
        /// </param>
        /// <param name="hKeyProgId">
        ///     The registry key for the file object or folder type.
        /// </param>
        public void Initialize(IntPtr pidlFolder, IntPtr pDataObj, IntPtr hKeyProgId)
        {
            if (pDataObj == IntPtr.Zero)
            {
                throw new ArgumentException();
            }

            var fe = new FORMATETC
                     {
                         cfFormat = (short)CLIPFORMAT.CF_HDROP,
                         ptd = IntPtr.Zero,
                         dwAspect = DVASPECT.DVASPECT_CONTENT,
                         lindex = -1,
                         tymed = TYMED.TYMED_HGLOBAL
                     };
            STGMEDIUM stm;

            // The pDataObj pointer contains the objects being acted upon. In this 
            // example, we get an HDROP handle for enumerating the selected files 
            // and folders.
            var dataObject = (IDataObject)Marshal.GetObjectForIUnknown(pDataObj);
            dataObject.GetData(ref fe, out stm);

            try
            {
                // Get an HDROP handle.
                var hDrop = stm.unionmember;
                if (hDrop == IntPtr.Zero)
                {
                    throw new ArgumentException();
                }

                // Determine how many files are involved in this operation.
                var nFiles = NativeMethods.DragQueryFile(hDrop, uint.MaxValue, null, 0);

                // This code sample displays the custom context menu item when only 
                // one file is selected. 
                if (nFiles == 1)
                {
                    _selectedFile = GetDropFileName(hDrop);
                    _selectedLauncher = GlobalHelper.GetLauncherPath(_selectedFile);
                    if (_selectedLauncher == null)
                        Marshal.ThrowExceptionForHR(WinError.E_FAIL);
                }
                else
                {
                    Marshal.ThrowExceptionForHR(WinError.E_FAIL);
                }
            }
            finally
            {
                NativeMethods.ReleaseStgMedium(ref stm);
            }
        }

        #endregion



        private static string GetDropFileName(IntPtr hDrop)
        {
            // Query the required length first; MAX_PATH is not a contract for shell paths.
            var fileNameLength = NativeMethods.DragQueryFile(hDrop, 0, null, 0);
            if (fileNameLength == 0)
                Marshal.ThrowExceptionForHR(WinError.E_FAIL);
            var fileName = new StringBuilder(checked((int)fileNameLength + 1));
            if (NativeMethods.DragQueryFile(hDrop, 0, fileName, fileName.Capacity) != fileNameLength)
                Marshal.ThrowExceptionForHR(WinError.E_FAIL);
            return fileName.ToString();
        }

        ~FileContextMenuExt()
        {
            ReleaseBitmap(ref _menuBmpPink);
            ReleaseBitmap(ref _menuBmpGray);
            ReleaseBitmap(ref _menuBmpBlue);
            ReleaseBitmap(ref _menuBmpYellow);
        }

        private static void ReleaseBitmap(ref IntPtr bitmap)
        {
            var handle = System.Threading.Interlocked.Exchange(ref bitmap, IntPtr.Zero);
            if (handle != IntPtr.Zero)
                NativeMethods.DeleteObject(handle);
        }

        private void OnVerbDisplayFileName(string cmd)
        {
            Process.Start(_selectedLauncher, cmd.Replace("%APP%", _selectedFile));
        }

        private int RegisterMenuItem(uint id,
                                     uint idCmdFirst,
                                     string text,
                                     IntPtr bitmap,
                                     IntPtr subMenu,
                                     uint position,
                                     IntPtr registerTo)
        {
            var sub = new MENUITEMINFO();
            sub.cbSize = (uint)Marshal.SizeOf(sub);

            var m = MIIM.MIIM_STRING | MIIM.MIIM_FTYPE | MIIM.MIIM_ID | MIIM.MIIM_STATE;
            if (bitmap != IntPtr.Zero)
                m |= MIIM.MIIM_BITMAP;
            if (subMenu != IntPtr.Zero)
                m |= MIIM.MIIM_SUBMENU;
            sub.fMask = m;

            sub.wID = idCmdFirst + id;
            sub.fType = MFT.MFT_STRING;
            sub.dwTypeData = text;
            sub.hSubMenu = subMenu;
            sub.fState = MFS.MFS_ENABLED;
            sub.hbmpItem = bitmap;

            if (!NativeMethods.InsertMenuItem(registerTo, position, true, ref sub))
                return Marshal.GetHRForLastWin32Error();
            return 0;
        }

        #region Shell Extension Registration

        [ComRegisterFunction]
        public static void Register(Type t)
        {
        }

        [ComUnregisterFunction]
        public static void Unregister(Type t)
        {
        }

        #endregion

        #region IContextMenu Members

        /// <summary>
        ///     Add commands to a shortcut menu.
        /// </summary>
        /// <param name="hMenu">A handle to the shortcut menu.</param>
        /// <param name="iMenu">
        ///     The zero-based position at which to insert the first new menu item.
        /// </param>
        /// <param name="idCmdFirst">
        ///     The minimum value that the handler can specify for a menu item ID.
        /// </param>
        /// <param name="idCmdLast">
        ///     The maximum value that the handler can specify for a menu item ID.
        /// </param>
        /// <param name="uFlags">
        ///     Optional flags that specify how the shortcut menu can be changed.
        /// </param>
        /// <returns>
        ///     If successful, returns an HRESULT value that has its severity value set
        ///     to SEVERITY_SUCCESS and its code value set to the offset of the largest
        ///     command identifier that was assigned, plus one.
        /// </returns>
        public int QueryContextMenu(
            IntPtr hMenu,
            uint iMenu,
            uint idCmdFirst,
            uint idCmdLast,
            uint uFlags)
        {
            if (((uint)CMF.CMF_DEFAULTONLY & uFlags) != 0 || idCmdLast < idCmdFirst)
                return 0;

            // Keep offsets equal to menuItems indices for InvokeCommand.
            int count = (int)Math.Min((ulong)menuItems.Count,
                Math.Min((ulong)idCmdLast - idCmdFirst + 1, (ulong)ushort.MaxValue));
            if (count < 2)
                return 0;

            var subMenu = NativeMethods.CreatePopupMenu();
            if (subMenu == IntPtr.Zero)
                return Marshal.GetHRForLastWin32Error();
            uint inserted = 0;
            bool attached = false;
            try
            {
                uint position = 0;
                for (int i = 4; i < count; i++)
                {
                    var profile = menuItems[i];
                    if (profile.ShowInMainMenu != true)
                        Marshal.ThrowExceptionForHR(RegisterMenuItem((uint)i, idCmdFirst, profile.Text,
                            profile.Bitmap, IntPtr.Zero, position++, subMenu));
                }
                if (position > 0)
                    InsertSeparator(subMenu, position++);
                var item = menuItems[1];
                Marshal.ThrowExceptionForHR(RegisterMenuItem(1, idCmdFirst, item.Text,
                    item.Bitmap, IntPtr.Zero, position++, subMenu));
                if (count > 2)
                    InsertSeparator(subMenu, position++);
                for (int i = 2; i < Math.Min(count, 4); i++)
                {
                    item = menuItems[i];
                    Marshal.ThrowExceptionForHR(RegisterMenuItem((uint)i, idCmdFirst, item.Text,
                        item.Bitmap, IntPtr.Zero, position++, subMenu));
                }

                InsertSeparator(hMenu, iMenu);
                inserted++;
                for (int i = 4; i < count; i++)
                {
                    item = menuItems[i];
                    if (item.ShowInMainMenu == true)
                    {
                        Marshal.ThrowExceptionForHR(RegisterMenuItem((uint)i, idCmdFirst, item.Text,
                            item.Bitmap, IntPtr.Zero, iMenu + inserted, hMenu));
                        inserted++;
                    }
                }
                item = menuItems[0];
                Marshal.ThrowExceptionForHR(RegisterMenuItem(0, idCmdFirst, item.Text,
                    item.Bitmap, subMenu, iMenu + inserted, hMenu));
                attached = true;
                return count;
            }
            catch (Exception e)
            {
                while (inserted > 0)
                {
                    RemoveMenu(hMenu, iMenu, 0x400); // MF_BYPOSITION
                    inserted--;
                }
                return Marshal.GetHRForException(e);
            }
            finally
            {
                if (!attached)
                    DestroyMenu(subMenu);
            }
        }

        private static void InsertSeparator(IntPtr menu, uint position)
        {
            var separator = new MENUITEMINFO();
            separator.cbSize = (uint)Marshal.SizeOf(separator);
            separator.fMask = MIIM.MIIM_FTYPE;
            separator.fType = MFT.MFT_SEPARATOR;
            if (!NativeMethods.InsertMenuItem(menu, position, true, ref separator))
                throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        }

        [DllImport("user32.dll")]
        private static extern bool DestroyMenu(IntPtr menu);

        [DllImport("user32.dll")]
        private static extern bool RemoveMenu(IntPtr menu, uint position, uint flags);

        /// <summary>
        ///     Carry out the command associated with a shortcut menu item.
        /// </summary>
        /// <param name="pici">
        ///     A pointer to a CMINVOKECOMMANDINFO or CMINVOKECOMMANDINFOEX structure
        ///     containing information about the command.
        /// </param>
        public void InvokeCommand(IntPtr pici)
        {
            var ici = (CMINVOKECOMMANDINFO)Marshal.PtrToStructure(pici, typeof (CMINVOKECOMMANDINFO));

            var item = menuItems[NativeMethods.LowWord(ici.verb.ToInt32())];

            OnVerbDisplayFileName(item.Commands);
        }

        /// <summary>
        ///     Get information about a shortcut menu command, including the help string
        ///     and the language-independent, or canonical, name for the command.
        /// </summary>
        /// <param name="idCmd">Menu command identifier offset.</param>
        /// <param name="uFlags">
        ///     Flags specifying the information to return. This parameter can have one
        ///     of the following values: GCS_HELPTEXTA, GCS_HELPTEXTW, GCS_VALIDATEA,
        ///     GCS_VALIDATEW, GCS_VERBA, GCS_VERBW.
        /// </param>
        /// <param name="pReserved">Reserved. Must be IntPtr.Zero</param>
        /// <param name="pszName">
        ///     The address of the buffer to receive the null-terminated string being
        ///     retrieved.
        /// </param>
        /// <param name="cchMax">
        ///     Size of the buffer, in characters, to receive the null-terminated string.
        /// </param>
        public void GetCommandString(
            UIntPtr idCmd,
            uint uFlags,
            IntPtr pReserved,
            StringBuilder pszName,
            uint cchMax)
        {
        }

        #endregion
    }
}