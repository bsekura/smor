#include "types.h"
#include "stdio.h"
#include "string.h"
#include "console.h"

static int skip_atoi(const char** s)
{
    int i = 0;
    while (is_digit(**s))
        i = i*10 + *((*s)++) - '0';

    return i;
}

#define ZEROPAD     1   /* pad with zero */
#define SIGN        2   /* unsigned/signed long */
#define PLUS        4   /* show plus */
#define SPACE       8   /* space if plus */
#define LEFT        16  /* left justified */
#define SPECIAL     32  /* 0x */
#define SMALL       64  /* use 'abcdef' instead of 'ABCDEF' */

#define do_div(n,base) ({ \
uint64_t __res; \
__asm__("divq %4":"=a" (n),"=d" (__res):"0" (n),"1" (0),"r" (base)); \
__res; })

static char* number(char* str, int64_t num, int64_t base, int size, int precision, int type)
{
    char tmp[36];

    const char *digits="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (type & SMALL)
        digits="0123456789abcdefghijklmnopqrstuvwxyz";

    if (type & LEFT)
        type &= ~ZEROPAD;

    if (base < 2 || base > 36)
      return NULL;

    char sign;
    if (type & SIGN && num < 0) {
        sign = '-';
        num = -num;
    } else
        sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);

    if (sign) 
        --size;

    if (type & SPECIAL) {
        if (base == 16) 
            size -= 2;
        else if (base == 8) 
            --size;
    }

    int i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else while (num != 0)
        tmp[i++] = digits[do_div(num,base)];

    if (i > precision) 
        precision = i;

    size -= precision;
    if (!(type & (ZEROPAD+LEFT))) {
        while (size-- > 0)
            *str++ = ' ';
    }

    if (sign)
        *str++ = sign;

    if (type & SPECIAL) {
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];
        }
    }

    if (!(type & LEFT)) {
        char c = (type & ZEROPAD) ? '0' : ' ' ;
        while (size-- > 0)
            *str++ = c;
    }

    while (i < precision--)
        *str++ = '0';
    while (i-- > 0)
        *str++ = tmp[i];
    while (size-- > 0)
        *str++ = ' ';

    return str;
}

static inline int64_t get_qualified_integer(va_list args, int qualifier)
{
    return (qualifier == -1 || qualifier == 'h') 
        ? va_arg(args, int32_t) 
        : va_arg(args, int64_t);
}

int vsprintf(char* buf, const char* fmt, va_list args)
{
    int len;
    char* str;
    char* s;
    int* ip;

    for (str = buf; *fmt; ++fmt) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        /* 
        * process flags 
        */
        int flags = 0;
        repeat:
            ++fmt;      /* this also skips first '%' */
            switch (*fmt) {
                case '-': flags |= LEFT; goto repeat;
                case '+': flags |= PLUS; goto repeat;
                case ' ': flags |= SPACE; goto repeat;
                case '#': flags |= SPECIAL; goto repeat;
                case '0': flags |= ZEROPAD; goto repeat;
            }

        /* 
        * get field width 
        */
        int field_width = -1;
        if (is_digit(*fmt)) {
            field_width = skip_atoi(&fmt);
        } else if (*fmt == '*') {
            /* 
            * it's the next argument 
            */
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* 
        * get the precision 
        */
        int precision = -1;
        if (*fmt == '.') {
            ++fmt;   
            if (is_digit(*fmt)) {
                precision = skip_atoi(&fmt);
            } else if (*fmt == '*') {
                /* 
                 * it's the next argument 
                 */
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* 
        * get the conversion qualifier 
        */
        int qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT)) {
                while (--field_width > 0)
                    *str++ = ' ';
            }
            *str++ = (unsigned char) va_arg(args, int);
            while (--field_width > 0)
                *str++ = ' ';
            break;

        case 's':
            s = va_arg(args, char*);
            if (!s)
                s = "<NULL>";
            len = strlen(s);
            if (precision < 0)
                precision = len;
            else if (len > precision)
                len = precision;

            if (!(flags & LEFT)) {
                while (len < field_width--)
                    *str++ = ' ';
            }
            for (int i = 0; i < len; ++i)
                *str++ = *s++;
            while (len < field_width--)
                *str++ = ' ';
            break;

        case 'o':
            str = number(str, get_qualified_integer(args, qualifier), 8, field_width, precision, flags);
            break;

        case 'p':
            if (field_width == -1) {
                field_width = 16;
                flags |= ZEROPAD;
            }
            str = number(str, (uint64_t) va_arg(args, void*), 16, field_width, precision, flags);
            break;

        case 'x':
            flags |= SMALL;
        case 'X':
            str = number(str, get_qualified_integer(args, qualifier), 16, field_width, precision, flags);
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            str = number(str, get_qualified_integer(args, qualifier), 10, field_width, precision, flags);
            break;

        case 'n':
            ip = va_arg(args, int*);
            *ip = (str - buf);
            break;

        default:
            if (*fmt != '%')
                *str++ = '%';
            if (*fmt)
                *str++ = *fmt;
            else
                --fmt;
            break;
        }
    }
    *str = '\0';
    return str-buf;
}

int printf(const char* fmt, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);

    console_putstr(buf);
    
    return i;
}

