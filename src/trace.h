#ifndef KERNEL_TRACE_H
#define KERNEL_TRACE_H

#ifdef TRACE_ENABLED
    #define trace(...) printf(__VA_ARGS__)
#else
    #define trace(...)
#endif

#endif // KERNEL_TRACE_H
