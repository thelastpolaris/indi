/////////////////////////////////////////////////////////////
//
// ApnCamData_S101401107.cpp:  Implementation file for the CApnCamData_S101401107 class.
//
// Copyright (c) 2003-2007 Apogee Instruments, Inc.
//
/////////////////////////////////////////////////////////////

#include "ApnCamData_S101401107.h"

#include <stdlib.h>
#include <malloc.h>
#include <string.h>


/////////////////////////////////////////////////////////////
// Construction/Destruction
/////////////////////////////////////////////////////////////


CApnCamData_S101401107::CApnCamData_S101401107()
{
}


CApnCamData_S101401107::~CApnCamData_S101401107()
{
}


void CApnCamData_S101401107::Initialize()
{
	strcpy( m_Sensor, "S101401107" );
	strcpy( m_CameraModel, "1107" );
	m_CameraId = 40;
	m_InterlineCCD = false;
	m_SupportsSerialA = true;
	m_SupportsSerialB = true;
	m_SensorTypeCCD = true;
	m_TotalColumns = 2068;
	m_ImagingColumns = 2048;
	m_ClampColumns = 10;
	m_PreRoiSkipColumns = 0;
	m_PostRoiSkipColumns = 0;
	m_OverscanColumns = 10;
	m_TotalRows = 128;
	m_ImagingRows = 122;
	m_UnderscanRows = 4;
	m_OverscanRows = 2;
	m_VFlushBinning = 4;
	m_EnableSingleRowOffset = false;
	m_RowOffsetBinning = 1;
	m_HFlushDisable = false;
	m_ShutterCloseDelay = 50;
	m_PixelSizeX = 6.8;
	m_PixelSizeY = 6.8;
	m_Color = false;
	m_ReportedGainSixteenBit = 1;
	m_MinSuggestedExpTime = 10.0;
	m_CoolingSupported = true;
	m_RegulatedCoolingSupported = true;
	m_TempSetPoint = -20.0;
	m_TempRampRateOne = 700;
	m_TempRampRateTwo = 4000;
	m_TempBackoffPoint = 2.0;
	m_PrimaryADType = ApnAdType_Alta_Sixteen;
	m_AlternativeADType = ApnAdType_Alta_Twelve;
	m_DefaultGainLeft = 100;
	m_DefaultOffsetLeft = 100;
	m_DefaultGainRight = -1;
	m_DefaultOffsetRight = -1;
	m_DefaultRVoltage = 1000;
	m_DefaultSpeed = 0xffff;
	m_DefaultDataReduction = false;

	set_vpattern();

	set_hpattern_clamp_sixteen();
	set_hpattern_skip_sixteen();
	set_hpattern_roi_sixteen();

	set_hpattern_clamp_twelve();
	set_hpattern_skip_twelve();
	set_hpattern_roi_twelve();
}


void CApnCamData_S101401107::set_vpattern()
{
	const unsigned short Mask = 0x6;
	const unsigned short NumElements = 61;
	unsigned short Pattern[NumElements] = 
	{
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0002, 0x0002, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 
		0x0002
	};

	m_VerticalPattern.Mask = Mask;
	m_VerticalPattern.NumElements = NumElements;
	m_VerticalPattern.PatternData = 
		(unsigned short *)malloc(NumElements * sizeof(unsigned short));

	for ( int i=0; i<NumElements; i++ )
	{
		m_VerticalPattern.PatternData[i] = Pattern[i];
	}
}


void CApnCamData_S101401107::set_hpattern_skip_sixteen()
{
	const unsigned short Mask = 0x2;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 99;
	const unsigned short SigNumElements = 100;

	unsigned short RefPatternData[RefNumElements] = 
	{
		0x0064, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 
		0x0068, 0x00E8, 0x00C8, 0x10C8, 0x10C8, 0x10C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8
	};

	unsigned short SigPatternData[SigNumElements] = 
	{
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0144, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0504, 0x0004, 0x0005, 0x0004, 
	};

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0002
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0048, 0x0044
	} };

	set_hpattern(	&m_SkipPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern_clamp_sixteen()
{
	const unsigned short Mask = 0x2;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 99;
	const unsigned short SigNumElements = 100;

	unsigned short RefPatternData[RefNumElements] = 
	{
		0x0064, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 
		0x0068, 0x00E8, 0x00C8, 0x10C8, 0x10C8, 0x10C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8
	};

	unsigned short SigPatternData[SigNumElements] = 
	{
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0144, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0504, 0x0004, 0x0005, 0x0004, 
	};

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0002
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0048, 0x0044
	} };

	set_hpattern(	&m_ClampPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern_roi_sixteen()
{
	const unsigned short Mask = 0x2;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 99;
	const unsigned short SigNumElements = 100;

	unsigned short RefPatternData[RefNumElements] = 
	{
		0x0064, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 0x006A, 
		0x006A, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 0x0068, 
		0x0068, 0x00E8, 0x00C8, 0x10C8, 0x10C8, 0x10C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 
		0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8, 0x00C8
	};

	unsigned short SigPatternData[SigNumElements] = 
	{
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 
		0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0044, 0x0144, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 
		0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0104, 0x0504, 0x8004, 0x8005, 0x0004, 
	};

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0002
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0048, 0x0044
	} };

	set_hpattern(	&m_RoiPatternSixteen,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern_skip_twelve()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000B
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0000, 0x020A, 0x0008, 0x0008, 0x0008, 0x2008, 0x0004, 0x0004, 0x0004, 0x0005, 
		0x4004
	} };

	set_hpattern(	&m_SkipPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern_clamp_twelve()
{
	const unsigned short Mask = 0x0;
	const unsigned short BinningLimit = 1;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x000C
	};

	unsigned short BinPatternData[1][256] = {
	{
		0x0800, 0x0A0A, 0x0808, 0x0808, 0x0808, 0x0808, 0x0804, 0x2804, 0x0804, 0x0804, 
		0x0805, 0x4804
	} };

	set_hpattern(	&m_ClampPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern_roi_twelve()
{
	const unsigned short Mask = 0x6802;
	const unsigned short BinningLimit = 10;
	const unsigned short RefNumElements = 0;
	const unsigned short SigNumElements = 0;

	unsigned short *RefPatternData = NULL;

	unsigned short *SigPatternData = NULL;

	unsigned short BinNumElements[APN_MAX_HBINNING] = 
	{
		0x0010, 0x0011, 0x0019, 0x0021, 0x0029, 0x0031, 0x0039, 0x0041, 0x0049, 0x0051
	};

	unsigned short BinPatternData[10][256] = {
	{
		0x0000, 0x000A, 0x020A, 0x0208, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 
		0x2004, 0x0004, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x8005, 
		0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x8005, 0xC004
	},
	{
		0x0004, 0x020A, 0x0008, 0x0008, 0x0008, 0x2004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 
		0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 
		0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 
		0x0008, 0x0004, 0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 
		0x0004, 0x0004, 0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0004, 0x0004, 0x8005, 
		0xC004
	} };

	set_hpattern(	&m_RoiPatternTwelve,
					Mask,
					BinningLimit,
					RefNumElements,
					SigNumElements,
					BinNumElements,
					RefPatternData,
					SigPatternData,
					BinPatternData );
}


void CApnCamData_S101401107::set_hpattern(	APN_HPATTERN_FILE	*Pattern,
											unsigned short	Mask,
											unsigned short	BinningLimit,
											unsigned short	RefNumElements,
											unsigned short	SigNumElements,
											unsigned short	BinNumElements[],
											unsigned short	RefPatternData[],
											unsigned short	SigPatternData[],
											unsigned short	BinPatternData[][APN_MAX_PATTERN_ENTRIES] )
{
	int i, j;

	Pattern->Mask = Mask;
	Pattern->BinningLimit = BinningLimit;
	Pattern->RefNumElements = RefNumElements;
	Pattern->SigNumElements = SigNumElements;

	if ( RefNumElements > 0 )
	{
		Pattern->RefPatternData = 
			(unsigned short *)malloc(RefNumElements * sizeof(unsigned short));

		for ( i=0; i<RefNumElements; i++ )
		{
			Pattern->RefPatternData[i] = RefPatternData[i];
		}
	}

	if ( SigNumElements > 0 )
	{
		Pattern->SigPatternData = 
			(unsigned short *)malloc(SigNumElements * sizeof(unsigned short));

		for ( i=0; i<SigNumElements; i++ )
		{
			Pattern->SigPatternData[i] = SigPatternData[i];
		}
	}

	if ( BinningLimit > 0 )
	{
		for ( i=0; i<BinningLimit; i++ )
		{
			Pattern->BinNumElements[i] = BinNumElements[i];

			Pattern->BinPatternData[i] = 
				(unsigned short *)malloc(BinNumElements[i] * sizeof(unsigned short));

			for ( j=0; j<BinNumElements[i]; j++ )
			{
				Pattern->BinPatternData[i][j] = BinPatternData[i][j];
			}
		}
	}
}