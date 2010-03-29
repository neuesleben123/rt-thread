/*
 * File      : kservice.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2009, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-03-16     Bernard      the first version
 * 2006-05-25     Bernard      rewrite vsprintf
 * 2006-08-10     Bernard      add rt_show_version
 * 2010-03-17     Bernard      remove rt_strlcpy function
 *                             fix gcc compiling issue.
 */

#include <rtthread.h>
#include <rthw.h>

/**
 * @addtogroup KernelService
 */
/*@{*/

#ifndef RT_USING_NEWLIB
/* global errno in RT-Thread*/
int errno;
#else
#include <errno.h>
#endif
static rt_device_t _console_device = RT_NULL;

/*
 * This function will get errno
 *
 * @return errno
 */
rt_err_t rt_get_errno(void)
{
	rt_thread_t tid;

	tid = rt_thread_self();
	if (tid == RT_NULL) return errno;

	return tid->error;
}

/*
 * This function will set errno
 *
 * @param error the errno shall be set
 */
void rt_set_errno(rt_err_t error)
{
	rt_thread_t tid;

	tid = rt_thread_self();
	if (tid == RT_NULL) { errno = error; return; }

	tid->error = error;
}

/**
 * This function will set the content of memory to specified value
 *
 * @param s the address of source memory
 * @param c the value shall be set in content
 * @param count the copied length
 *
 * @return the address of source memory
 *
 */
void *rt_memset(void * s, int c, rt_ubase_t count)
{
#ifdef RT_TINY_SIZE
	char *xs = (char *) s;

	while (count--)
		*xs++ = c;

	return s;
#else
#define LBLOCKSIZE 		(sizeof(rt_int32_t))
#define UNALIGNED(X)   	((rt_int32_t)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN) 	((LEN) < LBLOCKSIZE)

	int i;
	char *m = (char *)s;
	rt_uint32_t buffer;
	rt_uint32_t *aligned_addr;
	rt_uint32_t d = c & 0xff;

	if (!TOO_SMALL (count) && !UNALIGNED (s))
	{
		/* If we get this far, we know that n is large and m is word-aligned. */
		aligned_addr = (rt_uint32_t*)s;

		/* Store D into each char sized location in BUFFER so that
		 * we can set large blocks quickly.
		 */
		if (LBLOCKSIZE == 4)
		{
			buffer = (d << 8) | d;
			buffer |= (buffer << 16);
		}
		else
		{
			buffer = 0;
			for (i = 0; i < LBLOCKSIZE; i++)
				buffer = (buffer << 8) | d;
		}

		while (count >= LBLOCKSIZE*4)
		{
			*aligned_addr++ = buffer;
			*aligned_addr++ = buffer;
			*aligned_addr++ = buffer;
			*aligned_addr++ = buffer;
			count -= 4*LBLOCKSIZE;
		}

		while (count >= LBLOCKSIZE)
		{
			*aligned_addr++ = buffer;
			count -= LBLOCKSIZE;
		}

		/* Pick up the remainder with a bytewise loop.  */
		m = (char*)aligned_addr;
	}

	while (count--)
	{
		*m++ = (char)d;
	}

	return s;

#undef LBLOCKSIZE
#undef UNALIGNED
#undef TOO_SMALL
#endif
}

/**
 * This function will copy memory content from source address to destination
 * address.
 *
 * @param dst the address of destination memory
 * @param src  the address of source memory
 * @param count the copied length
 *
 * @return the address of destination memory
 *
 */
void *rt_memcpy(void * dst, const void *src, rt_ubase_t count)
{
#ifdef RT_TINY_SIZE
	char *tmp = (char *) dst, *s = (char *) src;

	while (count--)
		*tmp++ = *s++;

	return dst;
#else

#define UNALIGNED(X, Y) \
	(((rt_int32_t)X & (sizeof (rt_int32_t) - 1)) | ((rt_int32_t)Y & (sizeof (rt_int32_t) - 1)))
#define BIGBLOCKSIZE    (sizeof (rt_int32_t) << 2)
#define LITTLEBLOCKSIZE (sizeof (rt_int32_t))
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)

	char *dst_ptr = (char*)dst;
	char *src_ptr = (char*)src;
	rt_int32_t *aligned_dst;
	rt_int32_t *aligned_src;
	int len = count;

	/* If the size is small, or either SRC or DST is unaligned,
	then punt into the byte copy loop.  This should be rare.  */
	if (!TOO_SMALL(len) && !UNALIGNED (src_ptr, dst_ptr))
	{
		aligned_dst = (rt_int32_t*)dst_ptr;
		aligned_src = (rt_int32_t*)src_ptr;

		/* Copy 4X long words at a time if possible.  */
		while (len >= BIGBLOCKSIZE)
		{
			*aligned_dst++ = *aligned_src++;
			*aligned_dst++ = *aligned_src++;
			*aligned_dst++ = *aligned_src++;
			*aligned_dst++ = *aligned_src++;
			len -= BIGBLOCKSIZE;
		}

		/* Copy one long word at a time if possible.  */
		while (len >= LITTLEBLOCKSIZE)
		{
			*aligned_dst++ = *aligned_src++;
			len -= LITTLEBLOCKSIZE;
		}

		/* Pick up any residual with a byte copier.  */
		dst_ptr = (char*)aligned_dst;
		src_ptr = (char*)aligned_src;
	}

	while (len--)
		*dst_ptr++ = *src_ptr++;

	return dst;
#undef UNALIGNED
#undef BIGBLOCKSIZE
#undef LITTLEBLOCKSIZE
#undef TOO_SMALL
#endif
}

/**
 * This function will move memory content from source address to destination
 * address.
 *
 * @param dest the address of destination memory
 * @param src  the address of source memory
 * @param n the copied length
 *
 * @return the address of destination memory
 *
 */
void* rt_memmove(void *dest, const void *src, rt_ubase_t n)
{
	char *tmp = (char *) dest, *s = (char *) src;

	if (s < tmp && tmp < s + n)
	{
		tmp+=n;
		s+=n;

		while (n--)
			*tmp-- = *s--;
	}
	else
	{
		while (n--)
			*tmp++ = *s++;
	}

	return dest;
}

/**
 * memcmp - Compare two areas of memory
 * @param cs: One area of memory
 * @param ct: Another area of memory
 * @param count: The size of the area.
 */
rt_int32_t rt_memcmp(const void * cs,const void * ct, rt_ubase_t count)
{
	const unsigned char *su1, *su2;
	int res = 0;

	for( su1 = cs, su2 = ct; 0 < count; ++su1, ++su2, count--)
		if ((res = *su1 - *su2) != 0)
			break;
	return res;
}

/**
 * This function will return the first occurrence of a string.
 *
 * @param s1 the source string
 * @param s2 the find string
 *
 * @return the first occurrence of a s2 in s1, or RT_NULL if no found.
 */
char * rt_strstr(const char * s1,const char * s2)
{
	int l1, l2;

	l2 = rt_strlen(s2);
	if (!l2)
		return (char *) s1;
	l1 = rt_strlen(s1);
	while (l1 >= l2)
	{
		l1--;
		if (!rt_memcmp(s1,s2,l2))
			return (char *) s1;
		s1++;
	}
	return RT_NULL;
}

/**
 * This function will compare two strings while ignoring differences in case
 *
 * @param a the string to be compared
 * @param b the string to be compared
 *
 * @return the result
 */
rt_uint32_t rt_strcasecmp(const char *a, const char *b)
{
	int ca, cb;

	do
	{
		ca = *a++ & 0xff;
		cb = *b++ & 0xff;
		if (ca >= 'A' && ca <= 'Z')
			ca += 'a' - 'A';
		if (cb >= 'A' && cb <= 'Z')
			cb += 'a' - 'A';
	}
	while (ca == cb && ca != '\0');

	return ca - cb;
}

/**
 * This function will copy string no more than n bytes.
 *
 * @param dest the string to copy
 * @param src the string to be copied
 * @param n the maximum copied length
 *
 * @return the result
 */
char *rt_strncpy(char *dest, const char *src, rt_ubase_t n)
{
	char *tmp = (char *) dest, *s = (char *) src;

	while(n--)
		*tmp++ = *s++;

	return dest;
}

/**
 * This function will compare two strings with specified maximum length
 *
 * @param cs the string to be compared
 * @param ct the string to be compared
 * @param count the maximum compare length
 *
 * @return the result
 */
rt_ubase_t rt_strncmp(const char * cs, const char * ct, rt_ubase_t count)
{
	register signed char __res = 0;

	while (count)
	{
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}

/**
 * This function will return the length of a string, which terminate will
 * null character.
 *
 * @param s the string
 *
 * @return the length of string
 */
rt_ubase_t rt_strlen(const char *s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc) /* nothing */
		;

	return sc - s;
}

#ifdef RT_USING_HEAP
/**
 * This function will duplicate a string.
 *
 * @param s the string to be duplicated
 *
 * @return the duplicated string pointer
 */
char *rt_strdup(const char *s)
{
	rt_size_t len = rt_strlen(s) + 1;
	char *tmp = (char *)rt_malloc(len);

	if(!tmp) return RT_NULL;

	rt_memcpy(tmp, s, len);
	return tmp;
}
#endif

/**
 * This function will show the version of rt-thread rtos
 */
void rt_show_version()
{
	rt_kprintf("\n \\ | /\n");
	rt_kprintf("- RT -     Thread Operating System\n");
	rt_kprintf(" / | \\ 0.%d.%d build %s\n", RT_VERSION, RT_SUBVERSION, __DATE__);
	rt_kprintf(" 2006 - 2009 Copyright by rt-thread team\n");
}

/* private function */
#define isdigit(c)  ((unsigned)((c) - '0') < 10)

rt_inline rt_int32_t divide(rt_int32_t *n, rt_int32_t base)
{
	rt_int32_t res;

	/* optimized for processor which does not support divide instructions. */
	if (base == 10)
	{
		res = ((rt_uint32_t)*n) % 10U;
		*n = ((rt_uint32_t)*n) / 10U;
	}
	else
	{
		res = ((rt_uint32_t)*n) % 16U;
		*n = ((rt_uint32_t)*n) / 16U;
	}

	return res;
}

rt_inline int skip_atoi(const char **s)
{
	register int i=0;
	while (isdigit(**s)) i = i*10 + *((*s)++) - '0';

	return i;
}

#define ZEROPAD 	(1 << 0)	/* pad with zero */
#define SIGN 		(1 << 1)	/* unsigned/signed long */
#define PLUS 		(1 << 2)	/* show plus */
#define SPACE 		(1 << 3)	/* space if plus */
#define LEFT 		(1 << 4)	/* left justified */
#define SPECIAL 	(1 << 5)	/* 0x */
#define LARGE		(1 << 6)	/* use 'ABCDEF' instead of 'abcdef' */

#ifdef RT_PRINTF_PRECISION
static char *print_number(char * buf, char * end, long num, int base, int s, int precision, int type)
#else
static char *print_number(char * buf, char * end, long num, int base, int s, int type)
#endif
{
	char c, sign;
#ifdef RT_PRINTF_LONGLONG
	char tmp[32];
#else
	char tmp[16];
#endif
	const char *digits;
	static const char small_digits[] = "0123456789abcdef";
	static const char large_digits[] = "0123456789ABCDEF";
	register int i;
	register int size;

	size = s;

	digits = (type & LARGE) ? large_digits : small_digits;
	if (type & LEFT) type &= ~ZEROPAD;

	c = (type & ZEROPAD) ? '0' : ' ';

	/* get sign */
	sign = 0;
	if (type & SIGN)
	{
		if (num < 0)
		{
			sign = '-';
			num = -num;
		}
		else if (type & PLUS) sign = '+';
		else if (type & SPACE) sign = ' ';
	}

#ifdef RT_PRINTF_SPECIAL
	if (type & SPECIAL)
	{
		if (base == 16) size -= 2;
		else if (base == 8) size--;
	}
#endif

	i = 0;
	if (num == 0) tmp[i++]='0';
	else
	{
		while (num != 0) tmp[i++] = digits[divide(&num, base)];
	}

#ifdef RT_PRINTF_PRECISION
	if (i > precision) precision = i;
	size -= precision;
#else
	size -= i;
#endif

	if (!(type&(ZEROPAD | LEFT)))
	{
		while(size-->0)
		{
			if (buf <= end) *buf = ' ';
			++buf;
		}
	}

	if (sign)
	{
		if (buf <= end)
		{
			*buf = sign;
			--size;
		}
		++buf;
	}

#ifdef RT_PRINTF_SPECIAL
	if (type & SPECIAL)
	{
		if (base==8)
		{
			if (buf <= end) *buf = '0';
			++buf;
		}
		else if (base==16)
		{
			if (buf <= end) *buf = '0';
			++buf;
			if (buf <= end)
			{
				*buf = type & LARGE? 'X' : 'x';
			}
			++buf;
		}
	}
#endif

	/* no align to the left */
	if (!(type & LEFT))
	{
		while (size-- > 0)
		{
			if (buf <= end) *buf = c;
			++buf;
		}
	}

#ifdef RT_PRINTF_PRECISION
	while (i < precision--)
	{
		if (buf <= end) *buf = '0';
		++buf;
	}
#endif

	/* put number in the temporary buffer */
	while (i-- > 0)
	{
		if (buf <= end) *buf = tmp[i];
		++buf;
	}

	while (size-- > 0)
	{
		if (buf <= end) *buf = ' ';
		++buf;
	}

	return buf;
}

static rt_int32_t vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args)
{
#ifdef RT_PRINTF_LONGLONG
	unsigned long long num;
#else
	rt_uint32_t num;
#endif
	int i, len;
	char *str, *end, c;
	const char *s;

	rt_uint8_t base;			/* the base of number */
	rt_uint8_t flags;			/* flags to print number */
	rt_uint8_t qualifier;		/* 'h', 'l', or 'L' for integer fields */
	rt_int32_t field_width;	/* width of output field */

#ifdef RT_PRINTF_PRECISION
	int precision;		/* min. # of digits for integers and max for a string */
#endif

	str = buf;
	end = buf + size - 1;

	/* Make sure end is always >= buf */
	if (end < buf)
	{
		end = ((char *)-1);
		size = end - buf;
	}

	for (; *fmt ; ++fmt)
	{
		if (*fmt != '%')
		{
			if (str <= end) *str = *fmt;
			++str;
			continue;
		}

		/* process flags */
		flags = 0;

		while(1)
		{
			/* skips the first '%' also */
			++fmt;
			if (*fmt == '-') flags |= LEFT;
			else if (*fmt == '+') flags |= PLUS;
			else if (*fmt == ' ') flags |= SPACE;
			else if (*fmt == '#') flags |= SPECIAL;
			else if (*fmt == '0') flags |= ZEROPAD;
			else break;
		}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt)) field_width = skip_atoi(&fmt);
		else if (*fmt == '*')
		{
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0)
			{
				field_width = -field_width;
				flags |= LEFT;
			}
		}

#ifdef RT_PRINTF_PRECISION
		/* get the precision */
		precision = -1;
		if (*fmt == '.')
		{
			++fmt;
			if (isdigit(*fmt)) precision = skip_atoi(&fmt);
			else if (*fmt == '*')
			{
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0) precision = 0;
		}
#endif
		/* get the conversion qualifier */
		qualifier = 0;
		if (*fmt == 'h' || *fmt == 'l'
#ifdef RT_PRINTF_LONGLONG
				|| *fmt == 'L'
#endif
		   )
		{
			qualifier = *fmt;
			++fmt;
#ifdef RT_PRINTF_LONGLONG
			if (qualifier == 'l' && *fmt == 'l')
			{
				qualifier = 'L';
				++fmt;
			}
#endif
		}

		/* the default base */
		base = 10;

		switch (*fmt)
		{
		case 'c':
			if (!(flags & LEFT))
			{
				while (--field_width > 0)
				{
					if (str <= end) *str = ' ';
					++str;
				}
			}

			/* get character */
			c = (rt_uint8_t) va_arg(args, int);
			if (str <= end) *str = c;
			++str;

			/* put width */
			while (--field_width > 0)
			{
				if (str <= end) *str = ' ';
				++str;
			}
			continue;

		case 's':
			s = va_arg(args, char *);
			if (!s) s = "(NULL)";

			len = rt_strlen(s);
#ifdef RT_PRINTF_PRECISION
			if (precision > 0 && len > precision) len = precision;
#endif

			if (!(flags & LEFT))
			{
				while (len < field_width--)
				{
					if (str <= end) *str = ' ';
					++str;
				}
			}

			for (i = 0; i < len; ++i)
			{
				if (str <= end) *str = *s;
				++str;
				++s;
			}

			while (len < field_width--)
			{
				if (str <= end) *str = ' ';
				++str;
			}
			continue;

		case 'p':
			if (field_width == -1)
			{
				field_width = sizeof(void *) << 1;
				flags |= ZEROPAD;
			}
#ifdef RT_PRINTF_PRECISION
			str = print_number(str, end,
							   (long) va_arg(args, void *),
							   16, field_width, precision, flags);
#else
			str = print_number(str, end,
							   (long) va_arg(args, void *),
							   16, field_width, flags);
#endif
			continue;

		case '%':
			if (str <= end) *str = '%';
			++str;
			continue;

			/* integer number formats - set up the flags and "break" */
		case 'o':
			base = 8;
			break;

		case 'X':
			flags |= LARGE;
		case 'x':
			base = 16;
			break;

		case 'd':
		case 'i':
			flags |= SIGN;
		case 'u':
			break;

		default:
			if (str <= end) *str = '%';
			++str;

			if (*fmt)
			{
				if (str <= end) *str = *fmt;
				++str;
			}
			else
			{
				--fmt;
			}
			continue;
		}

#ifdef RT_PRINTF_LONGLONG
		if (qualifier == 'L') num = va_arg(args, long long);
		else if (qualifier == 'l')
#else
		if (qualifier == 'l')
#endif
		{
			num = va_arg(args, rt_uint32_t);
			if (flags & SIGN) num = (rt_int32_t) num;
		}
		else if (qualifier == 'h')
		{
			num = (rt_uint16_t) va_arg(args, rt_int32_t);
			if (flags & SIGN) num = (rt_int16_t) num;
		}
		else
		{
			num = va_arg(args, rt_uint32_t);
			if (flags & SIGN) num = (rt_int32_t) num;
		}
#ifdef RT_PRINTF_PRECISION
		str = print_number(str, end, num, base, field_width, precision, flags);
#else
		str = print_number(str, end, num, base, field_width, flags);
#endif
	}

	if (str <= end) *str = '\0';
	else *end = '\0';

	/* the trailing null byte doesn't count towards the total
	* ++str;
	*/
	return str-buf;
}

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param size the size of buffer
 * @param fmt the format
 */
rt_int32_t rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
	rt_int32_t n;
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buf, size, fmt, args);
	va_end(args);

	return n;
}

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param arg_ptr the arg_ptr
 * @param format the format
 */
rt_int32_t rt_vsprintf(char *buf, const char *format, va_list arg_ptr)
{
	return vsnprintf(buf, (rt_size_t) -1, format, arg_ptr);
}

/**
 * This function will fill a formatted string to buffer
 *
 * @param buf the buffer to save formatted string
 * @param format the format
 */
rt_int32_t rt_sprintf(char *buf ,const char *format,...)
{
	rt_int32_t n;
	va_list arg_ptr;

	va_start(arg_ptr, format);
	n = rt_vsprintf(buf ,format,arg_ptr);
	va_end (arg_ptr);

	return n;
}

/**
 * This function will set console to a device.
 * After set a device to console, all output of rt_kprintf will be
 * written to this device.
 *
 * @param device the new console device
 *
 * @return the old console device
 */
rt_device_t rt_console_set_device(const char* name)
{
	rt_device_t new, old;

	/* save old device */
	old = _console_device;

	/* find new console device */
	new = rt_device_find(name);
	if (new != RT_NULL)
	{
		if (_console_device != RT_NULL)
		{
			/* close old console device */
			rt_device_close(_console_device);
		}

		/* set new console device */
		_console_device = new;
		rt_device_open(_console_device, RT_DEVICE_OFLAG_RDWR);
	}

	return old;
}

#if defined(__GNUC__)
void rt_hw_console_output(const char* str) __attribute__((weak));


void rt_hw_console_output(const char* str)
#elif defined(__CC_ARM)
__weak void rt_hw_console_output(const char* str)
#elif defined(__ICCARM__)
__weak void rt_hw_console_output(const char* str)
#endif
{
    /* empty console output */
}

/**
 * This function will print a formatted string on system console
 *
 * @param fmt the format
 */
void rt_kprintf(const char *fmt, ...)
{
	va_list args;
	rt_size_t length;
	static char rt_log_buf[RT_CONSOLEBUF_SIZE];

	va_start(args, fmt);

	length = vsnprintf(rt_log_buf, sizeof(rt_log_buf), fmt, args);
	if (_console_device == RT_NULL)
	{
        rt_hw_console_output(rt_log_buf);
	}
	else
	{
		rt_device_write(_console_device, 0, rt_log_buf, length);
	}

	va_end(args);
}

#if !defined (RT_USING_NEWLIB) && defined (RT_USING_MINILIBC) && defined (__GNUC__)
#include <sys/types.h>
void* memcpy(void *dest, const void *src, size_t n) __attribute__((weak, alias("rt_memcpy")));
void* memset(void *s, int c, size_t n) __attribute__((weak, alias("rt_memset")));
void* memmove(void *dest, const void *src, size_t n) __attribute__((weak, alias("rt_memmove")));
int   memcmp(const void *s1, const void *s2, size_t n) __attribute__((weak, alias("rt_memcmp")));

size_t strlen(const char *s) __attribute__((weak, alias("rt_strlen")));
char *strstr(const char *s1,const char *s2) __attribute__((weak, alias("rt_strstr")));
int strcasecmp(const char *a, const char *b) __attribute__((weak, alias("rt_strcasecmp")));
char *strncpy(char *dest, const char *src, size_t n) __attribute__((weak, alias("rt_strncpy")));
int strncmp(const char *cs, const char *ct, size_t count) __attribute__((weak, alias("rt_strncmp")));
#ifdef RT_USING_HEAP
char *strdup(const char *s) __attribute__((weak, alias("rt_strdup")));
#endif
#endif

/*@}*/
