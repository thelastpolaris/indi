/*******************************************************************************
 Copyright(c) 2010, 2011 Gerry Rozema, Jasem Mutlaq. All rights reserved.

 Rapid Guide support added by CloudMakers, s. r. o.
 Copyright(c) 2013 CloudMakers, s. r. o. All rights reserved.

 Star detection algorithm is based on PHD Guiding by Craig Stark
 Copyright (c) 2006-2010 Craig Stark. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indiccd.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <zlib.h>
#include <errno.h>
#include <dirent.h>

#include <libnova.h>
#include <fitsio.h>

#ifdef __linux__
#include "webcam/v4l2_record/stream_recorder.h"
#else
class StreamRecorder {};
#endif

const char *IMAGE_SETTINGS_TAB  = "Image Settings";
const char *IMAGE_INFO_TAB      = "Image Info";
const char *GUIDE_HEAD_TAB      = "Guider Head";
const char *GUIDE_CONTROL_TAB   = "Guider Control";
const char *RAPIDGUIDE_TAB      = "Rapid Guide";
const char *ASTROMETRY_TAB      = "Astrometry";

// Create dir recursively
static int _mkdir(const char *dir, mode_t mode)
{
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
        if(*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, mode) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    if (mkdir(tmp, mode) == -1 && errno != EEXIST)
        return -1;

    return 0;
}

CCDChip::CCDChip()
{
    SendCompressed=false;
    Interlaced=false;

    RawFrame= (uint8_t *) malloc(sizeof(uint8_t)); // Seed for realloc
    RawFrameSize=0;

    BPP = 8;
    BinX = BinY = 1;
    NAxis = 2;

    BinFrame = NULL;

    strncpy(imageExtention, "fits", MAXINDIBLOBFMT);

    FrameType=LIGHT_FRAME;
    lastRapidX = lastRapidY = -1;
}

CCDChip::~CCDChip()
{
    free(RawFrame);
    RawFrameSize=0;
    RawFrame=NULL;
    free (BinFrame);
}

void CCDChip::setFrameType(CCD_FRAME type)
{
    FrameType=type;
}

void CCDChip::setResolution(int x, int y)
{
    XRes = x;
    YRes = y;

    ImagePixelSizeN[0].value=x;
    ImagePixelSizeN[1].value=y;

    IDSetNumber(&ImagePixelSizeNP, NULL);

    ImageFrameN[FRAME_X].min = 0;
    ImageFrameN[FRAME_X].max = x-1;
    ImageFrameN[FRAME_Y].min = 0;
    ImageFrameN[FRAME_Y].max = y-1;

    ImageFrameN[FRAME_W].min = 1;
    ImageFrameN[FRAME_W].max = x;
    ImageFrameN[FRAME_H].max = 1;
    ImageFrameN[FRAME_H].max = y;
    IUUpdateMinMax(&ImageFrameNP);
}

void CCDChip::setFrame(int subx, int suby, int subw, int subh)
{
    SubX = subx;
    SubY = suby;
    SubW = subw;
    SubH = subh;

    ImageFrameN[FRAME_X].value = SubX;
    ImageFrameN[FRAME_Y].value = SubY;
    ImageFrameN[FRAME_W].value = SubW;
    ImageFrameN[FRAME_H].value = SubH;

    IDSetNumber(&ImageFrameNP, NULL);
}

void CCDChip::setBin(int hor, int ver)
{
    BinX = hor;
    BinY = ver;

    ImageBinN[BIN_W].value = BinX;
    ImageBinN[BIN_H].value = BinY;

    IDSetNumber(&ImageBinNP, NULL);
}


void CCDChip::setMinMaxStep(const char *property, const char *element, double min, double max, double step, bool sendToClient)
{
    INumberVectorProperty *nvp = NULL;

    if (!strcmp(property, ImageExposureNP.name))
        nvp = &ImageExposureNP;
    else if (!strcmp(property, ImageFrameNP.name))
        nvp = &ImageFrameNP;
    else if (!strcmp(property, ImageBinNP.name))
        nvp = &ImageBinNP;
    else if (!strcmp(property, ImagePixelSizeNP.name))
        nvp = &ImagePixelSizeNP;
    else if (!strcmp(property, RapidGuideDataNP.name))
        nvp = &RapidGuideDataNP;

    INumber *np = IUFindNumber(nvp, element);
    if (np)
    {
        np->min  = min;
        np->max  = max;
        np->step = step;

        if (sendToClient)
            IUUpdateMinMax(nvp);
    }

}

void CCDChip::setPixelSize(float x, float y)
{
    PixelSizex = x;
    PixelSizey = y;

    ImagePixelSizeN[2].value=x;
    ImagePixelSizeN[3].value=x;
    ImagePixelSizeN[4].value=y;

    IDSetNumber(&ImagePixelSizeNP, NULL);

}

void CCDChip::setBPP(int bbp)
{
    BPP = bbp;

    ImagePixelSizeN[5].value = BPP;

    IDSetNumber(&ImagePixelSizeNP, NULL);
}

void CCDChip::setFrameBufferSize(int nbuf, bool allocMem)
{
    if (nbuf == RawFrameSize)
        return;

    RawFrameSize = nbuf;

    if (allocMem == false)
        return;

    RawFrame = (uint8_t *) realloc(RawFrame, nbuf * sizeof(uint8_t));

    if (BinFrame)
        BinFrame = (uint8_t *) realloc(BinFrame, nbuf * sizeof(uint8_t));
}

void CCDChip::setExposureLeft(double duration)
{
    ImageExposureN[0].value = duration;

    IDSetNumber(&ImageExposureNP, NULL);
}

void CCDChip::setExposureDuration(double duration)
{
    exposureDuration = duration;
    gettimeofday(&startExposureTime,NULL);
}

const char *CCDChip::getFrameTypeName(CCD_FRAME fType)
{
    return FrameTypeS[fType].name;
}

const char * CCDChip::getExposureStartTime()
{
    static char ts[32];

    char iso8601[32];
    struct tm *tp;
    time_t t = (time_t) startExposureTime.tv_sec;
    int    u = startExposureTime.tv_usec / 1000.0;

    tp = gmtime (&t);
    strftime (iso8601, sizeof(iso8601), "%Y-%m-%dT%H:%M:%S", tp);
    snprintf(ts, 32, "%s.%03d", iso8601, u);
    return (ts);
}

void CCDChip::setInterlaced(bool intr)
{
    Interlaced = intr;
}

void CCDChip::setExposureFailed()
{
    ImageExposureNP.s = IPS_ALERT;
    IDSetNumber(&ImageExposureNP, NULL);
}

int CCDChip::getNAxis() const
{
    return NAxis;
}

void CCDChip::setNAxis(int value)
{
    NAxis = value;
}

void CCDChip::setImageExtension(const char *ext)
{
    strncpy(imageExtention, ext, MAXINDIBLOBFMT);
}

void CCDChip::binFrame()
{
    if (BinX == 1)
        return;

    // Jasem: Keep full frame shadow in memory to enhance performance and just swap frame pointers after operation is complete
    if (BinFrame == NULL)
        BinFrame = (uint8_t*) malloc(RawFrameSize);

    memset(BinFrame, 0, RawFrameSize);

    switch (getBPP())
    {
    case 8:
    {
        uint8_t *bin_buf = BinFrame;
        uint8_t val;
        for (int i=0; i < SubH; i+= BinX)
            for (int j=0; j < SubW; j+= BinX)
            {
                for (int k=0; k < BinX; k++)
                {
                    for (int l=0; l < BinX; l++)
                    {
                        val = *(RawFrame + j + (i+k) * SubW + l);
                        if (val + *bin_buf > UINT8_MAX)
                            *bin_buf = UINT8_MAX;
                        else
                            *bin_buf  += val;
                    }
                }
                bin_buf++;
            }
    }
    break;

    case 16:
    {
        uint16_t *bin_buf = (uint16_t*) BinFrame;
        uint16_t *RawFrame16 = (uint16_t*) RawFrame;
        uint16_t val;
        for (int i=0; i < SubH; i+= BinX)
            for (int j=0; j < SubW; j+= BinX)
            {
                for (int k=0; k < BinX; k++)
                {
                    for (int l=0; l < BinX; l++)
                    {
                        val = *(RawFrame16 + j + (i+k) * SubW + l);
                        if (val + *bin_buf > UINT16_MAX)
                            *bin_buf = UINT16_MAX;
                        else
                            *bin_buf  += val;
                    }
                }
                bin_buf++;
            }
    }
    break;

    default:
        return;

    }

    // Swap frame pointers
    uint8_t *rawFramePointer = RawFrame;
    RawFrame = BinFrame;
    // We just memset it next time we use it
    BinFrame = rawFramePointer;
}

INDI::CCD::CCD()
{
    //ctor
    capability = 0;

    InExposure=false;
    InGuideExposure=false;
    RapidGuideEnabled=false;
    GuiderRapidGuideEnabled=false;
    ValidCCDRotation=false;

    AutoLoop=false;
    SendImage=false;
    ShowMarker=false;
    GuiderAutoLoop=false;
    GuiderSendImage=false;
    GuiderShowMarker=false;

    ExposureTime = 0.0;
    GuiderExposureTime = 0.0;
    CurrentFilterSlot=-1;

    RA=-1000;
    Dec=-1000;
    Aperture=FocalLength=-1;

    streamer = NULL;
}

INDI::CCD::~CCD()
{
    delete (streamer);
}

void INDI::CCD::SetCCDCapability(uint32_t cap)
{
    capability = cap;

    if (HasGuideHead())
        setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);
    else
        setDriverInterface(getDriverInterface() & ~GUIDER_INTERFACE);

#ifdef __linux__
    if (HasStreaming() && streamer == NULL)
    {
        delete (streamer);
        streamer = new StreamRecorder(this);
        streamer->initProperties();
    }
#endif

}

bool INDI::CCD::initProperties()
{
    DefaultDevice::initProperties();   //  let the base class flesh in what it wants

    // CCD Temperature
    IUFillNumber(&TemperatureN[0], "CCD_TEMPERATURE_VALUE", "Temperature (C)", "%5.2f", -50.0, 50.0, 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "CCD_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    /**********************************************/
    /**************** Primary Chip ****************/
    /**********************************************/

    // Primary CCD Region-Of-Interest (ROI)
    IUFillNumber(&PrimaryCCD.ImageFrameN[0],"X","Left ","%4.0f",0,0.0,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[1],"Y","Top","%4.0f",0,0,0,0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,0.0,0,0.0);
    IUFillNumber(&PrimaryCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,0,0,0.0);
    IUFillNumberVector(&PrimaryCCD.ImageFrameNP,PrimaryCCD.ImageFrameN,4,getDeviceName(),"CCD_FRAME","Frame",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    // Primary CCD Frame Type
    IUFillSwitch(&PrimaryCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.FrameTypeSP,PrimaryCCD.FrameTypeS,4,getDeviceName(),"CCD_FRAME_TYPE","Frame Type",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    // Primary CCD Exposure
    IUFillNumber(&PrimaryCCD.ImageExposureN[0],"CCD_EXPOSURE_VALUE","Duration (s)","%5.2f",0.01,3600,1.0,1.0);
    IUFillNumberVector(&PrimaryCCD.ImageExposureNP,PrimaryCCD.ImageExposureN,1,getDeviceName(),"CCD_EXPOSURE","Expose",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    // Primary CCD Abort
    IUFillSwitch(&PrimaryCCD.AbortExposureS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.AbortExposureSP,PrimaryCCD.AbortExposureS,1,getDeviceName(),"CCD_ABORT_EXPOSURE","Expose Abort",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    // Primary CCD Binning
    IUFillNumber(&PrimaryCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&PrimaryCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(&PrimaryCCD.ImageBinNP,PrimaryCCD.ImageBinN,2,getDeviceName(),"CCD_BINNING","Binning",IMAGE_SETTINGS_TAB,IP_RW,60,IPS_IDLE);

    // Primary CCD Info
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,16000,0,0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,16000,0,0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,0);
    IUFillNumber(&PrimaryCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",8,64,0,0);
    IUFillNumberVector(&PrimaryCCD.ImagePixelSizeNP,PrimaryCCD.ImagePixelSizeN,6,getDeviceName(),"CCD_INFO","CCD Information",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    // Primary CCD Compression Options
    IUFillSwitch(&PrimaryCCD.CompressS[0],"CCD_COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&PrimaryCCD.CompressS[1],"CCD_RAW","Raw",ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.CompressSP,PrimaryCCD.CompressS,2,getDeviceName(),"CCD_COMPRESSION","Image",IMAGE_SETTINGS_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    PrimaryCCD.SendCompressed = false;

    // Primary CCD Chip Data Blob
    IUFillBLOB(&PrimaryCCD.FitsB,"CCD1","Image","");
    IUFillBLOBVector(&PrimaryCCD.FitsBP,&PrimaryCCD.FitsB,1,getDeviceName(),"CCD1","Image Data",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    // Bayer
    IUFillText(&BayerT[0],"CFA_OFFSET_X","X Offset","0");
    IUFillText(&BayerT[1],"CFA_OFFSET_Y","Y Offset","0");
    IUFillText(&BayerT[2],"CFA_TYPE","Filter",NULL);
    IUFillTextVector(&BayerTP,BayerT,3,getDeviceName(),"CCD_CFA","Bayer Info",IMAGE_INFO_TAB,IP_RW,60,IPS_IDLE);

    // Reset Frame Settings
    IUFillSwitch(&PrimaryCCD.ResetS[0], "RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.ResetSP, PrimaryCCD.ResetS, 1, getDeviceName(), "CCD_FRAME_RESET", "Frame Values", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    /**********************************************/
    /********* Primary Chip Rapid Guide  **********/
    /**********************************************/

    IUFillSwitch(&PrimaryCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSP, PrimaryCCD.RapidGuideS, 2, getDeviceName(), "CCD_RAPID_GUIDE", "Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&PrimaryCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&PrimaryCCD.RapidGuideSetupSP, PrimaryCCD.RapidGuideSetupS, 3, getDeviceName(), "CCD_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&PrimaryCCD.RapidGuideDataN[0],"GUIDESTAR_X","Guide star position X","%5.2f",0,1024,0,0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[1],"GUIDESTAR_Y","Guide star position Y","%5.2f",0,1024,0,0);
    IUFillNumber(&PrimaryCCD.RapidGuideDataN[2],"GUIDESTAR_FIT","Guide star fit","%5.2f",0,1024,0,0);
    IUFillNumberVector(&PrimaryCCD.RapidGuideDataNP,PrimaryCCD.RapidGuideDataN,3,getDeviceName(),"CCD_RAPID_GUIDE_DATA","Rapid Guide Data",RAPIDGUIDE_TAB,IP_RO,60,IPS_IDLE);

    /**********************************************/
    /***************** Guide Chip *****************/
    /**********************************************/

    IUFillNumber(&GuideCCD.ImageFrameN[0],"X","Left ","%4.0f",0,0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[1],"Y","Top","%4.0f",0,0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[2],"WIDTH","Width","%4.0f",0,0,0,0);
    IUFillNumber(&GuideCCD.ImageFrameN[3],"HEIGHT","Height","%4.0f",0,0,0,0);
    IUFillNumberVector(&GuideCCD.ImageFrameNP,GuideCCD.ImageFrameN,4,getDeviceName(),"GUIDER_FRAME","Frame",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageBinN[0],"HOR_BIN","X","%2.0f",1,4,1,1);
    IUFillNumber(&GuideCCD.ImageBinN[1],"VER_BIN","Y","%2.0f",1,4,1,1);
    IUFillNumberVector(&GuideCCD.ImageBinNP,GuideCCD.ImageBinN,2,getDeviceName(),"GUIDER_BINNING","Binning",GUIDE_HEAD_TAB,IP_RW,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImagePixelSizeN[0],"CCD_MAX_X","Resolution x","%4.0f",1,16000,0,0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[1],"CCD_MAX_Y","Resolution y","%4.0f",1,16000,0,0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[2],"CCD_PIXEL_SIZE","Pixel size (um)","%5.2f",1,40,0,0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[3],"CCD_PIXEL_SIZE_X","Pixel size X","%5.2f",1,40,0,0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[4],"CCD_PIXEL_SIZE_Y","Pixel size Y","%5.2f",1,40,0,0);
    IUFillNumber(&GuideCCD.ImagePixelSizeN[5],"CCD_BITSPERPIXEL","Bits per pixel","%3.0f",8,64,0,0);
    IUFillNumberVector(&GuideCCD.ImagePixelSizeNP,GuideCCD.ImagePixelSizeN,6,getDeviceName(),"GUIDER_INFO", "Guide Info",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.FrameTypeS[0],"FRAME_LIGHT","Light",ISS_ON);
    IUFillSwitch(&GuideCCD.FrameTypeS[1],"FRAME_BIAS","Bias",ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[2],"FRAME_DARK","Dark",ISS_OFF);
    IUFillSwitch(&GuideCCD.FrameTypeS[3],"FRAME_FLAT","Flat",ISS_OFF);
    IUFillSwitchVector(&GuideCCD.FrameTypeSP,GuideCCD.FrameTypeS,4,getDeviceName(),"GUIDER_FRAME_TYPE","Frame Type",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);

    IUFillNumber(&GuideCCD.ImageExposureN[0],"GUIDER_EXPOSURE_VALUE","Duration (s)","%5.2f",0.01,3600,1.0,1.0);
    IUFillNumberVector(&GuideCCD.ImageExposureNP,GuideCCD.ImageExposureN,1,getDeviceName(),"GUIDER_EXPOSURE","Guide Head",MAIN_CONTROL_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.AbortExposureS[0],"ABORT","Abort",ISS_OFF);
    IUFillSwitchVector(&GuideCCD.AbortExposureSP,GuideCCD.AbortExposureS,1,getDeviceName(),"GUIDER_ABORT_EXPOSURE","Guide Abort",MAIN_CONTROL_TAB,IP_RW,ISR_ATMOST1,60,IPS_IDLE);

    IUFillSwitch(&GuideCCD.CompressS[0],"GUIDER_COMPRESS","Compress",ISS_OFF);
    IUFillSwitch(&GuideCCD.CompressS[1],"GUIDER_RAW","Raw",ISS_ON);
    IUFillSwitchVector(&GuideCCD.CompressSP,GuideCCD.CompressS,2,getDeviceName(),"GUIDER_COMPRESSION","Image",GUIDE_HEAD_TAB,IP_RW,ISR_1OFMANY,60,IPS_IDLE);
    GuideCCD.SendCompressed = false;

    IUFillBLOB(&GuideCCD.FitsB,"CCD2","Guider Image","");
    IUFillBLOBVector(&GuideCCD.FitsBP,&GuideCCD.FitsB,1,getDeviceName(),"CCD2","Image Data",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);


    /**********************************************/
    /********* Guider Chip Rapid Guide  ***********/
    /**********************************************/

    IUFillSwitch(&GuideCCD.RapidGuideS[0], "ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideS[1], "DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&GuideCCD.RapidGuideSP, GuideCCD.RapidGuideS, 2, getDeviceName(), "GUIDER_RAPID_GUIDE", "Guider Head Rapid Guide", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&GuideCCD.RapidGuideSetupS[0], "AUTO_LOOP", "Auto loop", ISS_ON);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[1], "SEND_IMAGE", "Send image", ISS_OFF);
    IUFillSwitch(&GuideCCD.RapidGuideSetupS[2], "SHOW_MARKER", "Show marker", ISS_OFF);
    IUFillSwitchVector(&GuideCCD.RapidGuideSetupSP, GuideCCD.RapidGuideSetupS, 3, getDeviceName(), "GUIDER_RAPID_GUIDE_SETUP", "Rapid Guide Setup", RAPIDGUIDE_TAB, IP_RW, ISR_NOFMANY, 0, IPS_IDLE);

    IUFillNumber(&GuideCCD.RapidGuideDataN[0],"GUIDESTAR_X","Guide star position X","%5.2f",0,1024,0,0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[1],"GUIDESTAR_Y","Guide star position Y","%5.2f",0,1024,0,0);
    IUFillNumber(&GuideCCD.RapidGuideDataN[2],"GUIDESTAR_FIT","Guide star fit","%5.2f",0,1024,0,0);
    IUFillNumberVector(&GuideCCD.RapidGuideDataNP,GuideCCD.RapidGuideDataN,3,getDeviceName(),"GUIDER_RAPID_GUIDE_DATA","Rapid Guide Data",RAPIDGUIDE_TAB,IP_RO,60,IPS_IDLE);

    /**********************************************/
    /************** Upload Settings ***************/
    /**********************************************/

    // Upload Mode
    IUFillSwitch(&UploadS[0], "UPLOAD_CLIENT", "Client", ISS_ON);
    IUFillSwitch(&UploadS[1], "UPLOAD_LOCAL", "Local", ISS_OFF);
    IUFillSwitch(&UploadS[2], "UPLOAD_BOTH", "Both", ISS_OFF);
    IUFillSwitchVector(&UploadSP, UploadS, 3, getDeviceName(), "UPLOAD_MODE", "Upload", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Upload Settings
    IUFillText(&UploadSettingsT[0],"UPLOAD_DIR","Dir","");
    IUFillText(&UploadSettingsT[1],"UPLOAD_PREFIX","Prefix","IMAGE_XXX");
    IUFillTextVector(&UploadSettingsTP,UploadSettingsT,2,getDeviceName(),"UPLOAD_SETTINGS","Upload Settings",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Upload File Path
    IUFillText(&FileNameT[0],"FILE_PATH","Path","");
    IUFillTextVector(&FileNameTP,FileNameT,1,getDeviceName(),"CCD_FILE_PATH","Filename",IMAGE_INFO_TAB,IP_RO,60,IPS_IDLE);

    /**********************************************/
    /**************** Astrometry ******************/
    /**********************************************/

    // Solver Enable/Disable
    IUFillSwitch(&SolverS[0], "ASTROMETRY_SOLVER_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&SolverS[1], "ASTROMETRY_SOLVER_DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&SolverSP, SolverS, 2, getDeviceName(), "ASTROMETRY_SOLVER", "Solver", ASTROMETRY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Solver Settings
    IUFillText(&SolverSettingsT[ASTROMETRY_SETTINGS_BINARY], "ASTROMETRY_SETTINGS_BINARY", "Solver", "/usr/bin/solve-field");
    IUFillText(&SolverSettingsT[ASTROMETRY_SETTINGS_OPTIONS], "ASTROMETRY_SETTINGS_OPTIONS", "Options", "--no-verify --no-plots --no-fits2fits --resort --downsample 2 -O");
    IUFillTextVector(&SolverSettingsTP, SolverSettingsT, 2, getDeviceName(), "ASTROMETRY_SETTINGS", "Settings", ASTROMETRY_TAB, IP_WO, 0, IPS_IDLE);

    // Solver Results
    IUFillNumber(&SolverResultN[ASTROMETRY_RESULTS_PIXSCALE], "ASTROMETRY_RESULTS_PIXSCALE", "Pixscale (arcmin)", "%g", 0, 10000, 1, 0);
    IUFillNumber(&SolverResultN[ASTROMETRY_RESULTS_ORIENTATION], "ASTROMETRY_RESULTS_ORIENTATION", "Orientation (E of N) °", "%g", -360, 360, 1, 0);
    IUFillNumber(&SolverResultN[ASTROMETRY_RESULTS_RA], "ASTROMETRY_RESULTS_RA", "RA (J2000)", "%g", 0, 24, 1, 0);
    IUFillNumber(&SolverResultN[ASTROMETRY_RESULTS_DE], "ASTROMETRY_RESULTS_DE", "DE (J2000)", "%g", -90, 90, 1, 0);
    IUFillNumberVector(&SolverResultNP, SolverResultN, 4, getDeviceName(), "ASTROMETRY_RESULTS", "Results", ASTROMETRY_TAB, IP_RO, 0, IPS_IDLE);

    // WCS Enable/Disable
    IUFillSwitch(&WorldCoordS[0], "WCS_ENABLE", "Enable", ISS_OFF);
    IUFillSwitch(&WorldCoordS[1], "WCS_DISABLE", "Disable", ISS_ON);
    IUFillSwitchVector(&WorldCoordSP, WorldCoordS, 2, getDeviceName(), "WCS_CONTROL", "WCS", ASTROMETRY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&TelescopeTypeS[0], "TELESCOPE_PRIMARY", "Primary", ISS_ON);
    IUFillSwitch(&TelescopeTypeS[1], "TELESCOPE_GUIDE", "Guide", ISS_OFF);
    IUFillSwitchVector(&TelescopeTypeSP, TelescopeTypeS, 2, getDeviceName(), "TELESCOPE_TYPE", "Telescope", ASTROMETRY_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&CCDRotationN[0],"CCD_ROTATION_VALUE","Rotation","%g",-360,360,1,0);
    IUFillNumberVector(&CCDRotationNP,CCDRotationN,1,getDeviceName(),"CCD_ROTATION","CCD FOV", ASTROMETRY_TAB,IP_RW,60,IPS_IDLE);

    /**********************************************/
    /**************** Snooping ********************/
    /**********************************************/

    // Snooped Devices
    IUFillText(&ActiveDeviceT[0],"ACTIVE_TELESCOPE","Telescope","Telescope Simulator");
    IUFillText(&ActiveDeviceT[1],"ACTIVE_FOCUSER","Focuser","Focuser Simulator");
    IUFillText(&ActiveDeviceT[2],"ACTIVE_FILTER","Filter","CCD Simulator");
    IUFillTextVector(&ActiveDeviceTP,ActiveDeviceT,3,getDeviceName(),"ACTIVE_DEVICES","Snoop devices",OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    // Snooped RA/DEC Property
    IUFillNumber(&EqN[0],"RA","Ra (hh:mm:ss)","%010.6m",0,24,0,0);
    IUFillNumber(&EqN[1],"DEC","Dec (dd:mm:ss)","%010.6m",-90,90,0,0);
    IUFillNumberVector(&EqNP,EqN,2,ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD","EQ Coord","Main Control",IP_RW,60,IPS_IDLE);

    // Snoop properties of interest
    IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
    IDSnoopDevice(ActiveDeviceT[0].text,"TELESCOPE_INFO");
    IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_SLOT");
    IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_NAME");

    // Guider Interface
    initGuiderProperties(getDeviceName(), GUIDE_CONTROL_TAB);

    setDriverInterface(CCD_INTERFACE|GUIDER_INTERFACE);

    return true;
}

void INDI::CCD::ISGetProperties (const char *dev)
{
    DefaultDevice::ISGetProperties(dev);

    defineText(&ActiveDeviceTP);
    loadConfig(true, "ACTIVE_DEVICES");

    // Streamer
    #ifdef __linux__
    if (HasStreaming())
        streamer->ISGetProperties(dev);
    #endif

    return;
}

bool INDI::CCD::updateProperties()
{
    //IDLog("INDI::CCD UpdateProperties isConnected returns %d %d\n",isConnected(),Connected);
    if(isConnected())
    {
       defineNumber(&PrimaryCCD.ImageExposureNP);

       if (CanAbort())
            defineSwitch(&PrimaryCCD.AbortExposureSP);
       if (CanSubFrame() == false)
           PrimaryCCD.ImageFrameNP.p = IP_RO;

        defineNumber(&PrimaryCCD.ImageFrameNP);
       if (CanBin())
            defineNumber(&PrimaryCCD.ImageBinNP);

        if(HasGuideHead())
        {
            defineNumber(&GuideCCD.ImageExposureNP);
            if (CanAbort())
                defineSwitch(&GuideCCD.AbortExposureSP);
            if (CanSubFrame() == false)
                GuideCCD.ImageFrameNP.p = IP_RO;
            defineNumber(&GuideCCD.ImageFrameNP);
        }

        if (HasCooler())
            defineNumber(&TemperatureNP);

        defineNumber(&PrimaryCCD.ImagePixelSizeNP);
        if(HasGuideHead())
        {
            defineNumber(&GuideCCD.ImagePixelSizeNP);
            if (CanBin())
                defineNumber(&GuideCCD.ImageBinNP);
        }
        defineSwitch(&PrimaryCCD.CompressSP);
        defineBLOB(&PrimaryCCD.FitsBP);
        if(HasGuideHead())
        {
            defineSwitch(&GuideCCD.CompressSP);
            defineBLOB(&GuideCCD.FitsBP);
        }
        if(HasST4Port())
        {
            defineNumber(&GuideNSNP);
            defineNumber(&GuideWENP);
        }
        defineSwitch(&PrimaryCCD.FrameTypeSP);

        if (CanBin() || CanSubFrame())
            defineSwitch(&PrimaryCCD.ResetSP);

        if (HasGuideHead())
            defineSwitch(&GuideCCD.FrameTypeSP);

        if (HasBayer())
            defineText(&BayerTP);

        defineSwitch(&PrimaryCCD.RapidGuideSP);

        if (HasGuideHead())
          defineSwitch(&GuideCCD.RapidGuideSP);

        if (RapidGuideEnabled)
        {
          defineSwitch(&PrimaryCCD.RapidGuideSetupSP);
          defineNumber(&PrimaryCCD.RapidGuideDataNP);
        }
        if (GuiderRapidGuideEnabled)
        {
          defineSwitch(&GuideCCD.RapidGuideSetupSP);
          defineNumber(&GuideCCD.RapidGuideDataNP);
        }
        defineSwitch(&SolverSP);
        defineText(&SolverSettingsTP);
        defineSwitch(&WorldCoordSP);
        defineSwitch(&UploadSP);

        if (UploadSettingsT[0].text == NULL)
            IUSaveText(&UploadSettingsT[0], getenv("HOME"));
        defineText(&UploadSettingsTP);
    }
    else
    {
        deleteProperty(PrimaryCCD.ImageFrameNP.name);
        deleteProperty(PrimaryCCD.ImagePixelSizeNP.name);

        if (CanBin())
            deleteProperty(PrimaryCCD.ImageBinNP.name);

        deleteProperty(PrimaryCCD.ImageExposureNP.name);
        if (CanAbort())
            deleteProperty(PrimaryCCD.AbortExposureSP.name);
        deleteProperty(PrimaryCCD.FitsBP.name);
        deleteProperty(PrimaryCCD.CompressSP.name);
        deleteProperty(PrimaryCCD.RapidGuideSP.name);
        if (RapidGuideEnabled)
        {
          deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
          deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
        }
        if(HasGuideHead())
        {
            deleteProperty(GuideCCD.ImageExposureNP.name);
            if (CanAbort())
                deleteProperty(GuideCCD.AbortExposureSP.name);
                deleteProperty(GuideCCD.ImageFrameNP.name);
                deleteProperty(GuideCCD.ImagePixelSizeNP.name);

            deleteProperty(GuideCCD.FitsBP.name);
            if (CanBin())
                deleteProperty(GuideCCD.ImageBinNP.name);
            deleteProperty(GuideCCD.CompressSP.name);
            deleteProperty(GuideCCD.FrameTypeSP.name);
            deleteProperty(GuideCCD.RapidGuideSP.name);
            if (GuiderRapidGuideEnabled)
            {
              deleteProperty(GuideCCD.RapidGuideSetupSP.name);
              deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }
        }
        if (HasCooler())
            deleteProperty(TemperatureNP.name);
        if(HasST4Port())
        {
            deleteProperty(GuideNSNP.name);
            deleteProperty(GuideWENP.name);
        }
        deleteProperty(PrimaryCCD.FrameTypeSP.name);
        if (CanBin() || CanSubFrame())
            deleteProperty(PrimaryCCD.ResetSP.name);
        if (HasBayer())
            deleteProperty(BayerTP.name);
        if (SolverS[0].s == ISS_ON)
        {
            deleteProperty(SolverResultNP.name);
        }
        deleteProperty(SolverSP.name);
        deleteProperty(SolverSettingsTP.name);
        if (WorldCoordS[0].s == ISS_ON)
        {
            deleteProperty(TelescopeTypeSP.name);
            deleteProperty(CCDRotationNP.name);
        }
        deleteProperty(WorldCoordSP.name);
        deleteProperty(UploadSP.name);
        deleteProperty(UploadSettingsTP.name);
    }

    // Streamer
    #ifdef __linux__
    if (HasStreaming())
        streamer->updateProperties();
    #endif

    return true;
}

bool INDI::CCD::ISSnoopDevice (XMLEle *root)
{
    XMLEle *ep=NULL;
    const char *propName = findXMLAttValu(root, "name");

     if(IUSnoopNumber(root,&EqNP)==0)
     {
        float newra,newdec;
        newra=EqN[0].value;
        newdec=EqN[1].value;
        if((newra != RA)||(newdec != Dec))
        {
            //IDLog("RA %4.2f  Dec %4.2f Snooped RA %4.2f  Dec %4.2f\n",RA,Dec,newra,newdec);
            RA=newra;
            Dec=newdec;
        }
     }
     else if (!strcmp(propName, "TELESCOPE_INFO"))
     {
          for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
          {
              const char *name = findXMLAttValu(ep, "name");

              if (!strcmp(name, "TELESCOPE_APERTURE"))
              {
                  if (TelescopeTypeS[0].s == ISS_ON)
                      Aperture = atof(pcdataXMLEle(ep));
              }
              else if (!strcmp(name, "TELESCOPE_FOCAL_LENGTH"))
              {
                  if (TelescopeTypeS[0].s == ISS_ON)
                      FocalLength = atof(pcdataXMLEle(ep));
              }
              else if (!strcmp(name, "GUIDER_APERTURE"))
              {
                  if (TelescopeTypeS[1].s == ISS_ON)
                      Aperture = atof(pcdataXMLEle(ep));
              }
              else if (!strcmp(name, "GUIDER_FOCAL_LENGTH"))
              {
                  if (TelescopeTypeS[1].s == ISS_ON)
                      FocalLength = atof(pcdataXMLEle(ep));
              }
          }
     }
     else if (!strcmp(propName, "FILTER_NAME"))
     {
         FilterNames.clear();

         for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             FilterNames.push_back(pcdataXMLEle(ep));
     }
     else if (!strcmp(propName, "FILTER_SLOT"))
     {
         CurrentFilterSlot=-1;
         for (ep = nextXMLEle(root, 1) ; ep != NULL ; ep = nextXMLEle(root, 0))
             CurrentFilterSlot = atoi(pcdataXMLEle(ep));
     }

     return INDI::DefaultDevice::ISSnoopDevice(root);
 }

bool INDI::CCD::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(!strcmp(name,ActiveDeviceTP.name))
        {
            int rc;
            ActiveDeviceTP.s=IPS_OK;
            rc=IUUpdateText(&ActiveDeviceTP,texts,names,n);
            //  Update client display
            IDSetText(&ActiveDeviceTP,NULL);
            //saveConfig();

            // Update the property name!
            strncpy(EqNP.device, ActiveDeviceT[0].text, MAXINDIDEVICE);
            IDSnoopDevice(ActiveDeviceT[0].text,"EQUATORIAL_EOD_COORD");
            IDSnoopDevice(ActiveDeviceT[0].text,"TELESCOPE_INFO");
            IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_SLOT");
            IDSnoopDevice(ActiveDeviceT[2].text,"FILTER_NAME");

            // Tell children active devices was updated.
            activeDevicesUpdated();

            //  We processed this one, so, tell the world we did it
            return true;
        }

        if (!strcmp(name, BayerTP.name))
        {
            IUUpdateText(&BayerTP, texts, names, n);
            BayerTP.s = IPS_OK;
            IDSetText(&BayerTP, NULL);
            return true;
        }

        if (!strcmp(name, UploadSettingsTP.name))
        {
            IUUpdateText(&UploadSettingsTP, texts, names, n);
            UploadSettingsTP.s = IPS_OK;
            IDSetText(&UploadSettingsTP, NULL);
            return true;
        }

        if (!strcmp(name, SolverSettingsTP.name))
        {
            IUUpdateText(&SolverSettingsTP, texts, names, n);
            SolverSettingsTP.s = IPS_OK;
            IDSetText(&SolverSettingsTP, NULL);
            return true;
        }
    }

    // Streamer
    #ifdef __linux__
    if (HasStreaming())
        streamer->ISNewText(dev,name,texts,names,n);
    #endif

    return INDI::DefaultDevice::ISNewText(dev,name,texts,names,n);
}

bool INDI::CCD::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    //IDLog("INDI::CCD::ISNewNumber %s\n",name);
    if(strcmp(dev,getDeviceName())==0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if(strcmp(name,"CCD_EXPOSURE")==0)
        {
            if (PrimaryCCD.getFrameType() != CCDChip::BIAS_FRAME && values[0] <  PrimaryCCD.ImageExposureN[0].min || values[0] > PrimaryCCD.ImageExposureN[0].max)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Requested exposure value (%g) seconds out of bounds [%g,%g].", values[0], PrimaryCCD.ImageExposureN[0].min, PrimaryCCD.ImageExposureN[0].max);
                PrimaryCCD.ImageExposureNP.s=IPS_ALERT;
                IDSetNumber(&PrimaryCCD.ImageExposureNP,NULL);
                return false;
            }

            if (PrimaryCCD.getFrameType() == CCDChip::BIAS_FRAME)
                PrimaryCCD.ImageExposureN[0].value = ExposureTime = PrimaryCCD.ImageExposureN[0].min;
            else
                PrimaryCCD.ImageExposureN[0].value = ExposureTime = values[0];

            if (PrimaryCCD.ImageExposureNP.s==IPS_BUSY)
            {
                if (AbortExposure() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "Warning: Aborting exposure failed.");
            }

            if (StartExposure(ExposureTime))
               PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
            else
               PrimaryCCD.ImageExposureNP.s=IPS_ALERT;
            IDSetNumber(&PrimaryCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_EXPOSURE")==0)
        {
            if (GuideCCD.getFrameType() != CCDChip::BIAS_FRAME && values[0] <  GuideCCD.ImageExposureN[0].min || values[0] > GuideCCD.ImageExposureN[0].max)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Requested guide exposure value (%g) seconds out of bounds [%g,%g].", values[0], GuideCCD.ImageExposureN[0].min, GuideCCD.ImageExposureN[0].max);
                GuideCCD.ImageExposureNP.s=IPS_ALERT;
                IDSetNumber(&GuideCCD.ImageExposureNP,NULL);
                return false;
            }

            if (GuideCCD.getFrameType() == CCDChip::BIAS_FRAME)
                GuideCCD.ImageExposureN[0].value = GuiderExposureTime = GuideCCD.ImageExposureN[0].min;
            else
                GuideCCD.ImageExposureN[0].value = GuiderExposureTime = values[0];

            GuideCCD.ImageExposureNP.s=IPS_BUSY;
            if (StartGuideExposure(GuiderExposureTime))
               GuideCCD.ImageExposureNP.s=IPS_BUSY;
            else
               GuideCCD.ImageExposureNP.s=IPS_ALERT;
            IDSetNumber(&GuideCCD.ImageExposureNP,NULL);
            return true;
        }

        if(strcmp(name,"CCD_BINNING")==0)
        {
            //  We are being asked to set camera binning
            INumber *np = IUFindNumber(&PrimaryCCD.ImageBinNP, names[0]);
            if (np == NULL)
            {
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber (&PrimaryCCD.ImageBinNP, NULL);
                return false;
            }

            int binx,biny;
            if (!strcmp(np->name, "HOR_BIN"))
            {
                binx = values[0];
                biny = values[1];
            }
            else
            {
                binx = values[1];
                biny = values[0];
            }

            if (UpdateCCDBin(binx, biny))
            {
                IUUpdateNumber(&PrimaryCCD.ImageBinNP,values,names,n);
                PrimaryCCD.ImageBinNP.s=IPS_OK;

            }
            else
                PrimaryCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber (&PrimaryCCD.ImageBinNP, NULL);

            return true;

        }

        if(strcmp(name,"GUIDER_BINNING")==0)
        {
            //  We are being asked to set camera binning
            INumber *np = IUFindNumber(&GuideCCD.ImageBinNP, names[0]);
            if (np == NULL)
            {
                GuideCCD.ImageBinNP.s = IPS_ALERT;
                IDSetNumber (&GuideCCD.ImageBinNP, NULL);
                return false;
            }

            int binx,biny;
            if (!strcmp(np->name, "HOR_BIN"))
            {
                binx = values[0];
                biny = values[1];
            }
            else
            {
                binx = values[1];
                biny = values[0];
            }

            if (UpdateGuiderBin(binx, biny))
            {
                IUUpdateNumber(&GuideCCD.ImageBinNP,values,names,n);
                GuideCCD.ImageBinNP.s=IPS_OK;

            }
            else
                GuideCCD.ImageBinNP.s = IPS_ALERT;

            IDSetNumber (&GuideCCD.ImageBinNP, NULL);

            return true;


        }

        if(strcmp(name,"CCD_FRAME")==0)
        {
            //  We are being asked to set CCD Frame
            if (IUUpdateNumber(&PrimaryCCD.ImageFrameNP,values,names,n) < 0)
                return false;

            PrimaryCCD.ImageFrameNP.s=IPS_OK;

            DEBUGF(Logger::DBG_DEBUG, "Requested CCD Frame is (%3.0f,%3.0f) (%3.0f x %3.0f)", values[0],values[1],values[2],values[3]);

            if (UpdateCCDFrame(PrimaryCCD.ImageFrameN[0].value, PrimaryCCD.ImageFrameN[1].value, PrimaryCCD.ImageFrameN[2].value,
                               PrimaryCCD.ImageFrameN[3].value) == false)
                PrimaryCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&PrimaryCCD.ImageFrameNP, NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_FRAME")==0)
        {
            //  We are being asked to set guide frame
            if (IUUpdateNumber(&GuideCCD.ImageFrameNP,values,names,n) < 0)
                return false;

            GuideCCD.ImageFrameNP.s=IPS_OK;

            DEBUGF(Logger::DBG_DEBUG, "Requested Guide Frame is %4.0f,%4.0f %4.0f x %4.0f",
                  values[0],values[1],values[2],values[4]);

            if (UpdateGuiderFrame(GuideCCD.ImageFrameN[0].value, GuideCCD.ImageFrameN[1].value, GuideCCD.ImageFrameN[2].value,
                               GuideCCD.ImageFrameN[3].value) == false)
                GuideCCD.ImageFrameNP.s = IPS_ALERT;

            IDSetNumber(&GuideCCD.ImageFrameNP, NULL);

            return true;
        }

        if(strcmp(name,"CCD_GUIDESTAR")==0)
        {
            PrimaryCCD.RapidGuideDataNP.s=IPS_OK;
            IUUpdateNumber(&PrimaryCCD.RapidGuideDataNP,values,names,n);
            IDSetNumber(&PrimaryCCD.RapidGuideDataNP, NULL);
            return true;
        }

        if(strcmp(name,"GUIDER_GUIDESTAR")==0)
        {
            GuideCCD.RapidGuideDataNP.s=IPS_OK;
            IUUpdateNumber(&GuideCCD.RapidGuideDataNP,values,names,n);
            IDSetNumber(&GuideCCD.RapidGuideDataNP, NULL);
            return true;
        }

        if (!strcmp(name,GuideNSNP.name) || !strcmp(name,GuideWENP.name))
        {
            processGuiderProperties(name, values, names, n);
            return true;
        }

        // CCD TEMPERATURE:
        if(!strcmp(name, TemperatureNP.name))
        {

            if(values[0] < TemperatureN[0].min || values[0] > TemperatureN[0].max)
            {
                TemperatureNP.s = IPS_ALERT;
                DEBUGF(INDI::Logger::DBG_ERROR, "Error: Bad temperature value! Range is [%.1f, %.1f] [C].",
                        TemperatureN[0].min, TemperatureN[0].max);
                IDSetNumber(&TemperatureNP, NULL);
                return false;

            }

            int rc= SetTemperature(values[0]);

            if (rc == 0)
                TemperatureNP.s = IPS_BUSY;
            else if (rc == 1)
                TemperatureNP.s = IPS_OK;
            else
                TemperatureNP.s = IPS_ALERT;

            IDSetNumber(&TemperatureNP, NULL);
            return true;
        }

        // Primary CCD Info
        if (!strcmp(name, PrimaryCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&PrimaryCCD.ImagePixelSizeNP, values, names, n);
            PrimaryCCD.ImagePixelSizeNP.s = IPS_OK;
            SetCCDParams(PrimaryCCD.ImagePixelSizeNP.np[0].value, PrimaryCCD.ImagePixelSizeNP.np[1].value, PrimaryCCD.getBPP(), PrimaryCCD.ImagePixelSizeNP.np[2].value, PrimaryCCD.ImagePixelSizeNP.np[3].value);
            IDSetNumber(&PrimaryCCD.ImagePixelSizeNP, NULL);
            return true;
        }

        // Guide CCD Info
        if (!strcmp(name, GuideCCD.ImagePixelSizeNP.name))
        {
            IUUpdateNumber(&GuideCCD.ImagePixelSizeNP, values, names, n);
            GuideCCD.ImagePixelSizeNP.s = IPS_OK;
            SetGuiderParams(GuideCCD.ImagePixelSizeNP.np[0].value, GuideCCD.ImagePixelSizeNP.np[1].value, GuideCCD.getBPP(), GuideCCD.ImagePixelSizeNP.np[2].value, GuideCCD.ImagePixelSizeNP.np[3].value);
            IDSetNumber(&GuideCCD.ImagePixelSizeNP, NULL);
            return true;
        }

        // CCD Rotation
        if (!strcmp(name, CCDRotationNP.name))
        {
            IUUpdateNumber(&CCDRotationNP, values, names, n);
            CCDRotationNP.s = IPS_OK;
            IDSetNumber(&CCDRotationNP, NULL);
            ValidCCDRotation=true;

            DEBUGF(INDI::Logger::DBG_SESSION, "CCD FOV rotation updated to %g degrees.", CCDRotationN[0].value);

            return true;
        }
    }

    // Streamer
    #ifdef __linux__
    if (HasStreaming())
        streamer->ISNewNumber(dev,name,values,names,n);
    #endif

    return DefaultDevice::ISNewNumber(dev,name,values,names,n);
}

bool INDI::CCD::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (!strcmp(name, UploadSP.name))
        {
            int prevMode = IUFindOnSwitchIndex(&UploadSP);
            IUUpdateSwitch(&UploadSP, states, names, n);
            UploadSP.s = IPS_OK;
            IDSetSwitch(&UploadSP, NULL);

            if (UploadS[0].s == ISS_ON)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client only.");
                if (prevMode != 0)
                    deleteProperty(FileNameTP.name);
            }
            else if (UploadS[1].s == ISS_ON)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to local only.");
                defineText(&FileNameTP);
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Upload settings set to client and local.");
                defineText(&FileNameTP);
            }
            return true;
        }

        if (!strcmp(name, TelescopeTypeSP.name))
        {
            IUUpdateSwitch(&TelescopeTypeSP, states, names, n);
            TelescopeTypeSP.s = IPS_OK;
            IDSetSwitch(&TelescopeTypeSP, NULL);
            return true;
        }

        // Astrometry Enable/Disable
        if (!strcmp(name, SolverSP.name))
        {
            pthread_mutex_lock(&lock);

            IUUpdateSwitch(&SolverSP, states, names, n);
            SolverSP.s = IPS_OK;

            if (SolverS[0].s == ISS_ON)
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Astrometry solver is enabled.");
                defineNumber(&SolverResultNP);
            }
            else
            {
                DEBUG(INDI::Logger::DBG_SESSION, "Astrometry solver is disabled.");
                deleteProperty(SolverResultNP.name);
            }

            IDSetSwitch(&SolverSP, NULL);

            pthread_mutex_unlock(&lock);
            return true;
        }

        // WCS Enable/Disable
        if (!strcmp(name, WorldCoordSP.name))
        {
            IUUpdateSwitch(&WorldCoordSP, states, names, n);
            WorldCoordSP.s = IPS_OK;

            if (WorldCoordS[0].s == ISS_ON)
            {
                DEBUG(INDI::Logger::DBG_WARNING, "World Coordinate System is enabled. CCD rotation must be set either manually or by solving the image before proceeding to capture any frames, otherwise the WCS information may be invalid.");
                defineSwitch(&TelescopeTypeSP);
                defineNumber(&CCDRotationNP);
            }
            else
            {
                deleteProperty(TelescopeTypeSP.name);
                deleteProperty(CCDRotationNP.name);
            }

            ValidCCDRotation=false;
            IDSetSwitch(&WorldCoordSP, NULL);
        }

        // Primary Chip Frame Reset
        if(strcmp(name,PrimaryCCD.ResetSP.name)==0)
        {
          IUResetSwitch(&PrimaryCCD.ResetSP);
          PrimaryCCD.ResetSP.s = IPS_OK;
          if (CanBin())
              UpdateCCDBin(1,1);
          if (CanSubFrame())
              UpdateCCDFrame(0,0, PrimaryCCD.getXRes(), PrimaryCCD.getYRes());

          IDSetSwitch(&PrimaryCCD.ResetSP, NULL);
          return true;
        }

        // Primary Chip Abort Expsoure
        if(strcmp(name,PrimaryCCD.AbortExposureSP.name)==0)
        {
            IUResetSwitch(&PrimaryCCD.AbortExposureSP);

            if (AbortExposure())
            {
                PrimaryCCD.AbortExposureSP.s = IPS_OK;
                PrimaryCCD.ImageExposureNP.s = IPS_IDLE;
                PrimaryCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                PrimaryCCD.AbortExposureSP.s = IPS_ALERT;
                PrimaryCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&PrimaryCCD.AbortExposureSP, NULL);
            IDSetNumber(&PrimaryCCD.ImageExposureNP, NULL);

            return true;
        }

        // Guide Chip Abort Exposure
        if(strcmp(name,GuideCCD.AbortExposureSP.name)==0)
        {
            IUResetSwitch(&GuideCCD.AbortExposureSP);

            if (AbortGuideExposure())
            {
                GuideCCD.AbortExposureSP.s = IPS_OK;
                GuideCCD.ImageExposureNP.s = IPS_IDLE;
                GuideCCD.ImageExposureN[0].value = 0;
            }
            else
            {
                GuideCCD.AbortExposureSP.s = IPS_ALERT;
                GuideCCD.ImageExposureNP.s = IPS_ALERT;
            }

            IDSetSwitch(&GuideCCD.AbortExposureSP, NULL);
            IDSetNumber(&GuideCCD.ImageExposureNP, NULL);

            return true;
        }

        // Primary Chip Compression
        if(strcmp(name,PrimaryCCD.CompressSP.name)==0)
        {

            IUUpdateSwitch(&PrimaryCCD.CompressSP,states,names,n);
            PrimaryCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&PrimaryCCD.CompressSP,NULL);

            if(PrimaryCCD.CompressS[0].s==ISS_ON    )
            {
                PrimaryCCD.SendCompressed=true;
            } else
            {
                PrimaryCCD.SendCompressed=false;
            }
            return true;
        }

        // Guide Chip Compression
        if(strcmp(name,GuideCCD.CompressSP.name)==0)
        {

            IUUpdateSwitch(&GuideCCD.CompressSP,states,names,n);
            GuideCCD.CompressSP.s = IPS_OK;
            IDSetSwitch(&GuideCCD.CompressSP,NULL);

            if(GuideCCD.CompressS[0].s==ISS_ON    )
            {
                GuideCCD.SendCompressed=true;
            } else
            {
                GuideCCD.SendCompressed=false;
            }
            return true;
        }

        // Primary Chip Frame Type
        if(strcmp(name,PrimaryCCD.FrameTypeSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.FrameTypeSP,states,names,n);
            PrimaryCCD.FrameTypeSP.s=IPS_OK;
            if(PrimaryCCD.FrameTypeS[0].s==ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if(PrimaryCCD.FrameTypeS[1].s==ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (HasShutter() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if(PrimaryCCD.FrameTypeS[2].s==ISS_ON)
            {
                PrimaryCCD.setFrameType(CCDChip::DARK_FRAME);
                if (HasShutter() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if(PrimaryCCD.FrameTypeS[3].s==ISS_ON)
                PrimaryCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateCCDFrameType(PrimaryCCD.getFrameType()) == false)
                PrimaryCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&PrimaryCCD.FrameTypeSP,NULL);

            return true;
        }

        // Guide Chip Frame Type
        if(strcmp(name,GuideCCD.FrameTypeSP.name)==0)
        {
            //  Compression Update
            IUUpdateSwitch(&GuideCCD.FrameTypeSP,states,names,n);
            GuideCCD.FrameTypeSP.s=IPS_OK;
            if(GuideCCD.FrameTypeS[0].s==ISS_ON)
                GuideCCD.setFrameType(CCDChip::LIGHT_FRAME);
            else if(GuideCCD.FrameTypeS[1].s==ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::BIAS_FRAME);
                if (HasShutter() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a bias frame.");
            }
            else if(GuideCCD.FrameTypeS[2].s==ISS_ON)
            {
                GuideCCD.setFrameType(CCDChip::DARK_FRAME);
                if (HasShutter() == false)
                    DEBUG(INDI::Logger::DBG_WARNING, "The CCD does not have a shutter. Cover the camera in order to take a dark frame.");
            }
            else if(GuideCCD.FrameTypeS[3].s==ISS_ON)
                GuideCCD.setFrameType(CCDChip::FLAT_FRAME);

            if (UpdateGuiderFrameType(GuideCCD.getFrameType()) == false)
                GuideCCD.FrameTypeSP.s = IPS_ALERT;

            IDSetSwitch(&GuideCCD.FrameTypeSP,NULL);

            return true;
        }

        // Primary Chip Rapid Guide Enable/Disable
        if (strcmp(name, PrimaryCCD.RapidGuideSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSP, states, names, n);
            PrimaryCCD.RapidGuideSP.s=IPS_OK;
            RapidGuideEnabled=(PrimaryCCD.RapidGuideS[0].s==ISS_ON);

            if (RapidGuideEnabled) {
              defineSwitch(&PrimaryCCD.RapidGuideSetupSP);
              defineNumber(&PrimaryCCD.RapidGuideDataNP);
            }
            else {
              deleteProperty(PrimaryCCD.RapidGuideSetupSP.name);
              deleteProperty(PrimaryCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&PrimaryCCD.RapidGuideSP,NULL);
            return true;
        }

        // Guide Chip Rapid Guide Enable/Disable
        if (strcmp(name, GuideCCD.RapidGuideSP.name)==0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSP, states, names, n);
            GuideCCD.RapidGuideSP.s=IPS_OK;
            GuiderRapidGuideEnabled=(GuideCCD.RapidGuideS[0].s==ISS_ON);

            if (GuiderRapidGuideEnabled) {
              defineSwitch(&GuideCCD.RapidGuideSetupSP);
              defineNumber(&GuideCCD.RapidGuideDataNP);
            }
            else {
              deleteProperty(GuideCCD.RapidGuideSetupSP.name);
              deleteProperty(GuideCCD.RapidGuideDataNP.name);
            }

            IDSetSwitch(&GuideCCD.RapidGuideSP,NULL);
            return true;
        }

        // Primary CCD Rapid Guide Setup
        if (strcmp(name, PrimaryCCD.RapidGuideSetupSP.name)==0)
        {
            IUUpdateSwitch(&PrimaryCCD.RapidGuideSetupSP, states, names, n);
            PrimaryCCD.RapidGuideSetupSP.s=IPS_OK;

            AutoLoop=(PrimaryCCD.RapidGuideSetupS[0].s==ISS_ON);
            SendImage=(PrimaryCCD.RapidGuideSetupS[1].s==ISS_ON);
            ShowMarker=(PrimaryCCD.RapidGuideSetupS[2].s==ISS_ON);

            IDSetSwitch(&PrimaryCCD.RapidGuideSetupSP,NULL);
            return true;
        }

        // Guide Chip Rapid Guide Setup
        if (strcmp(name, GuideCCD.RapidGuideSetupSP.name)==0)
        {
            IUUpdateSwitch(&GuideCCD.RapidGuideSetupSP, states, names, n);
            GuideCCD.RapidGuideSetupSP.s=IPS_OK;

            GuiderAutoLoop=(GuideCCD.RapidGuideSetupS[0].s==ISS_ON);
            GuiderSendImage=(GuideCCD.RapidGuideSetupS[1].s==ISS_ON);
            GuiderShowMarker=(GuideCCD.RapidGuideSetupS[2].s==ISS_ON);

            IDSetSwitch(&GuideCCD.RapidGuideSetupSP,NULL);
            return true;
        }
    }

    // Streamer
    #ifdef __linux__
    if (HasStreaming())
        streamer->ISNewSwitch(dev, name, states, names, n);
    #endif

    return DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

int INDI::CCD::SetTemperature(double temperature)
{
    INDI_UNUSED(temperature);
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::SetTemperature %4.2f -  Should never get here", temperature);
    return -1;
}

bool INDI::CCD::StartExposure(float duration)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::StartExposure %4.2f -  Should never get here",duration);
    return false;
}

bool INDI::CCD::StartGuideExposure(float duration)
{
    DEBUGF(INDI::Logger::DBG_WARNING, "INDI::CCD::StartGuide Exposure %4.2f -  Should never get here",duration);
    return false;
}

bool INDI::CCD::AbortExposure()
{
    DEBUG(INDI::Logger::DBG_WARNING, "INDI::CCD::AbortExposure -  Should never get here");
    return false;
}

bool INDI::CCD::AbortGuideExposure()
{
    DEBUG(INDI::Logger::DBG_WARNING, "INDI::CCD::AbortGuideExposure -  Should never get here");
    return false;
}

bool INDI::CCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setFrame(x, y, w, h);
    return true;
}

bool INDI::CCD::UpdateGuiderFrame(int x, int y, int w, int h)
{
    GuideCCD.setFrame(x,y, w,h);
    return true;
}

bool INDI::CCD::UpdateCCDBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    PrimaryCCD.setBin(hor,ver);
    return true;
}

bool INDI::CCD::UpdateGuiderBin(int hor, int ver)
{
    // Just set value, unless HW layer overrides this and performs its own processing
    GuideCCD.setBin(hor, ver);
    return true;
}

bool INDI::CCD::UpdateCCDFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

bool INDI::CCD::UpdateGuiderFrameType(CCDChip::CCD_FRAME fType)
{
    INDI_UNUSED(fType);
    // Child classes can override this
    return true;
}

void INDI::CCD::addFITSKeywords(fitsfile *fptr, CCDChip *targetChip)
{
    int status=0;
    char frame_s[32];
    char dev_name[32];
    char exp_start[32];
    double min_val, max_val;
    double exposureDuration;
    double pixSize1,pixSize2;
    unsigned int xbin, ybin;

    char *orig = setlocale(LC_NUMERIC,"C");

    if (targetChip->getNAxis() == 2)
        getMinMax(&min_val, &max_val, targetChip);

    xbin = targetChip->getBinX();
    ybin = targetChip->getBinY();

    char myDevice[MAXINDIDEVICE];
    strncpy(myDevice, getDeviceName(), MAXINDIDEVICE);
    fits_update_key_s(fptr, TSTRING, "INSTRUME", myDevice, "CCD Name" , &status);

    switch (targetChip->getFrameType())
    {
      case CCDChip::LIGHT_FRAME:
        strcpy(frame_s, "Light");
    break;
      case CCDChip::BIAS_FRAME:
        strcpy(frame_s, "Bias");
    break;
      case CCDChip::FLAT_FRAME:
        strcpy(frame_s, "Flat Field");
    break;
      case CCDChip::DARK_FRAME:
        strcpy(frame_s, "Dark");
    break;
    }

    exposureDuration = targetChip->getExposureDuration();

    pixSize1 = targetChip->getPixelSizeX();
    pixSize2 = targetChip->getPixelSizeY();

    strncpy(dev_name, getDeviceName(), 32);
    strncpy(exp_start, targetChip->getExposureStartTime(), 32);

    fits_update_key_s(fptr, TDOUBLE, "EXPTIME", &(exposureDuration), "Total Exposure Time (s)", &status);

    if(targetChip->getFrameType() == CCDChip::DARK_FRAME)
        fits_update_key_s(fptr, TDOUBLE, "DARKTIME", &(exposureDuration), "Total Exposure Time (s)", &status);

    if (HasCooler())
      fits_update_key_s(fptr, TDOUBLE, "CCD-TEMP" , &(TemperatureN[0].value), "CCD Temperature (Celcius)", &status);

    fits_update_key_s(fptr, TDOUBLE, "PIXSIZE1", &(pixSize1), "Pixel Size 1 (microns)", &status);
    fits_update_key_s(fptr, TDOUBLE, "PIXSIZE2", &(pixSize2), "Pixel Size 2 (microns)", &status);
    fits_update_key_s(fptr, TUINT, "XBINNING", &(xbin) , "Binning factor in width", &status);
    fits_update_key_s(fptr, TUINT, "YBINNING", &(ybin), "Binning factor in height", &status);
    fits_update_key_s(fptr, TSTRING, "FRAME", frame_s, "Frame Type", &status);
    if (CurrentFilterSlot != -1 && CurrentFilterSlot <= FilterNames.size())
    {
        char filter[32];
        strncpy(filter, FilterNames.at(CurrentFilterSlot-1).c_str(), 32);
        fits_update_key_s(fptr, TSTRING, "FILTER", filter, "Filter", &status);
    }

    if (targetChip->getNAxis() == 2)
    {
        fits_update_key_s(fptr, TDOUBLE, "DATAMIN", &min_val, "Minimum value", &status);
        fits_update_key_s(fptr, TDOUBLE, "DATAMAX", &max_val, "Maximum value", &status);
    }

    if (HasBayer() && targetChip->getNAxis() == 2)
    {
        unsigned int bayer_offset_x = atoi(BayerT[0].text);
        unsigned int bayer_offset_y = atoi(BayerT[1].text);

        fits_update_key_s(fptr, TUINT, "XBAYROFF", &bayer_offset_x, "X offset of Bayer array", &status);
        fits_update_key_s(fptr, TUINT, "YBAYROFF", &bayer_offset_y, "Y offset of Bayer array", &status);
        fits_update_key_s(fptr, TSTRING, "BAYERPAT", BayerT[2].text, "Bayer color pattern", &status);
    }

    if (FocalLength != -1)
        fits_update_key_s(fptr, TDOUBLE, "FOCALLEN", &FocalLength, "Focal Length (mm)", &status);

    if (targetChip->getFrameType() == CCDChip::LIGHT_FRAME && RA != -1000 && Dec != -1000)
    {
        ln_equ_posn epochPos, J2000Pos;
        epochPos.ra   = RA*15.0;
        epochPos.dec  = Dec;

        // Convert from JNow to J2000
        //TODO use exp_start instead of julian from system
        ln_get_equ_prec2(&epochPos, ln_get_julian_from_sys(), JD2000, &J2000Pos);

        double raJ2000  = J2000Pos.ra/15.0;
        double decJ2000 = J2000Pos.dec;

        fits_update_key_s(fptr, TDOUBLE, "OBJCTRA", &raJ2000, "Object RA", &status);
        fits_update_key_s(fptr, TDOUBLE, "OBJCTDEC", &decJ2000, "Object DEC", &status);

        int epoch = 2000;

        //fits_update_key_s(fptr, TINT, "EPOCH", &epoch, "Epoch", &status);
        fits_update_key_s(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);


        // Add WCS Info
        if (WorldCoordS[0].s == ISS_ON && ValidCCDRotation && FocalLength != -1)
        {
            raJ2000 *= 15;
            fits_update_key_s(fptr, TDOUBLE, "CRVAL1", &raJ2000, "CRVAL1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CRVAL2", &decJ2000, "CRVAL1", &status);

            char radecsys[8] = "FK5";
            char ctype1[16]  = "RA---TAN";
            char ctype2[16]  = "DEC--TAN";

            fits_update_key_s(fptr, TSTRING, "RADECSYS", radecsys, "RADECSYS", &status);
            fits_update_key_s(fptr, TSTRING, "CTYPE1", ctype1, "CTYPE1", &status);
            fits_update_key_s(fptr, TSTRING, "CTYPE2", ctype2, "CTYPE2", &status);

            double crpix1 = targetChip->getSubW()/targetChip->getBinX()/2.0;
            double crpix2 = targetChip->getSubH()/targetChip->getBinY()/2.0;

            fits_update_key_s(fptr, TDOUBLE, "CRPIX1", &crpix1, "CRPIX1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CRPIX2", &crpix2, "CRPIX2", &status);

            double secpix1 = pixSize1 / FocalLength * 206.3 * targetChip->getBinX();
            double secpix2 = pixSize2 / FocalLength * 206.3 * targetChip->getBinY();

            //double secpix1 = pixSize1 / FocalLength * 206.3;
            //double secpix2 = pixSize2 / FocalLength * 206.3;

            fits_update_key_s(fptr, TDOUBLE, "SECPIX1", &secpix1, "SECPIX1", &status);
            fits_update_key_s(fptr, TDOUBLE, "SECPIX2", &secpix2, "SECPIX2", &status);

            double degpix1 =  secpix1 / 3600.0;
            double degpix2 =  secpix2 / 3600.0;

            fits_update_key_s(fptr, TDOUBLE, "CDELT1", &degpix1, "CDELT1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CDELT2", &degpix2, "CDELT2", &status);

            // Rotation is CW, we need to convert it to CCW per CROTA1 definition
            double rotation = 360 - CCDRotationN[0].value;
            if (rotation > 360)
              rotation -= 360;

            fits_update_key_s(fptr, TDOUBLE, "CROTA1", &rotation, "CROTA1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CROTA2", &rotation, "CROTA2", &status);

            /*double cd[4];
            cd[0] = degpix1;
            cd[1] = 0;
            cd[2] = 0;
            cd[3] = degpix2;

            fits_update_key_s(fptr, TDOUBLE, "CD1_1", &cd[0], "CD1_1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD1_2", &cd[1], "CD1_2", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD2_1", &cd[2], "CD2_1", &status);
            fits_update_key_s(fptr, TDOUBLE, "CD2_2", &cd[3], "CD2_2", &status);*/

        }

    }

    fits_update_key_s(fptr, TSTRING, "DATE-OBS", exp_start, "UTC start date of observation", &status);
    fits_write_comment(fptr, "Generated by INDI", &status);

    setlocale(LC_NUMERIC,orig);

}

void INDI::CCD::fits_update_key_s(fitsfile* fptr, int type, std::string name, void* p, std::string explanation, int* status)
{
        // this function is for removing warnings about deprecated string conversion to char* (from arg 5)
        fits_update_key(fptr,type,name.c_str(),p, const_cast<char*>(explanation.c_str()), status);
}

bool INDI::CCD::ExposureComplete(CCDChip *targetChip)
{
    bool sendImage = (UploadS[0].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool saveImage = (UploadS[1].s == ISS_ON || UploadS[2].s == ISS_ON);
    bool useSolver = (SolverS[0].s == ISS_ON);
    bool showMarker = false;
    bool autoLoop = false;
    bool sendData = false;

    if (RapidGuideEnabled && targetChip == &PrimaryCCD && (PrimaryCCD.getBPP() == 16 || PrimaryCCD.getBPP() == 8))
    {
      autoLoop = AutoLoop;
      sendImage = SendImage;
      showMarker = ShowMarker;
      sendData = true;
      saveImage = false;
    }

    if (GuiderRapidGuideEnabled && targetChip == &GuideCCD && (GuideCCD.getBPP() == 16 || PrimaryCCD.getBPP() == 8))
    {
      autoLoop = GuiderAutoLoop;
      sendImage = GuiderSendImage;
      showMarker = GuiderShowMarker;
      sendData = true;
      saveImage = false;
    }

    if (sendData)
    {
      static double P0 = 0.906, P1 = 0.584, P2 = 0.365, P3 = 0.117, P4 = 0.049, P5 = -0.05, P6 = -0.064, P7 = -0.074, P8 = -0.094;
      targetChip->RapidGuideDataNP.s=IPS_BUSY;
      int width = targetChip->getSubW() / targetChip->getBinX();
      int height = targetChip->getSubH() / targetChip->getBinY();
      void *src = (unsigned short *) targetChip->getFrameBuffer();
      int i0, i1, i2, i3, i4, i5, i6, i7, i8;
      int ix = 0, iy = 0;
      int xM4;
      double average, fit, bestFit = 0;
      int minx = 4;
      int maxx = width -4;
      int miny = 4;
      int maxy = height -4;
      if (targetChip->lastRapidX > 0 && targetChip->lastRapidY > 0) {
        minx = std::max(targetChip->lastRapidX - 20, 4);
        maxx = std::min(targetChip->lastRapidX + 20, width - 4);
        miny = std::max(targetChip->lastRapidY - 20, 4);
        maxy = std::min(targetChip->lastRapidY + 20, height -4);
      }
      if (targetChip->getBPP() == 16) {
        unsigned short *p;
        for (int x = minx; x < maxx; x++)
          for (int y = miny; y < maxy; y++) {
            i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
            xM4 = x - 4;
            p = (unsigned short *)src + (y - 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y - 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y - 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y - 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y + 0) * width + xM4; i8 += *p++; i6 += *p++; i3 += *p++; i1 += *p++; i0 += *p++; i1 += *p++; i3 += *p++; i6 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y + 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y + 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y + 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned short *)src + (y + 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
            fit = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) + P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) + P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
            if (bestFit < fit) {
              bestFit = fit;
              ix = x;
              iy = y;
            }
          }
      } else {
        unsigned char *p;
        for (int x = minx; x < maxx; x++)
          for (int y = miny; y < maxy; y++) {
            i0 = i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = 0;
            xM4 = x - 4;
            p = (unsigned char *)src + (y - 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y - 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y - 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y - 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y + 0) * width + xM4; i8 += *p++; i6 += *p++; i3 += *p++; i1 += *p++; i0 += *p++; i1 += *p++; i3 += *p++; i6 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y + 1) * width + xM4; i8 += *p++; i7 += *p++; i4 += *p++; i2 += *p++; i1 += *p++; i2 += *p++; i4 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y + 2) * width + xM4; i8 += *p++; i8 += *p++; i5 += *p++; i4 += *p++; i3 += *p++; i4 += *p++; i5 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y + 3) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i7 += *p++; i6 += *p++; i7 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            p = (unsigned char *)src + (y + 4) * width + xM4; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++; i8 += *p++;
            average = (i0 + i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8) / 85.0;
            fit = P0 * (i0 - average) + P1 * (i1 - 4 * average) + P2 * (i2 - 4 * average) + P3 * (i3 - 4 * average) + P4 * (i4 - 8 * average) + P5 * (i5 - 4 * average) + P6 * (i6 - 4 * average) + P7 * (i7 - 8 * average) + P8 * (i8 - 48 * average);
            if (bestFit < fit) {
              bestFit = fit;
              ix = x;
              iy = y;
            }
          }
      }

      targetChip->RapidGuideDataN[0].value = ix;
      targetChip->RapidGuideDataN[1].value = iy;
      targetChip->RapidGuideDataN[2].value = bestFit;
      targetChip->lastRapidX = ix;
      targetChip->lastRapidY = iy;
      if (bestFit > 50) {
        int sumX = 0;
        int sumY = 0;
        int total = 0;
        int max = 0;
        int noiseThreshold = 0;

        if (targetChip->getBPP() == 16) {
          unsigned short *p;
          for (int y = iy - 4; y <= iy + 4; y++) {
            p = (unsigned short *)src + y * width + ix - 4;
            for (int x = ix - 4; x <= ix + 4; x++) {
              int w = *p++;
              noiseThreshold += w;
              if (w > max)
                max = w;
            }
          }
          noiseThreshold = (noiseThreshold/81+max)/2; // set threshold between peak and average
          for (int y = iy - 4; y <= iy + 4; y++) {
            p = (unsigned short *)src + y * width + ix - 4;
            for (int x = ix - 4; x <= ix + 4; x++) {
              int w = *p++;
              if (w < noiseThreshold)
                w = 0;
              sumX += x * w;
              sumY += y * w;
              total += w;
            }
          }
        } else {
          unsigned char *p;
          for (int y = iy - 4; y <= iy + 4; y++) {
            p = (unsigned char *)src + y * width + ix - 4;
            for (int x = ix - 4; x <= ix + 4; x++) {
              int w = *p++;
              noiseThreshold += w;
              if (w > max)
                max = w;
            }
          }
          noiseThreshold = (noiseThreshold/81+max)/2; // set threshold between peak and average
          for (int y = iy - 4; y <= iy + 4; y++) {
            p = (unsigned char *)src + y * width + ix - 4;
            for (int x = ix - 4; x <= ix + 4; x++) {
              int w = *p++;
              if (w < noiseThreshold)
                w = 0;
              sumX += x * w;
              sumY += y * w;
              total += w;
            }
          }
        }

        if (total > 0)
        {
          targetChip->RapidGuideDataN[0].value = ((double)sumX)/total;
          targetChip->RapidGuideDataN[1].value = ((double)sumY)/total;
          targetChip->RapidGuideDataNP.s=IPS_OK;

          DEBUGF(INDI::Logger::DBG_DEBUG, "Guide Star X: %g Y: %g FIT: %g", targetChip->RapidGuideDataN[0].value, targetChip->RapidGuideDataN[1].value,
                  targetChip->RapidGuideDataN[2].value);
        }
        else
        {
          targetChip->RapidGuideDataNP.s=IPS_ALERT;
          targetChip->lastRapidX = targetChip->lastRapidY = -1;
        }
      }
      else
      {
        targetChip->RapidGuideDataNP.s=IPS_ALERT;
        targetChip->lastRapidX = targetChip->lastRapidY = -1;
      }
      IDSetNumber(&targetChip->RapidGuideDataNP,NULL);

      if (showMarker)
      {
        int xmin = std::max(ix - 10, 0);
        int xmax = std::min(ix + 10, width - 1);
        int ymin = std::max(iy - 10, 0);
        int ymax = std::min(iy + 10, height - 1);

        //fprintf(stderr, "%d %d %d %d\n", xmin, xmax, ymin, ymax);

        if (targetChip->getBPP() == 16) {
          unsigned short *p;
          if (ymin > 0)
          {
            p = (unsigned short *)src + ymin * width + xmin;
            for (int x = xmin; x <= xmax; x++)
              *p++ = 50000;
          }

          if (xmin > 0)
          {
            for (int y = ymin; y<= ymax; y++)
            {
              *((unsigned short *)src + y * width + xmin) = 50000;
            }
          }

          if (xmax < width - 1)
          {
            for (int y = ymin; y<= ymax; y++)
            {
              *((unsigned short *)src + y * width + xmax) = 50000;
            }
          }

          if (ymax < height -1)
          {
            p = (unsigned short *)src + ymax * width + xmin;
            for (int x = xmin; x <= xmax; x++)
              *p++ = 50000;
          }
        } else {
          unsigned char *p;
          if (ymin > 0)
          {
            p = (unsigned char *)src + ymin * width + xmin;
            for (int x = xmin; x <= xmax; x++)
              *p++ = 255;
          }

          if (xmin > 0)
          {
            for (int y = ymin; y<= ymax; y++)
            {
              *((unsigned char *)src + y * width + xmin) = 255;
            }
          }

          if (xmax < width - 1)
          {
            for (int y = ymin; y<= ymax; y++)
            {
              *((unsigned char *)src + y * width + xmax) = 255;
            }
          }

          if (ymax < height -1)
          {
            p = (unsigned char *)src + ymax * width + xmin;
            for (int x = xmin; x <= xmax; x++)
              *p++ = 255;
          }

        }
      }
    }

    if (sendImage || saveImage || useSolver)
    {
      if (!strcmp(targetChip->getImageExtension(), "fits"))
      {
          void *memptr;
          size_t memsize;
          int img_type=0;
          int byte_type=0;
          int status=0;
          long naxis=targetChip->getNAxis();
          long naxes[naxis];
          int nelements=0;
          std::string bit_depth;

          fitsfile *fptr=NULL;

          naxes[0]=targetChip->getSubW()/targetChip->getBinX();
          naxes[1]=targetChip->getSubH()/targetChip->getBinY();

          switch (targetChip->getBPP())
          {
              case 8:
                  byte_type = TBYTE;
                  img_type  = BYTE_IMG;
                  bit_depth = "8 bits per pixel";
                  break;

              case 16:
                  byte_type = TUSHORT;
                  img_type = USHORT_IMG;
                  bit_depth = "16 bits per pixel";
                  break;

              case 32:
                  byte_type = TULONG;
                  img_type = ULONG_IMG;
                  bit_depth = "32 bits per pixel";
                  break;

               default:
                  DEBUGF(Logger::DBG_WARNING, "Unsupported bits per pixel value %d\n", targetChip->getBPP() );
                  return false;
                  break;
          }

          nelements = naxes[0] * naxes[1];
          if (naxis== 3)
          {
              nelements *= 3;
              naxes[2] = 3;
          }

          /*DEBUGF(Logger::DBG_DEBUG, "Exposure complete. Image Depth: %s. Width: %d Height: %d nelements: %d", bit_depth.c_str(), naxes[0],
                  naxes[1], nelements);*/

          //  Now we have to send fits format data to the client
          memsize=5760;
          memptr=malloc(memsize);
          if(!memptr)
          {
              DEBUGF(INDI::Logger::DBG_ERROR, "Error: failed to allocate memory: %lu",(unsigned long)memsize);
          }

          fits_create_memfile(&fptr,&memptr,&memsize,2880,realloc,&status);

          if(status)
          {
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          fits_create_img(fptr, img_type , naxis, naxes, &status);

          if (status)
          {
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          addFITSKeywords(fptr, targetChip);

          fits_write_img(fptr,byte_type,1,nelements,targetChip->getFrameBuffer(),&status);

          if (status)
          {
            fits_report_error(stderr, status);  /* print out any error messages */
            return false;
          }

          fits_close_file(fptr,&status);

          uploadFile(targetChip, memptr, memsize, sendImage, saveImage, useSolver);

          free(memptr);
      }
      else
      {
          uploadFile(targetChip, targetChip->getFrameBuffer(), targetChip->getFrameBufferSize(), sendImage, saveImage);
      }


    }

    targetChip->ImageExposureNP.s=IPS_OK;
    IDSetNumber(&targetChip->ImageExposureNP,NULL);

    if (autoLoop)
    {
      if (targetChip == &PrimaryCCD)
      {
        PrimaryCCD.ImageExposureN[0].value = ExposureTime;
        PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
        if (StartExposure(ExposureTime))
           PrimaryCCD.ImageExposureNP.s=IPS_BUSY;
        else
        {
           DEBUG(INDI::Logger::DBG_DEBUG, "Autoloop: Primary CCD Exposure Error!");
           PrimaryCCD.ImageExposureNP.s=IPS_ALERT;
        }

        IDSetNumber(&PrimaryCCD.ImageExposureNP,NULL);
      }
      else
      {
        GuideCCD.ImageExposureN[0].value = GuiderExposureTime;
        GuideCCD.ImageExposureNP.s=IPS_BUSY;
        if (StartGuideExposure(GuiderExposureTime))
           GuideCCD.ImageExposureNP.s=IPS_BUSY;
        else
        {
           DEBUG(INDI::Logger::DBG_DEBUG, "Autoloop: Guide CCD Exposure Error!");
           GuideCCD.ImageExposureNP.s=IPS_ALERT;
        }

        IDSetNumber(&GuideCCD.ImageExposureNP,NULL);
      }
    }

    return true;
}

bool INDI::CCD::uploadFile(CCDChip * targetChip, const void *fitsData, size_t totalBytes, bool sendImage, bool saveImage, bool useSolver)
{
    unsigned char *compressedData = NULL;
    uLongf compressedBytes=0;

    if (saveImage)
    {
        targetChip->FitsB.blob=(unsigned char *)fitsData;
        targetChip->FitsB.bloblen=totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());

        FILE *fp = NULL;
        char imageFileName[MAXRBUF];

        if (useSolver)
        {
            strncpy(imageFileName, "/tmp/ccdsolver.fits", MAXRBUF);
        }
        else
        {
            std::string prefix = UploadSettingsT[1].text;
            int maxIndex = getFileIndex(UploadSettingsT[0].text, UploadSettingsT[1].text, targetChip->FitsB.format);

            if (maxIndex < 0)
            {
                DEBUGF(INDI::Logger::DBG_ERROR, "Error iterating directory %s. %s", UploadSettingsT[0].text, strerror(errno));
                return false;
            }

            if (maxIndex > 0)
            {
                char indexString[4];
                snprintf(indexString, 4, "%03d", maxIndex);
                std::string prefixIndex = indexString;
                prefix.replace(prefix.find("XXX"), 3, prefixIndex);
            }

            snprintf(imageFileName, MAXRBUF, "%s/%s%s", UploadSettingsT[0].text, prefix.c_str(), targetChip->FitsB.format);
        }

        fp = fopen(imageFileName, "w");
        if (fp == NULL)
        {
            DEBUGF(INDI::Logger::DBG_ERROR, "Unable to save image file (%s). %s", imageFileName, strerror(errno));
            return false;
        }

        int n=0;
        for (int nr=0; nr < (int) targetChip->FitsB.bloblen; nr += n)
            n = fwrite( (static_cast<char *>(targetChip->FitsB.blob) + nr), 1, targetChip->FitsB.bloblen - nr, fp);

        fclose(fp);

        // Save image file path
        IUSaveText(&FileNameT[0], imageFileName);

        if (useSolver)
        {
            pthread_mutex_lock(&lock);
            SolverSP.s = IPS_BUSY;
            DEBUG(INDI::Logger::DBG_SESSION, "Solving image...");
            IDSetSwitch(&SolverSP, NULL);
            pthread_mutex_unlock(&lock);

            int result = pthread_create( &solverThread, NULL, &INDI::CCD::runSolverHelper, this);

            if (result != 0)
            {
                SolverSP.s = IPS_ALERT;
                DEBUGF(INDI::Logger::DBG_SESSION, "Failed to create solver thread: %s", strerror(errno));
                IDSetSwitch(&SolverSP, NULL);
            }
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Image saved to %s", imageFileName);
            FileNameTP.s = IPS_OK;
            IDSetText(&FileNameTP, NULL);
        }
    }

    if (targetChip->SendCompressed)
    {
        compressedBytes = sizeof(char) * totalBytes + totalBytes / 64 + 16 + 3;
        compressedData = (unsigned char *) malloc (compressedBytes);

        if (fitsData == NULL || compressedData == NULL)
        {
            if (compressedData)
                free(compressedData);
            DEBUG(INDI::Logger::DBG_ERROR, "Error: Ran out of memory compressing image");
            return false;
        }

        int r = compress2(compressedData, &compressedBytes, (const Bytef*)fitsData, totalBytes, 9);
        if (r != Z_OK)
        {
            /* this should NEVER happen */
            DEBUG(INDI::Logger::DBG_ERROR, "Error: Failed to compress image");
            return false;
        }

        targetChip->FitsB.blob=compressedData;
        targetChip->FitsB.bloblen=compressedBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s.z", targetChip->getImageExtension());
    } else
    {
        targetChip->FitsB.blob=(unsigned char *)fitsData;
        targetChip->FitsB.bloblen=totalBytes;
        snprintf(targetChip->FitsB.format, MAXINDIBLOBFMT, ".%s", targetChip->getImageExtension());
    }

    targetChip->FitsB.size = totalBytes;
    targetChip->FitsBP.s=IPS_OK;

    if (sendImage)
        IDSetBLOB(&targetChip->FitsBP,NULL);

    if (compressedData)
        free (compressedData);

    return true;
}

void INDI::CCD::SetCCDParams(int x,int y,int bpp,float xf,float yf)
{
    PrimaryCCD.setResolution(x, y);
    PrimaryCCD.setFrame(0, 0, x, y);
    if (CanBin())
        PrimaryCCD.setBin(1,1);
    PrimaryCCD.setPixelSize(xf, yf);
    PrimaryCCD.setBPP(bpp);

}

void INDI::CCD::SetGuiderParams(int x,int y,int bpp,float xf,float yf)
{
    capability |= CCD_HAS_GUIDE_HEAD;

    GuideCCD.setResolution(x, y);
    GuideCCD.setFrame(0, 0, x, y);
    GuideCCD.setPixelSize(xf, yf);
    GuideCCD.setBPP(bpp);

}

bool INDI::CCD::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &ActiveDeviceTP);
    IUSaveConfigSwitch(fp, &UploadSP);
    IUSaveConfigText(fp, &UploadSettingsTP);
    IUSaveConfigSwitch(fp, &TelescopeTypeSP);

    IUSaveConfigSwitch(fp, &PrimaryCCD.CompressSP);

    if (HasGuideHead())
        IUSaveConfigSwitch(fp, &GuideCCD.CompressSP);

    if (CanSubFrame())
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageFrameNP);

    if (CanBin())
        IUSaveConfigNumber(fp, &PrimaryCCD.ImageBinNP);

    if (HasBayer())
        IUSaveConfigText(fp, &BayerTP);

    IUSaveConfigText(fp, &SolverSettingsTP);

    return true;
}

IPState INDI::CCD::GuideNorth(float ms)
{
    INDI_UNUSED(ms);
    DEBUG(INDI::Logger::DBG_ERROR, "The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState INDI::CCD::GuideSouth(float ms)
{
    INDI_UNUSED(ms);
    DEBUG(INDI::Logger::DBG_ERROR, "The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState INDI::CCD::GuideEast(float ms)
{
    INDI_UNUSED(ms);
    DEBUG(INDI::Logger::DBG_ERROR, "The CCD does not support guiding.");
    return IPS_ALERT;
}

IPState INDI::CCD::GuideWest(float ms)
{
    INDI_UNUSED(ms);
    DEBUG(INDI::Logger::DBG_ERROR, "The CCD does not support guiding.");
    return IPS_ALERT;
}

void INDI::CCD::getMinMax(double *min, double *max, CCDChip *targetChip)
{
    int ind=0, i, j;
    int imageHeight = targetChip->getSubH() / targetChip->getBinY();
    int imageWidth  = targetChip->getSubW() / targetChip->getBinX();
    double lmin, lmax;

    switch (targetChip->getBPP())
    {
        case 8:
        {
            unsigned char *imageBuffer = (unsigned char *) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];


            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];
                }

        }
        break;

        case 16:
        {
            unsigned short *imageBuffer = (unsigned short* ) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];

            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                     else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];
                }

        }
        break;

        case 32:
        {
            unsigned int *imageBuffer = (unsigned int* ) targetChip->getFrameBuffer();
            lmin = lmax = imageBuffer[0];

            for (i= 0; i < imageHeight ; i++)
                for (j= 0; j < imageWidth; j++)
                {
                    ind = (i * imageWidth) + j;
                    if (imageBuffer[ind] < lmin) lmin = imageBuffer[ind];
                    else if (imageBuffer[ind] > lmax) lmax = imageBuffer[ind];

                }

        }
        break;

    }
        *min = lmin;
        *max = lmax;
}

int INDI::CCD::getFileIndex(const char *dir, const char *prefix, const char *ext)
{
    DIR *dpdf;
    struct dirent *epdf;
    std::vector<std::string> files = std::vector<std::string>();

    std::string prefixIndex = prefix;
    if (prefixIndex.find("XXX") == std::string::npos)
        return 0;

    std::string prefixSearch = prefix;
    prefixSearch.replace(prefixSearch.find("XXX"), 3, "");

    // Create directory if does not exist
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
       DEBUGF(INDI::Logger::DBG_DEBUG, "Creating directory %s...", dir);
       if (_mkdir(dir, 0755) == -1)
           DEBUGF(INDI::Logger::DBG_ERROR, "Error creating directory %s (%s)", dir, strerror(errno));
    }

    dpdf = opendir(dir);
    if (dpdf != NULL)
    {
       while (epdf = readdir(dpdf))
       {
          if (strstr(epdf->d_name, prefixSearch.c_str()))
              files.push_back(epdf->d_name);
       }
    }
    else
        return -1;

    int maxIndex=0;

    std::string filterIndex = "%d";
    prefixIndex.replace(prefixIndex.find("XXX"), 3, filterIndex);
    char filter[MAXRBUF];
    snprintf(filter, MAXRBUF, "%s%s", prefixIndex.c_str(), ext);
    for (int i=0; i < files.size(); i++)
    {
        int index=-1;
        sscanf(files.at(i).c_str(), filter, &index);
        if (index > maxIndex)
            maxIndex=index;
    }

    return (maxIndex+1);

}

void INDI::CCD::GuideComplete(INDI_EQ_AXIS axis)
{
    INDI::GuiderInterface::GuideComplete(axis);
}

bool INDI::CCD::StartStreaming()
{
    DEBUG(INDI::Logger::DBG_ERROR, "Streaming is not supported.");
    return false;
}

bool INDI::CCD::StopStreaming()
{
    DEBUG(INDI::Logger::DBG_ERROR, "Streaming is not supported.");
    return false;
}

void * INDI::CCD::runSolverHelper(void *context)
{
  (static_cast<INDI::CCD *> (context))->runSolver();
  return NULL;
}

void INDI::CCD::runSolver()
{
    char cmd[MAXRBUF], line[256];
    float ra,dec, angle, pixscale, field_w, field_h;
    ra=dec=angle=pixscale=field_w=field_h=-1000;
    snprintf(cmd, MAXRBUF, "%s %s -W /tmp/solution.wcs /tmp/ccdsolver.fits",SolverSettingsT[ASTROMETRY_SETTINGS_BINARY].text, SolverSettingsT[ASTROMETRY_SETTINGS_OPTIONS].text);

    DEBUGF(INDI::Logger::DBG_DEBUG, "%s", cmd);
    FILE *handle = popen(cmd, "r");
    if (handle == NULL)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Failed to run solver: %s", strerror(errno));
        pthread_mutex_lock(&lock);
        SolverSP.s = IPS_ALERT;
        IDSetSwitch(&SolverSP, NULL);
        pthread_mutex_unlock(&lock);
        return;
    }

    while (fgets(line, sizeof(line), handle))
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s", line);

        sscanf(line, "Field rotation angle: up is %f", &angle);
        sscanf(line, "Field center: (RA,Dec) = (%f,%f)", &ra, &dec);
        sscanf(line, "Field size: %f x %f arcminutes", &field_w, &field_h);

        if (field_w != -1000 && ra != -1000 && dec != -1000 && angle != -1000)
        {
            // Pixscale is arcsec/pixel. Astrometry result is in arcmin
            SolverResultN[ASTROMETRY_RESULTS_PIXSCALE].value = (field_w * 60) / PrimaryCCD.getSubW();
            // Astrometry.net angle, E of N
            SolverResultN[ASTROMETRY_RESULTS_ORIENTATION].value = angle;
            // Astrometry.net J2000 RA in degrees
            SolverResultN[ASTROMETRY_RESULTS_RA].value = ra;
            // Astrometry.net J2000 DEC in degrees
            SolverResultN[ASTROMETRY_RESULTS_DE].value = dec;

            SolverResultNP.s = IPS_OK;
            IDSetNumber(&SolverResultNP, NULL);

            pthread_mutex_lock(&lock);
            SolverSP.s = IPS_OK;
            IDSetSwitch(&SolverSP, NULL);
            pthread_mutex_unlock(&lock);

            fclose(handle);
            DEBUG(INDI::Logger::DBG_SESSION, "Solver complete.");
            return;
        }

        pthread_mutex_lock(&lock);
        if (SolverS[1].s == ISS_ON)
        {
            SolverSP.s = IPS_IDLE;
            IDSetSwitch(&SolverSP, NULL);
            pthread_mutex_unlock(&lock);
            fclose(handle);
            DEBUG(INDI::Logger::DBG_SESSION, "Solver cancelled.");
            return;
        }
        pthread_mutex_unlock(&lock);
    }

    fclose(handle);

    pthread_mutex_lock(&lock);
    SolverSP.s = IPS_ALERT;
    IDSetSwitch(&SolverSP, NULL);
    DEBUG(INDI::Logger::DBG_SESSION, "Solver failed.");
    pthread_mutex_unlock(&lock);

    pthread_exit(0);

}
