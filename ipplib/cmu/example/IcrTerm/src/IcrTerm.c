/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *-----------------------------------------------------------------------------
 */

// IcrTerm.c : Defines the entry point for the console application.
//

#if defined(WM) || defined (WINCE)
#include "stdafx.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "ctrlsvr.h"
#include "utility.h"
#include "viewmodes.h"




#define ARRAY_SIZE(a)\
	( sizeof(a) / sizeof((a)[0]) )

#if defined(WM) || defined (WINCE)
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	IcrSession *pIcrSession;
	IcrError err;

	IcrBitmapInfo srcInfo, dstInfo;
	int iBpp, bIndexed;
	BYTE *pSrc, *pDst;

	char cmd[256];
	int x, y, left, top, right, bottom;
	int bEnabled;
	char sViewModes[][20] = {"Custom", "Standard", "Vivid", "Soft", "Landscape", "Portrait", "Sepia", "Platinum"};
	char sAceModes[][20] = {"Off", "Low", "Medium", "High"};
	char sRoute[][20] = {"Off", "TV", "LCD1", "LCD2"};
	long long token;

	IcrChannel eChannel = ICR_MAIN_CHANNEL;



#if defined(PLATFORM_PXA168) || defined(WIN32)
	if (argc != 3)
	{
		LOG("Incorrect number of parameters!\n");
		LOG("Usage:\n\t%s <input_image> <output_image>\n", argv[0]);
		return -1;
	}
#endif


	err = IcrOpen(&pIcrSession);
	ICR_CHECK(err == ICR_ERR_OK, "Failed to opent the ICR device!\n");

	err = SetupIcrModes(pIcrSession);
	ICR_CHECK(err == ICR_ERR_OK, "Failed to set up the ODM/OEM specific view modes\n");

#if defined(PLATFORM_PXA168) || defined(WIN32)
	IcrGetBitmapInfo(argv[1], &srcInfo.iWidth, &srcInfo.iHeight, &iBpp, &bIndexed);
	ICR_CHECK(iBpp==24 && !bIndexed, "Only 24bit, non-indexed bitmap is supported!\n");

	pSrc = (BYTE*)IcrBufAlloc(pIcrSession, srcInfo.iWidth, srcInfo.iHeight, 3, 0);
	pDst = (BYTE*)IcrBufAlloc(pIcrSession, srcInfo.iWidth, srcInfo.iHeight, 3, 0);
	srcInfo.iFormat = ICR_FORMAT_RGB888;
	IcrLoadBitmap(argv[1], pSrc, &srcInfo.iWidth, &srcInfo.iHeight);

	dstInfo.iWidth = srcInfo.iWidth;
	dstInfo.iHeight = srcInfo.iHeight;
	dstInfo.iFormat = ICR_FORMAT_RGB888;

	IcrFormatSet(pIcrSession, ICR_FORMAT_RGB888, ICR_FORMAT_RGB888);

	// Transform
	err = IcrTransform(pIcrSession, pSrc, &srcInfo, pDst, &dstInfo, 0, 0, &token);
	ICR_ASSERT(err == ICR_ERR_OK, "");

	err = IcrWaitTransform(pIcrSession, token);
	ICR_ASSERT(err == ICR_ERR_OK, "");


	// Store/display image
	IcrStoreBitmap(argv[2], pDst, srcInfo.iWidth, srcInfo.iHeight);
	IcrDisplayBitmap(pDst, srcInfo.iWidth, srcInfo.iHeight);
#endif

	while (1) {
		// Display the usage
		LOG("\n*******************************************\n");

		LOG("Usage: %s <input bmp file> <output bmp file>\n", argv[0]);
		LOG("Commands:\n");
		LOG("\tq\t\tquit\n");
		LOG("\tr=<0~3>\tset the cmu route (0:off, 1:tv, 2:lcd1, 3:lcd2)\n");
		LOG("\tp=<l,t,r,b>\tset the pip channel rectangle\n");
		LOG("\tl=<l,t,r,b>\tset the letter box rectangle\n");
		LOG("\tx\t\tswitch the control to main/pip channel\n");
		LOG("\tm=<0~%d>\tswitch mode\n", ARRAY_SIZE(sViewModes)-1);
		for (x=0 ; x<ARRAY_SIZE(sViewModes) ; x++)
			LOG("\t\t%d - %s\n", x, sViewModes[x]);
		LOG("\tb=<%d~%d>\tset the brightness level\n", BRIGHTNESS_MIN, BRIGHTNESS_MAX);
		LOG("\tc=<%d~%d>\tset the contrast\n", CONTRAST_MIN, CONTRAST_MAX);
		LOG("\th=<%d~%d>\tset the hue\n", HUE_MIN, HUE_MAX);
		LOG("\ts=<%d~%d>\tset the saturation\n", SATURATION_MIN, SATURATION_MAX);
		LOG("\tt=<%d~%d>\tset the color tone (color temperature)\n", TONE_MIN, TONE_MAX);
		LOG("\tbe=<0~14>\tset bright enhancement level (0~14 for \"off\" to \"highest\")\n");
		LOG("\tace=<0~3>\tset auto contrast enhancement mode (0:off, 1:low, 2:medium, 3:high)\n");
		LOG("\tf\t\tswitch on/off the flesh tone preserve\n");
		LOG("\tg\t\tswitch on/off the gamut compression\n");
		LOG("\n-------------------------------------------\n");

		err = IcrRouteGet(pIcrSession, (IcrRoute*)&x);
		ICR_ASSERT(err == ICR_ERR_OK, "");
		if (x != ICR_ROUTE_OFF)
		{
			// Display the main channel settings
			err = IcrMainGet(pIcrSession, &x, &y);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("Main Channel (0, 0, %d, %d):\n", x, y);

			err = IcrViewModeGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(m)Mode           0~%d            %d - %s\n", ARRAY_SIZE(sViewModes)-1, x, sViewModes[x]);

			err = IcrFleshToneGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(f)Flesh Tone     On/Off         %s\n", x ? "On" : "Off");

			err = IcrGamutCompressGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(g)Gamut Compress On/Off         %s\n", x ? "On" : "Off");

			err = IcrBrightnessGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(b)Brightness     -127~128       %d\n", x);

			err = IcrContrastGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(c)Contrast       -127~128       %d\n", x);

			err = IcrHueGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(h)Hue            0~360          %d\n", x);

			err = IcrSaturationGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(s)Saturation     -127~128       %d\n", x);

			err = IcrToneGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(t)Color Tone     4000~10000     %d\n", x);

			err = IcrBeLevelGet(pIcrSession, ICR_MAIN_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(be)BE Level      0~14           %d\n", x);

			err = IcrAceModeGet(pIcrSession, ICR_MAIN_CHANNEL, (IcrAceMode *)&x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(ace)ACE Mode     0~3            %d - %s\n", x, sAceModes[x]);

			LOG("\n-------------------------------------------\n");
			err = IcrPipGet(pIcrSession, &left, &top, &right, &bottom);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("PIP Channel (%d, %d, %d, %d):\n", left, top, right, bottom);

			// Display the state
			err = IcrViewModeGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(m)Mode           0~%d            %d - %s\n", ARRAY_SIZE(sViewModes)-1, x, sViewModes[x]);

			err = IcrFleshToneGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(f)Flesh Tone     On/Off         %s\n", x ? "On" : "Off");

			err = IcrGamutCompressGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(g)Gamut Compress On/Off         %s\n", x ? "On" : "Off");

			err = IcrBrightnessGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(b)Brightness     -127~128       %d\n", x);

			err = IcrContrastGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(c)Contrast       -127~128       %d\n", x);

			err = IcrHueGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(h)Hue            0~360          %d\n", x);

			err = IcrSaturationGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(s)Saturation     -127~128       %d\n", x);

			err = IcrToneGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(t)Color Tone     4000~10000     %d\n", x);

			err = IcrBeLevelGet(pIcrSession, ICR_PIP_CHANNEL, &x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(be)BE Level      0~14           %d\n", x);

			err = IcrAceModeGet(pIcrSession, ICR_PIP_CHANNEL, (IcrAceMode *)&x);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(ace)ACE Mode     0~3            %d - %s\n", x, sAceModes[x]);

			LOG("\n-------------------------------------------\n");

			err = IcrPipGet(pIcrSession, &left, &top, &right, &bottom);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(p)Pip Rect       (l,t,r,b)      (%d, %d, %d, %d)\n", left, top, right, bottom);

			err = IcrLetterBoxGet(pIcrSession, &left, &top, &right, &bottom);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			LOG("(l)Letter Box     (l,t,r,b)      (%d, %d, %d, %d)\n", left, top, right, bottom);

			LOG("(x)Control        Main/Pip       %s\n", eChannel==ICR_MAIN_CHANNEL ? "Main" : "Pip");
		}

		err = IcrRouteGet(pIcrSession, (IcrRoute*)&x);
		ICR_ASSERT(err == ICR_ERR_OK, "");
		LOG("(r)Route          0~3            %d - %s\n", x, sRoute[x]);

		LOG("(q)Quit\n");

		LOG("*******************************************\n\n");

		// accept the command
		while (0 == scanf("%s", cmd));

		// Parse command and change the settings
		switch ( cmd[0] )
		{
		case 'q':
		case 'Q':
			LOG("Quit...\n");
			goto exit;
			break;

		case 'r':
		case 'R':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrRouteSet(pIcrSession, (IcrRoute)x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'm':
		case 'M':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrViewModeSet(pIcrSession, eChannel, x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'f':
		case 'F':
			err = IcrFleshToneGet(pIcrSession, eChannel, &bEnabled);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			err = IcrFleshToneEnable(pIcrSession, eChannel, !bEnabled);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			break;

		case 'g':
		case 'G':
			err = IcrGamutCompressGet(pIcrSession, eChannel, &bEnabled);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			err = IcrGamutCompressEnable(pIcrSession, eChannel, !bEnabled);
			ICR_ASSERT(err == ICR_ERR_OK, "");
			break;

		case 'b':
		case 'B':
			if ((cmd[1]=='e' || cmd[1]=='E'))
			{
				if (1 == sscanf(cmd+3, "%d", &x))
				{
					err = IcrBeLevelSet(pIcrSession, eChannel, x);
					ICR_ASSERT(err == ICR_ERR_OK, "");
				}
			}
			else 
			{
				if (1 == sscanf(cmd+2, "%d", &x))
				{
					err = IcrBrightnessSet(pIcrSession, eChannel, x);
					ICR_ASSERT(err == ICR_ERR_OK, "");
				}
			}
			break;

		case 'c':
		case 'C':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrContrastSet(pIcrSession, eChannel, x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'h':
		case 'H':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrHueSet(pIcrSession, eChannel, x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 's':
		case 'S':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrSaturationSet(pIcrSession, eChannel, x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 't':
		case 'T':
			if (1 == sscanf(cmd+2, "%d", &x))
			{
				err = IcrToneSet(pIcrSession, eChannel, x);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'a':
		case 'A':
			if ((cmd[1]=='c' || cmd[1]=='C') && (cmd[2]=='e' || cmd[2]=='E'))
			{
				if (1 == sscanf(cmd+4, "%d", &x))
				{
					err = IcrAceModeSet(pIcrSession, eChannel, (IcrAceMode)x);
					ICR_ASSERT(err == ICR_ERR_OK, "");
				}
			}
			else
			{
				ICR_ASSERT(0, "Not supported command '%s'\n", cmd);
			}
			break;

		case 'u':
		case 'U':
			LOG("Frame update\n", x);
			break;

		case 'p':
		case 'P':
			if (4 == sscanf(cmd+2, "%d,%d,%d,%d", &left, &top, &right, &bottom))
			{
				err = IcrPipSet(pIcrSession, left, top, right, bottom);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'l':
		case 'L':
			if (4 == sscanf(cmd+2, "%d,%d,%d,%d", &left, &top, &right, &bottom))
			{
				err = IcrLetterBoxSet(pIcrSession, left, top, right, bottom);
				ICR_ASSERT(err == ICR_ERR_OK, "");
			}
			break;

		case 'x':
		case 'X':
			if (eChannel == ICR_MAIN_CHANNEL)
				eChannel = ICR_PIP_CHANNEL;
			else
				eChannel = ICR_MAIN_CHANNEL;
			continue;
			break;

		default:
			ICR_ASSERT(0, "Not supported command '%s'\n", cmd);
			break;
		}

#if defined(PLATFORM_PXA168) || defined(WIN32)
		// Transform
		err = IcrTransform(pIcrSession, pSrc, &srcInfo, pDst, &dstInfo, 0, 0, &token);
		ICR_ASSERT(err == ICR_ERR_OK, "");

		err = IcrWaitTransform(pIcrSession, token);
		ICR_ASSERT(err == ICR_ERR_OK, "");


		// Store/display image
		IcrStoreBitmap(argv[2], pDst, srcInfo.iWidth, srcInfo.iHeight);
		IcrDisplayBitmap(pDst, srcInfo.iWidth, srcInfo.iHeight);
#endif
	}

exit:
#if defined(PLATFORM_PXA168) || defined(WIN32)
	IcrBufFree(pIcrSession, pSrc);
	IcrBufFree(pIcrSession, pDst);
#endif
	IcrClose(pIcrSession);

	return 0;
}

