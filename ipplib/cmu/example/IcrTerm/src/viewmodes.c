/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *-----------------------------------------------------------------------------
 */

#include "ctrlsvr.h"
#include "viewmodes.h"

const IcrViewModeParam icr_vm_vivid = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	1, 1,
	{0,   40,  80,  120, 160, 200, 240, 280, 320, 360, 0, 0, 0, 0},
	{0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0, 0, 0, 0},
	{120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 0, 0, 0, 0},
	{
		256,   0,   0,   0,
		  0, 256,   0,   0,
		  0,   0, 256,   0,
	},
};

const IcrViewModeParam icr_vm_soft = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	1, 1,
	{0,   40,  80,  120, 160, 200, 240, 280, 320, 360, 0, 0, 0, 0},
	{0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0, 0, 0, 0},
	{30,  30,  30,  30,  30,  30,  30,  30,  30,  30, 0, 0, 0, 0},
	{
		256,   0,   0,   0,
		  0, 256,   0,   0,
		  0,   0, 256,   0,
	},
};
/*
const IcrViewModeParam icr_vm_landscape = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	0, 1,
	{0,   40,  80,  120, 160, 200, 240, 280, 320, 360, 0, 0, 0, 0},
	{0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 20,   0,0x0f,0x0f, 0, 0, 0, 0},
	{120,100,  128, 100, 100, 100, 128, 128, 100, 100, 0, 0, 0, 0},
};*/

const IcrViewModeParam icr_vm_landscape = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	0, 1,
	{0,  160,  170, 250, 280, 300, 310, 330, 340, 360, 0, 0, 0, 0},
	{15,  15,   15,   0,  15,  15,  25,   5,  15,  15, 0, 0, 0, 0},
	{64 , 64,  100, 100,  64,  64,  80,  80,  64,  64, 0, 0, 0, 0},
	{
		256,   0,   0,   0,
		  0, 256,   0,   0,
		  0,   0, 256,   0,
	},
};

const IcrViewModeParam icr_vm_portrait = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	1, 1,
	{0,  160,  170, 250, 280, 300, 310, 330, 340, 360, 0, 0, 0, 0},
	{15,  15,   15,   0,  15,  15,  25,   5,  15,  15, 0, 0, 0, 0},
	{64 , 64,  100, 100,  64,  64,  80,  80,  64,  64, 0, 0, 0, 0},
	{
		256,   0,   0,   0,
		  0, 256,   0,   0,
		  0,   0, 256,   0,
	},
};

#define TONE_MATRIX(r, g, b, percent)\
{\
	(r*256/(r+2*g+b) * percent + 256 * (100-percent)) / 100, r*512/(r+2*g+b), r*256/(r+2*g+b), 0,\
	g*256/(r+2*g+b), (g*512/(r+2*g+b) * percent + 256 * (100-percent)) / 100, g*256/(r+2*g+b), 0,\
	b*256/(r+2*g+b), b*512/(r+2*g+b), (b*256/(r+2*g+b) * percent + 256 * (100-percent)) / 100, 0,\
}

const IcrViewModeParam icr_vm_sepia = {
	CONTRAST_NORMAL+50, BRIGHTNESS_NORMAL+20, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	0, 1,
	{   0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 0, 0, 0, 0},
	{0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0, 0, 0, 0},
	{  64,  64,  64,  64,  64,  64,  64,  64,  64,  64, 0, 0, 0, 0},
	TONE_MATRIX( 162, 138, 101, 100 ),
};

const IcrViewModeParam icr_vm_platinum = {
	CONTRAST_NORMAL, BRIGHTNESS_NORMAL, SATURATION_NORMAL, HUE_NORMAL, TONE_NORMAL,
	0, ICR_ACE_OFF,
	0, 1,
	{   0,  40,  80, 120, 160, 200, 240, 280, 320, 360, 0, 0, 0, 0},
	{0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0, 0, 0, 0},
	{  64,  64,  64,  64,  64,  64,  64,  64,  64,  64, 0, 0, 0, 0},
	TONE_MATRIX( 100, 100, 111, 100 ),
};

IcrError SetupIcrModes(IcrSession* pSession)
{
	IcrError err;

	if ( !pSession )
		return ICR_ERR_INVALID_PARAM;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_VIVID, &icr_vm_vivid);
	if (err != ICR_ERR_OK)
		return err;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_SOFT, &icr_vm_soft);
	if (err != ICR_ERR_OK)
		return err;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_LANDSCAPE, &icr_vm_landscape);
	if (err != ICR_ERR_OK)
		return err;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_PORTRAIT, &icr_vm_portrait);
	if (err != ICR_ERR_OK)
		return err;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_SEPIA, &icr_vm_sepia);
	if (err != ICR_ERR_OK)
		return err;

	err = IcrViewModeRegister(pSession, ICR_VIEWMODE_PLATINUM, &icr_vm_platinum);
	if (err != ICR_ERR_OK)
		return err;

	return err;
}
