
#include "xboxlcd.h"
#include "conio.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "undocumented.h"

#define SCROLL_SPEED_IN_MSEC 250
#define XBOX_LCD_COMMAND_MODE 0x80
#define XBOX_LCD_DATA_MODE 0x40

#define US2066_DISPLAY_MODE 0x08
#define US2066_DISPLAY_MODE_OFF_FLAG 0x00
#define US2066_DISPLAY_MODE_ON_FLAG 0x04
#define US2066_DISPLAY_MODE_CURSOR_OFF_FLAG 0x00
#define US2066_DISPLAY_MODE_CURSOR_ON_FLAG 0x02
#define US2066_DISPLAY_MODE_BLINK_OFF_FLAG 0x00
#define US2066_DISPLAY_MODE_BLINK_ON_FLAG 0x01

#define US2066_EXTENDED_MDOE 0x08
#define US2066_EXTENDED_MDOE_5DOT_FLAG 0x00
#define US2066_EXTENDED_MDOE_6DOT_FLAG 0x04
#define US2066_EXTENDED_MDOE_INVERT_CURSOR_OFF_FLAG 0x00
#define US2066_EXTENDED_MDOE_INVERT_CURSOR_FLAG 0x02
#define US2066_EXTENDED_MDOE_1TO2ROWS_FLAG 0x00
#define US2066_EXTENDED_MDOE_3TO4ROWS_FLAG 0x01

//*************************************************************************************************************
CXboxLCD::CXboxLCD()
{
  m_iActualpos=0;
  m_iRows    = 4;
  m_iColumns = 20;        // display rows each line
  m_iBackLight=32;
  m_iContrast=50;     

  if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_LCD_KS0073)
  {
    // Special case: it's the KS0073
    m_iRow1adr = 0x00;
    m_iRow2adr = 0x20;
    m_iRow3adr = 0x40;
    m_iRow4adr = 0x60;
  }
  else
  {
    // We assume that it's a HD44780 compatible
    m_iRow1adr = 0x00;
    m_iRow2adr = 0x40;
    m_iRow3adr = 0x14;
    m_iRow4adr = 0x54;
  }
}

//*************************************************************************************************************
CXboxLCD::~CXboxLCD()
{
}

//*************************************************************************************************************
void CXboxLCD::Initialize()
{
  StopThread();
  if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_NONE) 
  {
    CLog::Log(LOGINFO, "lcd not used");
    return;
  }
  ILCD::Initialize();
  Create();
  
}
void CXboxLCD::SetBackLight(int iLight)
{
  m_iBackLight=iLight;
}
void CXboxLCD::SetContrast(int iContrast)
{
  m_iContrast=iContrast;
}

//*************************************************************************************************************
void CXboxLCD::Stop()
{
  if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_NONE) return;
  StopThread();
}

//*************************************************************************************************************
void CXboxLCD::SetLine(int iLine, const CStdString& strLine)
{
	if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_NONE) 
		return;
	if (iLine < 0 || iLine >= (int)m_iRows) 
		return;

	CStdString strLineLong=strLine;
	//strLineLong.Trim();
	StringToLCDCharSet(strLineLong);

	while (strLineLong.size() < m_iColumns) 
		strLineLong+=" ";
	if (strLineLong != m_strLine[iLine])
	{
		m_bUpdate[iLine] = true;
		m_strLine[iLine] = strLineLong;
		m_event.Set();
	}
}


//************************************************************************************************************************
// wait_us: delay routine
// Input: (wait in ~us)
//************************************************************************************************************************
void CXboxLCD::wait_us(unsigned int value) 
{
	// 1 us = 1000msec
	int iValue = value / 30;
	if (iValue>10) 
		iValue = 10;
	if (iValue) 
		Sleep(iValue);
}


//************************************************************************************************************************
// DisplayOut: writes command or datas to display
// Input: (Value to write, token as CMD = Command / DAT = DATAs / INI for switching to 4 bit mode)
//************************************************************************************************************************
void CXboxLCD::DisplayOut(unsigned char data, unsigned char command) 
{
	HalWriteSMBusValue(0x3C << 1, command, FALSE, data);
}

//************************************************************************************************************************
//DisplayBuildCustomChars: load customized characters to character ram of display, resets cursor to pos 0
//************************************************************************************************************************
void CXboxLCD::DisplayBuildCustomChars() 
{

}


//************************************************************************************************************************
// DisplaySetPos: sets cursor position
// Input: (row position, line number from 0 to 3)
//************************************************************************************************************************
void CXboxLCD::DisplaySetPos(unsigned char pos, unsigned char line) 
{
	if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_LCD_KS0073)
	{
		int row_offsets[] = { 0x00, 0x20, 0x40, 0x60 };
		DisplayOut(0x80 | row_offsets[line] | pos, XBOX_LCD_COMMAND_MODE);
	}
	else
	{
		int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
		DisplayOut(0x80 | row_offsets[line] | pos, XBOX_LCD_COMMAND_MODE);
	}
}

//************************************************************************************************************************
// DisplayWriteFixText: write a fixed text to actual cursor position
// Input: ("fixed text like")
//************************************************************************************************************************
void CXboxLCD::DisplayWriteFixtext(const char *textstring)
{ 
  unsigned char  c;
  while (c = *textstring++) {
    DisplayOut(c, XBOX_LCD_DATA_MODE);
  }
} 


//************************************************************************************************************************
// DisplayWriteString: write a string to acutal cursor position 
// Input: (pointer to a 0x00 terminated string)
//************************************************************************************************************************

void CXboxLCD::DisplayWriteString(char *pointer) 
{
  /* display a normal 0x00 terminated string on the LCD display */
  unsigned char c;
  do {
    c = *pointer;
    if (c == 0x00)
      break;

    DisplayOut(c, XBOX_LCD_DATA_MODE);
    *pointer++;
    } while(1);
}		


//************************************************************************************************************************
// DisplayClearChars:  clears a number of chars in a line and resets cursor position to it's startposition
// Input: (Startposition of clear in row, row number, number of chars to clear)
//************************************************************************************************************************
void CXboxLCD::DisplayClearChars(unsigned char startpos , unsigned char line, unsigned char lenght) 
{
  int i;

  DisplaySetPos(startpos,line);
  for (i=0;i<lenght; i++){
    DisplayOut(0x20, XBOX_LCD_DATA_MODE);
  }
  DisplaySetPos(startpos,line);
}


//************************************************************************************************************************
// DisplayProgressBar: shows a grafic bar staring at actual cursor position
// Input: (percent of bar to display, lenght of whole bar in chars when 100 %)
//************************************************************************************************************************
void CXboxLCD::DisplayProgressBar(unsigned char percent, unsigned char charcnt) 
{

}
//************************************************************************************************************************
//Set brightness level 
//************************************************************************************************************************
void CXboxLCD::DisplaySetBacklight(unsigned char level) 
{
  if (level<0) level=0;
  if (level>100) level=100;
  //m_xenium.SetBacklight(level/4);
}
//************************************************************************************************************************
//Set Contrast level
//************************************************************************************************************************
void CXboxLCD::DisplaySetContrast(unsigned char level)
{
	if (level<0) level=0;
	if (level>100) level=100;
	DisplayOut(0x2A, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x79, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x81, XBOX_LCD_COMMAND_MODE);
	DisplayOut((unsigned char)(level * 2.55f), XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x78, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x28, XBOX_LCD_COMMAND_MODE);
}
//************************************************************************************************************************
void CXboxLCD::DisplayInit()
{
	DisplayOut(0x2a, XBOX_LCD_COMMAND_MODE);  // function set (extended command set)
	DisplayOut(0x71, XBOX_LCD_COMMAND_MODE);  // function selection A, disable internal Vdd regualtor

	DisplayOut(0x00, XBOX_LCD_DATA_MODE);

	DisplayOut(0x28, XBOX_LCD_COMMAND_MODE);  // function set (fundamental command set)
	DisplayOut(US2066_DISPLAY_MODE | US2066_DISPLAY_MODE_OFF_FLAG, XBOX_LCD_COMMAND_MODE);

	//Set display clock devide ratio, oscillator freq
	DisplayOut(0x2a, XBOX_LCD_COMMAND_MODE); //RE=1
	DisplayOut(0x79, XBOX_LCD_COMMAND_MODE); //SD=1
	DisplayOut(0xd5, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x70, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x78, XBOX_LCD_COMMAND_MODE); //SD=0
	DisplayOut(US2066_EXTENDED_MDOE | US2066_EXTENDED_MDOE_5DOT_FLAG | US2066_EXTENDED_MDOE_INVERT_CURSOR_OFF_FLAG | US2066_EXTENDED_MDOE_3TO4ROWS_FLAG, 1);
	DisplayOut(0x06, XBOX_LCD_COMMAND_MODE);

	//CGROM/CGRAM Management
	DisplayOut(0x72, XBOX_LCD_COMMAND_MODE);

	DisplayOut(0x00, XBOX_LCD_DATA_MODE);    //ROM A
	
	//Set OLED Characterization
	DisplayOut(0x2a, XBOX_LCD_COMMAND_MODE); //RE=1
	DisplayOut(0x79, XBOX_LCD_COMMAND_MODE); //SD=1
	
	//Set SEG pins Hardware configuration
	DisplayOut(0xda, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x10, XBOX_LCD_COMMAND_MODE);

	DisplayOut(0xdc, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x00, XBOX_LCD_COMMAND_MODE);

	//Set contrast control
	DisplayOut(0x81, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0x7f, XBOX_LCD_COMMAND_MODE);

	//Set precharge period
	DisplayOut(0xd9, XBOX_LCD_COMMAND_MODE);
	DisplayOut(0xf1, XBOX_LCD_COMMAND_MODE);

	//Set VCOMH Deselect level
	DisplayOut(0xdb, XBOX_LCD_COMMAND_MODE); 
	DisplayOut(0x40, XBOX_LCD_COMMAND_MODE);

	//Exiting Set OLED Characterization
	DisplayOut(0x78, XBOX_LCD_COMMAND_MODE); //SD=0
	DisplayOut(0x28, XBOX_LCD_COMMAND_MODE); //RE=0, IS=0

	//Clear display
	DisplayOut(0x01, XBOX_LCD_COMMAND_MODE);

	//Set DDRAM Address
	DisplayOut(0x80, XBOX_LCD_COMMAND_MODE);

	DisplayOut(US2066_DISPLAY_MODE | US2066_DISPLAY_MODE_ON_FLAG, XBOX_LCD_COMMAND_MODE);

	SetContrast(m_iContrast);
}

//************************************************************************************************************************
void CXboxLCD::Process()
{
  int iOldLight=-1;
  int iOldContrast=-1;


  m_iColumns = g_advancedSettings.m_lcdColumns;
  m_iRows    = g_advancedSettings.m_lcdRows;
  m_iRow1adr = g_advancedSettings.m_lcdAddress1;
  m_iRow2adr = g_advancedSettings.m_lcdAddress2;
  m_iRow3adr = g_advancedSettings.m_lcdAddress3;
  m_iRow4adr = g_advancedSettings.m_lcdAddress4;
  m_iBackLight= g_guiSettings.GetInt("lcd.backlight");
  m_iContrast = g_guiSettings.GetInt("lcd.contrast");
  if (m_iRows >= MAX_ROWS) m_iRows=MAX_ROWS-1;

  DisplayInit();
  while (!m_bStop)
  {
    Sleep(SCROLL_SPEED_IN_MSEC);
    if (m_iBackLight != iOldLight)
    {
      // backlight setting changed
      iOldLight=m_iBackLight;
      DisplaySetBacklight(m_iBackLight);
    }
    if (m_iContrast != iOldContrast)
    {
      // contrast setting changed
      iOldContrast=m_iContrast;
      DisplaySetContrast(m_iContrast);
    }
    DisplayBuildCustomChars();
    for (int iLine=0; iLine < (int)m_iRows; ++iLine)
    {
      if (m_bUpdate[iLine])
      {
        CStdString strTmp=m_strLine[iLine];
        if (strTmp.size() > m_iColumns)
        {
          strTmp=m_strLine[iLine].Left(m_iColumns);
        }
        m_iPos[iLine]=0;
        DisplaySetPos(0,iLine);
        DisplayWriteFixtext(strTmp.c_str());
        m_bUpdate[iLine]=false;
        m_dwSleep[iLine]=GetTickCount();
      }
      else if ( (GetTickCount()-m_dwSleep[iLine]) > 1000)
      {
        int iSize=m_strLine[iLine].size();
        if (iSize > (int)m_iColumns)
        {
          //scroll line
          CStdString strRow=m_strLine[iLine]+"   -   ";
          int iSize=strRow.size();
          m_iPos[iLine]++;
          if (m_iPos[iLine]>=iSize) m_iPos[iLine]=0;
          int iPos=m_iPos[iLine];
          CStdString strLine="";
          for (int iCol=0; iCol < (int)m_iColumns;++iCol)
          {
            strLine +=strRow.GetAt(iPos);
            iPos++;
            if (iPos >= iSize) iPos=0;
          }
          DisplaySetPos(0,iLine);
          DisplayWriteFixtext(strLine.c_str());
        }
      }
    }
  }
  for (int i=0; i < (int)m_iRows; i++)
  {
    DisplayClearChars(0,i,m_iColumns);
  }
  DisplayOut(US2066_DISPLAY_MODE | US2066_DISPLAY_MODE_OFF_FLAG, XBOX_LCD_COMMAND_MODE);
  //m_xenium.HideDisplay();
}
