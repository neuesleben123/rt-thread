/*
 * File      : rtgui_config.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-10-16     Bernard      first version
 * 2010-02-08     Bernard      move some RTGUI options to bsp
 */

#ifndef __RTGUI_CONFIG_H__
#define __RTGUI_CONFIG_H__

/* RTGUI options */

#ifdef _WIN32
#define RTGUI_USING_STDIO_FILERW
#define RTGUI_IMAGE_PNG
#define RTGUI_IMAGE_JPEG
#define RTGUI_USING_FONT12
#define RTGUI_USING_HZ_BMP
#define RTGUI_MEM_TRACE
#endif

#define RTGUI_SVR_THREAD_PRIORITY		15
#define RTGUI_SVR_THREAD_TIMESLICE		5
#ifdef RTGUI_USING_SMALL_SIZE
#define RTGUI_SVR_THREAD_STACK_SIZE		1024
#else
#define RTGUI_SVR_THREAD_STACK_SIZE		2048
#endif

#define RTGUI_APP_THREAD_PRIORITY		25
#define RTGUI_APP_THREAD_TIMESLICE		5
#ifdef RTGUI_USING_SMALL_SIZE
#define RTGUI_APP_THREAD_STACK_SIZE		1024
#else
#define RTGUI_APP_THREAD_STACK_SIZE		2048
#endif

#endif
