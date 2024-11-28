
#include "modxolcd.h"
#include "conio.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "undocumented.h"

#define MODXO_SCROLL_SPEED_IN_MSEC 250
#define MODXO_LCD_COMMAND_MODE 0x80
#define MODXO_LCD_DATA_MODE 0x40

#define MODXO_REGISTER_LCD_DATA_MODE 0x00
#define MODXO_REGISTER_LCD_DATA_PORT 0xDEA8
#define MODXO_REGISTER_LCD_COMMAND_MODE 0x01
#define MODXO_REGISTER_LCD_COMMAND_PORT 0xDEA9

#define MODXO_LCD_SPI 0x00
#define MODXO_LCD_I2C 0x01
#define MODXO_LCD_REMOVE_I2C_PREFIX 2
#define MODXO_LCD_SET_I2C_PREFIX 3

//*************************************************************************************************************
CModxoLCD::CModxoLCD()
{
  m_iActualpos=0;
  m_iRows    = 4;
  m_iColumns = 20;        // display rows each line
  m_iBackLight=32;
  m_iContrast=50; 
  m_iProtocol=0;
  m_iI2CAddress=0;

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
CModxoLCD::~CModxoLCD()
{
}

//*************************************************************************************************************
void CModxoLCD::Initialize()
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
void CModxoLCD::SetBackLight(int iLight)
{
  m_iBackLight=iLight;
}
void CModxoLCD::SetContrast(int iContrast)
{
  m_iContrast=iContrast;
}

//*************************************************************************************************************
void CModxoLCD::Stop()
{
  if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_NONE) return;
  StopThread();
}

//*************************************************************************************************************
void CModxoLCD::SetLine(int iLine, const CStdString& strLine)
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
void CModxoLCD::wait_us(unsigned int value) 
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
void CModxoLCD::DisplayOut(unsigned char data, unsigned char command) 
{
	unsigned short port = (command == MODXO_REGISTER_LCD_COMMAND_MODE) ? MODXO_REGISTER_LCD_COMMAND_PORT : MODXO_REGISTER_LCD_DATA_PORT;

	__asm {
		mov dx, port
		mov al, data
		out dx, al
	}
}

//************************************************************************************************************************
//DisplayBuildCustomChars: load customized characters to character ram of display, resets cursor to pos 0
//************************************************************************************************************************
void CModxoLCD::DisplayBuildCustomChars() 
{

}


//************************************************************************************************************************
// DisplaySetPos: sets cursor position
// Input: (row position, line number from 0 to 3)
//************************************************************************************************************************
void CModxoLCD::DisplaySetPos(unsigned char pos, unsigned char line) 
{
	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		DisplayOut(17, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(pos, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(line, MODXO_REGISTER_LCD_DATA_MODE);
	}
	else
	{
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_COMMAND_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		if (g_guiSettings.GetInt("lcd.type") == LCD_TYPE_LCD_KS0073)
		{
			int row_offsets[] = { 0x00, 0x20, 0x40, 0x60 };
			DisplayOut(0x80 | row_offsets[line] | pos, MODXO_REGISTER_LCD_DATA_MODE);
		}
		else
		{
			int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
			DisplayOut(0x80 | row_offsets[line] | pos, MODXO_REGISTER_LCD_DATA_MODE);
		}
	}
}

//************************************************************************************************************************
// DisplayWriteFixText: write a fixed text to actual cursor position
// Input: ("fixed text like")
//************************************************************************************************************************
void CModxoLCD::DisplayWriteFixtext(const char *textstring)
{ 
	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		unsigned char  c;
		while (c = *textstring++) {
			DisplayOut(c, MODXO_REGISTER_LCD_DATA_MODE);
		}
	}
	else
	{
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_DATA_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		unsigned char  c;
		while (c = *textstring++) {
			DisplayOut(c, MODXO_REGISTER_LCD_DATA_MODE);
		}
	}
} 


//************************************************************************************************************************
// DisplayWriteString: write a string to acutal cursor position 
// Input: (pointer to a 0x00 terminated string)
//************************************************************************************************************************

void CModxoLCD::DisplayWriteString(char *pointer) 
{
	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		/* display a normal 0x00 terminated string on the LCD display */
		unsigned char c;
		do {
			c = *pointer;
			if (c == 0x00)
			break;

			DisplayOut(c, MODXO_REGISTER_LCD_DATA_MODE);
			*pointer++;
		} while(1);
	}
	else
	{
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_DATA_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		/* display a normal 0x00 terminated string on the LCD display */
		unsigned char c;
		do {
			c = *pointer;
			if (c == 0x00)
			break;

			DisplayOut(c, MODXO_REGISTER_LCD_DATA_MODE);
			*pointer++;
		} while(1);
	}
}		


//************************************************************************************************************************
// DisplayClearChars:  clears a number of chars in a line and resets cursor position to it's startposition
// Input: (Startposition of clear in row, row number, number of chars to clear)
//************************************************************************************************************************
void CModxoLCD::DisplayClearChars(unsigned char startpos , unsigned char line, unsigned char lenght) 
{
	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		int i;

		DisplaySetPos(startpos,line);
		for (i=0;i<lenght; i++){
			DisplayOut(0x20, MODXO_REGISTER_LCD_DATA_MODE);
		}
		DisplaySetPos(startpos,line);
	}
	else
	{
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_DATA_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		int i;

		DisplaySetPos(startpos,line);
		for (i=0;i<lenght; i++){
			DisplayOut(0x20, MODXO_REGISTER_LCD_DATA_MODE);
		}
		DisplaySetPos(startpos,line);
	}
}


//************************************************************************************************************************
// DisplayProgressBar: shows a grafic bar staring at actual cursor position
// Input: (percent of bar to display, lenght of whole bar in chars when 100 %)
//************************************************************************************************************************
void CModxoLCD::DisplayProgressBar(unsigned char percent, unsigned char charcnt) 
{

}
//************************************************************************************************************************
//Set brightness level 
//************************************************************************************************************************
void CModxoLCD::DisplaySetBacklight(unsigned char level) 
{
	if (level<0) level=0;
	if (level>100) level=100;

	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		DisplayOut(14, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(level, MODXO_REGISTER_LCD_DATA_MODE);
	}
}
//************************************************************************************************************************
//Set Contrast level
//************************************************************************************************************************
void CModxoLCD::DisplaySetContrast(unsigned char level)
{
	if (level<0) level=0;
	if (level>100) level=100;

	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		DisplayOut(15, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(level, MODXO_REGISTER_LCD_DATA_MODE);
	}
	else
	{
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_COMMAND_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(0x2A, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x79, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x81, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut((unsigned char)(level * 2.55f), MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x78, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x28, MODXO_REGISTER_LCD_DATA_MODE);
	}
}
//************************************************************************************************************************
void CModxoLCD::DisplayInit()
{
	if (m_iProtocol == PROTOCOL_SPI2PAR)
	{
		//Spi
		DisplayOut(MODXO_LCD_SPI, MODXO_REGISTER_LCD_COMMAND_MODE); 

		// show display
		DisplayOut(3, MODXO_REGISTER_LCD_DATA_MODE); 

		// hide cursor
		DisplayOut(4, MODXO_REGISTER_LCD_DATA_MODE);

		// scroll off
		DisplayOut(20, MODXO_REGISTER_LCD_DATA_MODE); 

		// wrap off
		DisplayOut(24, MODXO_REGISTER_LCD_DATA_MODE);

		lcdSetBacklight(backlight);
	}
	else
	{
		int i2c_addresses[] = { 0x27, 0x3c, 0x3d, 0x3f };
		DisplayOut(MODXO_LCD_I2C, MODXO_REGISTER_LCD_COMMAND_MODE); 
		DisplayOut(i2c_addresses[m_iI2CAddress], MODXO_REGISTER_LCD_COMMAND_MODE); 
		
		//Set I2C Command Mode
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_COMMAND_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		DisplayOut(0x2a, MODXO_REGISTER_LCD_DATA_MODE);  // function set (extended command set)
		DisplayOut(0x71, MODXO_REGISTER_LCD_DATA_MODE);  // function selection A, disable internal Vdd regualtor

		//Set I2C Data Mode
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_DATA_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		DisplayOut(0x00, MODXO_REGISTER_LCD_DATA_MODE);

		//Set I2C Command Mode
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_COMMAND_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		DisplayOut(0x28, MODXO_REGISTER_LCD_DATA_MODE);  // function set (fundamental command set)
		DisplayOut(0x08, MODXO_REGISTER_LCD_DATA_MODE);

		//Set display clock devide ratio, oscillator freq
		DisplayOut(0x2a, MODXO_REGISTER_LCD_DATA_MODE); //RE=1
		DisplayOut(0x79, MODXO_REGISTER_LCD_DATA_MODE); //SD=1
		DisplayOut(0xd5, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x70, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x78, MODXO_REGISTER_LCD_DATA_MODE); //SD=0
		DisplayOut(0x08 | 0x01, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x06, MODXO_REGISTER_LCD_DATA_MODE);

		//CGROM/CGRAM Management
		DisplayOut(0x72, MODXO_REGISTER_LCD_DATA_MODE);

		//Set I2C Data Mode
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_DATA_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);

		DisplayOut(0x00, MODXO_REGISTER_LCD_DATA_MODE);    //ROM A

		//Set I2C Command Mode
		DisplayOut(MODXO_LCD_SET_I2C_PREFIX, MODXO_REGISTER_LCD_COMMAND_MODE);
		DisplayOut(MODXO_LCD_COMMAND_MODE, MODXO_REGISTER_LCD_COMMAND_MODE);
		
		//Set OLED Characterization
		DisplayOut(0x2a, MODXO_REGISTER_LCD_DATA_MODE); //RE=1
		DisplayOut(0x79, MODXO_REGISTER_LCD_DATA_MODE); //SD=1
		
		//Set SEG pins Hardware configuration
		DisplayOut(0xda, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x10, MODXO_REGISTER_LCD_DATA_MODE);

		DisplayOut(0xdc, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x00, MODXO_REGISTER_LCD_DATA_MODE);

		//Set contrast control
		DisplayOut(0x81, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0x7f, MODXO_REGISTER_LCD_DATA_MODE);

		//Set precharge period
		DisplayOut(0xd9, MODXO_REGISTER_LCD_DATA_MODE);
		DisplayOut(0xf1, MODXO_REGISTER_LCD_DATA_MODE);

		//Set VCOMH Deselect level
		DisplayOut(0xdb, MODXO_REGISTER_LCD_DATA_MODE); 
		DisplayOut(0x40, MODXO_REGISTER_LCD_DATA_MODE);

		//Exiting Set OLED Characterization
		DisplayOut(0x78, MODXO_REGISTER_LCD_DATA_MODE); //SD=0
		DisplayOut(0x28, MODXO_REGISTER_LCD_DATA_MODE); //RE=0, IS=0

		//Clear display
		DisplayOut(0x01, MODXO_REGISTER_LCD_DATA_MODE);

		//Set DDRAM Address
		DisplayOut(0x80, MODXO_REGISTER_LCD_DATA_MODE);

		DisplayOut(0x08 | 0x04, MODXO_REGISTER_LCD_DATA_MODE);
	}
	SetContrast(m_iContrast);
}

//************************************************************************************************************************
void CModxoLCD::Process()
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
  m_iProtocol = g_guiSettings.GetInt("lcd.protocol");
  m_iI2CAddress = g_guiSettings.GetInt("lcd.i2caddress");
  if (m_iRows >= MAX_ROWS) m_iRows=MAX_ROWS-1;

  DisplayInit();
  while (!m_bStop)
  {
    Sleep(MODXO_SCROLL_SPEED_IN_MSEC);
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
  DisplayOut(0x08, MODXO_LCD_COMMAND_MODE);
}
