/*-----------------------------------------------------------------------------
 * Copyright 2009 Marvell International Ltd.
 * All rights reserved
 *-----------------------------------------------------------------------------
 */

#ifndef _ICR_VIEWMODES_H_
#define _ICR_VIEWMODES_H_


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#define ICR_VIEWMODE_VIVID		2
#define ICR_VIEWMODE_SOFT		3
#define ICR_VIEWMODE_LANDSCAPE	4
#define ICR_VIEWMODE_PORTRAIT	5
#define ICR_VIEWMODE_SEPIA		6
#define ICR_VIEWMODE_PLATINUM	7
#define ICR_VIEWMODE_SKY		8
#define ICR_VIEWMODE_SKYGRASS	9


extern IcrError SetupIcrModes(IcrSession* pSession);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _ICR_VIEWMODES_H_

