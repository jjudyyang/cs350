#ifndef __STDARG_H_
#define	__STDARG_H_

#ifndef _VA_LIST_DECLARED
#define	_VA_LIST_DECLARED
typedef	__builtin_va_list va_list;
#endif

#define	va_start(ap, last) \
	__builtin_va_start((ap), (last))

#define	va_arg(ap, type) \
	__builtin_va_arg((ap), type)

#define	__va_copy(dest, src) \
	__builtin_va_copy((dest), (src))

#define	va_copy(dest, src) \
	__va_copy(dest, src)

#define	va_end(ap) \
	__builtin_va_end(ap)

#endif /* __STDARG_H_ */
