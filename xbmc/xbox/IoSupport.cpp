/*
* XBMC Media Center
* Copyright (c) 2002 d7o3g4q and RUNTiME
* Portions Copyright (c) by the authors of ffmpeg and xvid
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// IoSupport.cpp: implementation of the CIoSupport class.
//
//////////////////////////////////////////////////////////////////////

#include "system.h"
#include "IoSupport.h"
#include "settings/Settings.h"
#include "utils/log.h"
#ifdef HAS_UNDOCUMENTED
#ifdef _XBOX
#include "Undocumented.h"
#include "XKExports.h"
#else
#include "ntddcdrm.h"
#endif
#endif

#define NT_STATUS_OBJECT_NAME_NOT_FOUND long(0xC0000000 | 0x0034)
#define NT_STATUS_VOLUME_DISMOUNTED     long(0xC0000000 | 0x026E)

typedef struct
{
  char* szDriveName;
  char* szDevice;
  int iPartition;
}
stDriveMapping;

#ifdef _XBOX
stDriveMapping driveMapping[] =
  {
	{ "DVD-ROM", "Cdrom0", -1},
	{ "D", "Cdrom0", -1},
	{ "HDD0-C", "Harddisk0\\Partition2", 2},
	{ "C", "Harddisk0\\Partition2", 2},
	{ "HDD0-E", "Harddisk0\\Partition1", 1},
	{ "E", "Harddisk0\\Partition1", 1},
	{ "HDD0-X", "Harddisk0\\Partition3", 3},
	{ "X", "Harddisk0\\Partition3", 3},
	{ "HDD0-Y", "Harddisk0\\Partition4", 4},
	{ "Y", "Harddisk0\\Partition4", 4},
	{ "HDD0-Z", "Harddisk0\\Partition5", 5},
	{ "Z", "Harddisk0\\Partition5", 5},
	{ "HDD0-F", "Harddisk0\\Partition6", 6},
	{ "F", "Harddisk0\\Partition6", 6},
	{ "HDD0-G", "Harddisk0\\Partition7", 7},
	{ "G", "Harddisk0\\Partition7", 7},
	{ "HDD0-H", "Harddisk0\\Partition8", 8},
	{ "HDD0-I", "Harddisk0\\Partition9", 9},
	{ "HDD0-J", "Harddisk0\\Partition10", 10},
	{ "HDD0-K", "Harddisk0\\Partition11", 11},
	{ "HDD0-L", "Harddisk0\\Partition12", 12},
	{ "HDD0-M", "Harddisk0\\Partition13", 13},
	{ "HDD0-N", "Harddisk0\\Partition13", 14},
	{ "HDD1-C", "Harddisk1\\Partition2", 2},
	{ "HDD1-E", "Harddisk1\\Partition1", 1},
	{ "HDD1-X", "Harddisk1\\Partition3", 3},
	{ "HDD1-Y", "Harddisk1\\Partition4", 4},
	{ "HDD1-Z", "Harddisk1\\Partition5", 5},
	{ "HDD1-F", "Harddisk1\\Partition6", 6},
	{ "HDD1-G", "Harddisk1\\Partition7", 7},
	{ "HDD1-H", "Harddisk1\\Partition8", 8},
	{ "HDD1-I", "Harddisk1\\Partition9", 9},
	{ "HDD1-J", "Harddisk1\\Partition10", 10},
	{ "HDD1-K", "Harddisk1\\Partition11", 11},
	{ "HDD1-L", "Harddisk1\\Partition12", 12},
	{ "HDD1-M", "Harddisk1\\Partition13", 13},
	{ "HDD1-N", "Harddisk1\\Partition13", 14},
  };

#else
stDriveMapping driveMapping[] =
  {
    { "C", "C:", 2},
    { "D", "D:", -1},
    { "E", "E:", 1},
    { "X", "X:", 3},
    { "Y", "Y:", 4},
    { "Z", "Z:", 5},
  };

#include "../../Tools/Win32/XBMC_PC.h"

#endif

#define NUM_OF_DRIVES ( sizeof( driveMapping) / sizeof( driveMapping[0] ) )


PVOID CIoSupport::m_rawXferBuffer;

// szDriveName e.g. 'DVD-ROM'
// szDevice e.g. "Cdrom0" or "Harddisk0\Partition6"
HRESULT CIoSupport::MapDriveLetter(const char* szDriveName, const char* szDevice)
{
#ifdef _XBOX
  char szSourceDevice[MAX_PATH+32];
  char szDestinationDrive[16];
  NTSTATUS status;

  CLog::Log(LOGNOTICE, "Mapping drive %s to %s", szDriveName, szDevice);

  sprintf(szSourceDevice, "\\Device\\%s", szDevice);
  sprintf(szDestinationDrive, "\\??\\%s:", szDriveName);

  ANSI_STRING DeviceName, LinkName;

  RtlInitAnsiString(&DeviceName, szSourceDevice);
  RtlInitAnsiString(&LinkName, szDestinationDrive);

  status = IoCreateSymbolicLink(&LinkName, &DeviceName);

  if (!NT_SUCCESS(status))
    CLog::Log(LOGERROR, "Failed to create symbolic link!  (status=0x%08x)", status);

  return status;
#else
  if ((strnicmp(szDevice, "Harddisk0", 9) == 0) ||
      (strnicmp(szDevice, "Cdrom", 5) == 0))
    return S_OK;
  return E_FAIL;
#endif
}

// cDriveLetter e.g. 'D'
HRESULT CIoSupport::UnmapDriveLetter(const char* szDriveName)
{
#ifdef _XBOX
  char szDestinationDrive[16];
  ANSI_STRING LinkName;
  NTSTATUS status;

  sprintf(szDestinationDrive, "\\??\\%s:", szDriveName);
  RtlInitAnsiString(&LinkName, szDestinationDrive);

  status =  IoDeleteSymbolicLink(&LinkName);

  if (NT_SUCCESS(status))
    CLog::Log(LOGNOTICE, "Unmapped drive %s", szDriveName);
  else if(status != NT_STATUS_OBJECT_NAME_NOT_FOUND)
    CLog::Log(LOGERROR, "Failed to delete symbolic link!  (status=0x%08x)", status);

  return status;
#else
  return S_OK;
#endif
}

HRESULT CIoSupport::RemapDriveLetter(const char* szDriveName, const char* szDevice)
{
  UnmapDriveLetter(szDriveName);

  return MapDriveLetter(szDriveName, szDevice);
}
// to be used with CdRom devices.
HRESULT CIoSupport::Dismount(const char* szDevice)
{
#ifdef _XBOX
  char szSourceDevice[MAX_PATH+32];
  ANSI_STRING DeviceName;
  NTSTATUS status;

  sprintf(szSourceDevice, "\\Device\\%s", szDevice);

  RtlInitAnsiString(&DeviceName, szSourceDevice);

  status = IoDismountVolumeByName(&DeviceName);

  if (NT_SUCCESS(status))
    CLog::Log(LOGNOTICE, "Dismounted %s", szDevice);
  else if(status != NT_STATUS_VOLUME_DISMOUNTED)
    CLog::Log(LOGERROR, "Failed to dismount volume!  (status=0x%08x)", status);

  return status;
#else
  return S_OK;
#endif
}

bool CIoSupport::CompareString(const char* value1, const char* value2, bool caseInsensitive)
{
	uint32_t valueLength1 = strlen(value1);
	uint32_t valueLength2 = strlen(value2);
	if (valueLength1 != valueLength2)
	{
		return false;
	}
	if (caseInsensitive) 
	{
		return strnicmp(value1, value2, valueLength1) == 0;
	}
	return strncmp(value1, value2, valueLength1) == 0;
}

void CIoSupport::GetDriveNameFromPath(const char* szDrivePath, char* szDriveName)
{
	const char *colon_pos = strchr(szDrivePath, ':');
	if (colon_pos != NULL) 
	{
		size_t length = colon_pos - szDrivePath;
		strncpy(szDriveName, szDrivePath, length);
		szDriveName[length] = '\0';
	} 
	else
	{
		strcpy(szDriveName, szDrivePath);
	}
}

void CIoSupport::GetPathFromDevicePath(const char* szDevicePath, char* szPath)
{
	const char *colon_pos = strchr(szDevicePath, ':');
	if (colon_pos != NULL) 
	{
		strcpy(szPath, colon_pos + 1); 
	} 
	else 
	{
		szPath[0] = '\0';
	}
}

void CIoSupport::GetPartition(const char* szDriveName, char* szPartition)
{
  for (unsigned int i=0; i < NUM_OF_DRIVES; i++)
    if (CompareString(driveMapping[i].szDriveName, szDriveName, true))
    {
      strcpy(szPartition, driveMapping[i].szDevice);
      return;
    }
  *szPartition = 0;
}

void CIoSupport::GetDrive(const char* szPartition, char* szDriveName, int maxDriveNameLen)
{
  int part_str_len = strlen(szPartition);
  int drive_num;
  int part_num;

  if (part_str_len < 19)
  {
    *szDriveName = 0;
    return;
  }
        
  //01234567890123456789
  //Harddisk0\Partition2
  //HDD0-C

  drive_num = atoi(szPartition + 8);
  part_num = atoi(szPartition + 19);

  for (unsigned int i=0; i < NUM_OF_DRIVES; i++)
  {
    if (strnicmp(driveMapping[i].szDevice, szPartition, strlen(driveMapping[i].szDevice)) == 0)
    {
	  sprintf(szDriveName, "%s", driveMapping[i].szDriveName);
      return;
    }
  }
  *szDriveName = 0;
}

HRESULT CIoSupport::EjectTray()
{
#ifdef _WIN32PC
  BOOL bRet= FALSE;
  char cDL = cDriveLetter;
  if( !cDL )
  {
    char* dvdDevice = CLibcdio::GetInstance()->GetDeviceFileName();
    cDL = dvdDevice[4];
  }
  
  CStdString strVolFormat; strVolFormat.Format( _T("\\\\.\\%c:" ), cDL);
  HANDLE hDrive= CreateFile( strVolFormat, GENERIC_READ, FILE_SHARE_READ, 
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  CStdString strRootFormat; strRootFormat.Format( _T("%c:\\"), cDL);
  if( ( hDrive != INVALID_HANDLE_VALUE || GetLastError() == NO_ERROR) && 
      ( GetDriveType( strRootFormat ) == DRIVE_CDROM ) )
  {
    DWORD dwDummy;
    bRet= DeviceIoControl( hDrive, ( bEject ? IOCTL_STORAGE_EJECT_MEDIA : IOCTL_STORAGE_LOAD_MEDIA), 
                                    NULL, 0, NULL, 0, &dwDummy, NULL);
    CloseHandle( hDrive );
  }
  return bRet? S_OK : S_FALSE;
#endif
#ifdef _XBOX
  HalWriteSMBusValue(0x20, 0x0C, FALSE, 0);  // eject tray
#endif
  return S_OK;
}

HRESULT CIoSupport::CloseTray()
{
#ifdef _XBOX
  HalWriteSMBusValue(0x20, 0x0C, FALSE, 1);  // close tray
#endif
  return S_OK;
}

DWORD CIoSupport::GetTrayState()
{
#ifdef _XBOX
  DWORD dwTrayState, dwTrayCount;
  if (g_advancedSettings.m_usePCDVDROM)
  {
    dwTrayState = TRAY_CLOSED_MEDIA_PRESENT;
  }
  else
  {
    HalReadSMCTrayState(&dwTrayState, &dwTrayCount);
  }

  return dwTrayState;
#endif
  return DRIVE_NOT_READY;
}

HRESULT CIoSupport::ToggleTray()
{
  if (GetTrayState() == TRAY_OPEN || GetTrayState() == DRIVE_OPEN)
    return CloseTray();
  else
    return EjectTray();
}

HRESULT CIoSupport::Shutdown()
{
#ifdef _XBOX
  // fails assertion on debug bios (symptom lockup unless running dr watson)
  // so you can continue past the failed assertion).
  if (IsDebug())
    return E_FAIL;
  KeRaiseIrqlToDpcLevel();
  HalInitiateShutdown();
#endif
  return S_OK;
}

HANDLE CIoSupport::OpenCDROM()
{
  HANDLE hDevice;

#ifdef _XBOX
  IO_STATUS_BLOCK status;
  ANSI_STRING filename;
  OBJECT_ATTRIBUTES attributes;
  RtlInitAnsiString(&filename, "\\Device\\Cdrom0");
  InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);
  if (!NT_SUCCESS(NtOpenFile(&hDevice,
                             GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                             &attributes,
                             &status,
                             FILE_SHARE_READ,
                             FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
  {
    return NULL;
  }
#else

  hDevice = CreateFile("\\\\.\\Cdrom0", GENERIC_READ, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING,
                       FILE_FLAG_RANDOM_ACCESS, NULL );

#endif
  return hDevice;
}

void CIoSupport::AllocReadBuffer()
{
  m_rawXferBuffer = GlobalAlloc(GPTR, RAW_SECTOR_SIZE);
}

void CIoSupport::FreeReadBuffer()
{
  GlobalFree(m_rawXferBuffer);
}

INT CIoSupport::ReadSector(HANDLE hDevice, DWORD dwSector, LPSTR lpczBuffer)

{
  DWORD dwRead;
  DWORD dwSectorSize = 2048;
  LARGE_INTEGER Displacement;
  Displacement.QuadPart = ((INT64)dwSector) * dwSectorSize;

  for (int i = 0; i < 5; i++)
  {
    SetFilePointer(hDevice, Displacement.LowPart, &Displacement.HighPart, FILE_BEGIN);

    if (ReadFile(hDevice, m_rawXferBuffer, dwSectorSize, &dwRead, NULL))
    {
      memcpy(lpczBuffer, m_rawXferBuffer, dwSectorSize);
      return dwRead;
    }
  }

  OutputDebugString("CD Read error\n");
  return -1;
}


INT CIoSupport::ReadSectorMode2(HANDLE hDevice, DWORD dwSector, LPSTR lpczBuffer)
{
#ifdef HAS_DVD_DRIVE
  DWORD dwBytesReturned;
  RAW_READ_INFO rawRead = {0};

  // Oddly enough, DiskOffset uses the Red Book sector size
  rawRead.DiskOffset.QuadPart = 2048 * dwSector;
  rawRead.SectorCount = 1;
  rawRead.TrackMode = XAForm2;

  for (int i = 0; i < 5; i++)
  {
    if ( DeviceIoControl( hDevice,
                          IOCTL_CDROM_RAW_READ,
                          &rawRead,
                          sizeof(RAW_READ_INFO),
                          m_rawXferBuffer,
                          RAW_SECTOR_SIZE,
                          &dwBytesReturned,
                          NULL ) != 0 )
    {
      memcpy(lpczBuffer, (char*)m_rawXferBuffer+MODE2_DATA_START, MODE2_DATA_SIZE);
      return MODE2_DATA_SIZE;
    }
    else
    {
      int iErr = GetLastError();
      //   printf("%i\n", iErr);
    }
  }
#endif
  return -1;
}

INT CIoSupport::ReadSectorCDDA(HANDLE hDevice, DWORD dwSector, LPSTR lpczBuffer)
{
#ifdef HAS_DVD_DRIVE
  DWORD dwBytesReturned;
  RAW_READ_INFO rawRead;

  // Oddly enough, DiskOffset uses the Red Book sector size
  rawRead.DiskOffset.QuadPart = 2048 * dwSector;
  rawRead.SectorCount = 1;
  rawRead.TrackMode = CDDA;

  for (int i = 0; i < 5; i++)
  {
    if ( DeviceIoControl( hDevice,
                          IOCTL_CDROM_RAW_READ,
                          &rawRead,
                          sizeof(RAW_READ_INFO),
                          m_rawXferBuffer,
                          sizeof(RAW_SECTOR_SIZE),
                          &dwBytesReturned,
                          NULL ) != 0 )
    {
      memcpy(lpczBuffer, m_rawXferBuffer, RAW_SECTOR_SIZE);
      return RAW_SECTOR_SIZE;
    }
  }
#endif
  return -1;
}

VOID CIoSupport::CloseCDROM(HANDLE hDevice)
{
  CloseHandle(hDevice);
}

// returns true if this is a debug machine
BOOL CIoSupport::IsDebug()
{
#ifdef _XBOX
  return (XboxKrnlVersion->Qfe & 0x8000) || ((DWORD)XboxHardwareInfo & 0x10);
#else
  return FALSE;
#endif
}


VOID CIoSupport::GetXbePath(char* szDest)
{
#ifdef _XBOX
  //Function to get the XBE Path like:
  //E:\DevKit\xbplayer\xbplayer.xbe

  char szTemp[MAX_PATH];
  char szDriveName[MAX_PATH];

  strncpy(szTemp, XeImageFileName->Buffer + 8, XeImageFileName->Length - 8);
  szTemp[20] = 0;
  GetDrive(szTemp, szDriveName, MAX_PATH);

  strncpy(szTemp, XeImageFileName->Buffer + 29, XeImageFileName->Length - 29);
  szTemp[XeImageFileName->Length - 29] = 0;

  sprintf(szDest, "%s:\\%s", szDriveName, szTemp);

#else
  GetCurrentDirectory(XBMC_MAX_PATH, szDest);
  strcat(szDest, "\\XBMC_PC.exe");
#endif
}

bool CIoSupport::IsDrivePath(const char* szPath)
{
  char szDriveName[MAX_PATH];
  GetDriveNameFromPath(szPath, szDriveName);
  for (unsigned int i=0; i < NUM_OF_DRIVES; i++)
  {
    if (CompareString(driveMapping[i].szDriveName, szDriveName, true))
    {
      return true;
    }
  }
  return false;
}

bool CIoSupport::DriveExists(const char* szDriveName)
{
#ifdef _XBOX
  char rootPath[1024];
  strcpy(rootPath, szDriveName);
  strcat(rootPath, ":\\");

  ULARGE_INTEGER totalNumberOfBytes;
  BOOL status = GetDiskFreeSpaceExA(rootPath, NULL, &totalNumberOfBytes, NULL);
  if (status == 0) 
  {
    return 0;
  }
  return totalNumberOfBytes.QuadPart != 0;
#else
  char upperLetter = toupper(szDriveName[0]);
  if (upperLetter < 'A' || upperLetter > 'Z')
    return false;

  DWORD drivelist;
  DWORD bitposition = upperLetter - 'A';

  drivelist = GetLogicalDrives();

  if (!drivelist)
    return false;

  return (drivelist >> bitposition) & 1;
#endif
}

bool CIoSupport::PartitionExists(int nDrive, int nPartition)
{
#ifdef _XBOX
  char szPartition[32];
  ANSI_STRING part_string;
  NTSTATUS status;
  HANDLE hTemp;
  OBJECT_ATTRIBUTES oa;
  IO_STATUS_BLOCK iosb;

  sprintf(szPartition, "\\Device\\Harddisk%u\\Partition%u", nDrive, nPartition);
  RtlInitAnsiString(&part_string, szPartition);

  oa.Attributes = OBJ_CASE_INSENSITIVE;
  oa.ObjectName = &part_string;
  oa.RootDirectory = 0;

  status = NtOpenFile(&hTemp, GENERIC_READ | GENERIC_WRITE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_ALERT);

  if (NT_SUCCESS(status))
  {
    CloseHandle(hTemp);
    return true;
  }

  return false;
#else
  return false;
#endif
}